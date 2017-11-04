
#include "syscall.h"
#include "task.h"
#include "types.h"
#include "lib.h"
#include "filesystem.h"
#include "keyboard.h"
#include "terminal.h"
#include "rtc.h"
#include "paging.h"
#include "x86_desc.h"
#include "interrupt.h"


/* file system information - size, number of file, etc - bootblock info */
extern filesystem_info_t filesystem_info;
/* e use a NULL for the first item in the jump table as syscalls 
start numbering at 1 so we can avoid subtracting by 1 in the syscall handler*/
syscall_func_t syscall_func[NUM_SYSCALLS + 
    SYSCALL_NUM_NULL_ENTRY_OFFSET] = {
    (syscall_func_t) NULL, /* don't remove this */
    (syscall_func_t) do_halt,
    (syscall_func_t) do_execute,
    (syscall_func_t) do_read,
    (syscall_func_t) do_write,
    (syscall_func_t) do_open,
    (syscall_func_t) do_close,
    (syscall_func_t) do_getargs,
    (syscall_func_t) do_vidmap,
    (syscall_func_t) do_set_handler,
    (syscall_func_t) do_sigreturn,
    (syscall_func_t) do_malloc,
    (syscall_func_t) do_free,
    (syscall_func_t) do_touch
};

/* stdin fops table */
static operations_t stdin_operations= {
    terminal_open,
    terminal_read,
    NULL,
    terminal_close
};

/* stdout fops table */
static operations_t stdout_operations= {
    terminal_open,
    NULL,
    terminal_write,
    terminal_close
};

/* default fops table */
static operations_t fds_operations= {
    NULL,
    NULL,
    NULL,
    NULL
};

/* default signal handlers
 */

static void div_zero_default(int32_t unused) {
    printf("divide by zero\n");
    do_halt_exception();
}

static void segfault_default(int32_t unused) {
    printf("segfault\n");
    do_halt_exception();
}

static void interrupt_default(int32_t unused) {
    do_halt(0);
}


void* signal_handler_default[NUM_SIGNAL] = {
    div_zero_default,
    segfault_default,
    interrupt_default,
    NULL,
    NULL
};


/*
 * init_syscall_by_idx
 *   DESCRIPTION: initialize the syscall - specifically the file descriptor
 *                table for a specific file descriptor
 *   INPUTS: idx: file descriptor entry to initalize
 *   OUTPUTS: none
 *   RETURN VALUE: 0, always succeeds
 *   SIDE EFFECTS: current_pcb[active_terminal_idx]->file_descriptors array written to.
 */
static void init_syscall_by_idx(uint32_t idx) {
    int32_t i;
    // Initialize file descriptors
    for (i = 0; i < FILE_ARRAY_LENGTH; ++i) {
        /* fd 0 is always stdin/keyboard */
        if (i == FD_STDIN) {
            pcb_array[idx]->file_descriptors[i].operations = stdin_operations;
            pcb_array[idx]->file_descriptors[i].inode = NULL;
            pcb_array[idx]->file_descriptors[i].pos = 0;
            pcb_array[idx]->file_descriptors[i].flags = FLAG_IN_USE;
        } /* fd 1 is always stdout/screen */
        else if (i == FD_STDOUT) {
            pcb_array[idx]->file_descriptors[i].operations = stdout_operations;
            pcb_array[idx]->file_descriptors[i].inode = NULL;
            pcb_array[idx]->file_descriptors[i].pos = 0;
            pcb_array[idx]->file_descriptors[i].flags = FLAG_IN_USE;
        }
        else {
            /* default other fds to NULLs */
            pcb_array[idx]->file_descriptors[i].operations = fds_operations;
            pcb_array[idx]->file_descriptors[i].inode = NULL;
            pcb_array[idx]->file_descriptors[i].pos = 0;
            pcb_array[idx]->file_descriptors[i].flags = CLEAR_ALL_FLAGS;
        }
    }
}

/*
 * init_syscall
 *   DESCRIPTION: initialize the syscall - specifically the file descriptor
 *                table. entry 0 and 1 are prepopulated with the keyboard and
 *                screen respectively. Other entries are set to default values.
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: 0, always succeeds
 *   SIDE EFFECTS: current_pcb[active_terminal_idx]->file_descriptors array written to.
 */
int32_t init_syscall() {
    int32_t i;
    for (i=0; i<MAX_PCB; i++)
        init_syscall_by_idx(i);
    return SUCCESS;
}

/*
 * do_halt
 *   DESCRIPTION: halts the current program with a return value of status 
 *                passed in BL, then expanded to 32bits and passed back to the 
 *                original exec function, top bits are zeroed out
 *   INPUTS: status - return code of program - see program that was run. 256 if
 *           program halted because of an exception - see do_halt_exception
 *   OUTPUTS: none
 *   RETURN VALUE: should not return anything to caller, returns to program that
 *                 called exec - see exec's return codes
 *   SIDE EFFECTS: none
 */
int32_t do_halt (uint8_t status) {
    /* no process is running, should not occur */
    if(!current_pcb[active_task_idx]) {
        return ERR;
    }
    /* disable interrupt,especially from pit
     * | | | |
     * | | | |
     * | | | |
     * | | | |
     * . . . .
     * no sti
     */
    cli();

    int32_t i;
    for(i=FD_STDOUT; i<=FILE_ARRAY_LENGTH; ++i) {
        do_close(i);
    }   //close all files other than the preopened ones

    /* save the parent's pid and kmode stack pointer since we will change the 
    PCB */
    uint32_t parent_pid=current_pcb[active_task_idx]->parent_pid;
    uint32_t parent_esp=current_pcb[active_task_idx]->parent_esp;
    uint32_t parent_ebp=current_pcb[active_task_idx]->parent_ebp;

    /* kill the current task */
    current_pcb[active_task_idx]->flag=TASK_NOT_PRESENT;

    /* restart the task if the inital shell was killed, which is PID 0 */
    if (current_pcb[active_task_idx]->pid == TERM_0 || 
        current_pcb[active_task_idx]->pid == TERM_1 || 
        current_pcb[active_task_idx]->pid == TERM_2)
        execute((const uint8_t*)"shell");

    /* restore the parent's stack pointer and PCB */
    current_pcb[active_task_idx]=pcb_array[parent_pid];
    tss.ss0 = KERNEL_DS;
    tss.esp0 = parent_esp;

    //update paging to the parent's
    update_page_directory(current_pcb[active_task_idx]->pid + PAGE_DIR_USER_IDX_OFFSET);

    /* jump back to the exec function call of the parent, copy the return value 
    to eax and zero out upper 24 bits */
    asm volatile(
        "movl %0, %%esp;"
        "movl %1, %%ebp;"
        "movzbl %2, %%eax;"
        "jmp __halt_return_point;"
        :
        :"r"(parent_esp), "r"(parent_ebp),"r"(status)
        :"cc");

/* should not reach here */
    return ERR;
}

/*
 * do_halt_exception
 *   DESCRIPTION: halts the current program with a return value 256 if program
 *                caused a exception
 *   INPUTS: status - return code of program - see program that was run. 256 if
 *           program halted because of an exception - see do_halt_exception
 *   OUTPUTS: none
 *   RETURN VALUE: should not return anything to caller, returns to program that
 *                 called exec - see exec's return codes
 *   SIDE EFFECTS: none
 */
int32_t do_halt_exception () {
    /* see exec, function desciption */
    int32_t status = EXCEPTION_RETVAL;
    /* no process is running, should not occur */
    if(!current_pcb[active_task_idx]) {
        return ERR;
    }
    /* disable interrupt,especially from pit
     * | | | |
     * | | | |
     * | | | |
     * | | | |
     * . . . .
     * no sti
     */
    cli();

    int32_t i;
    for(i=FD_STDOUT; i<=FILE_ARRAY_LENGTH; ++i) {
        do_close(i);
    }   //close all files other than the preopened ones

    /* save the parent's pid and kmode stack pointer since we will change the 
    PCB */
    uint32_t parent_pid=current_pcb[active_task_idx]->parent_pid;
    uint32_t parent_esp=current_pcb[active_task_idx]->parent_esp;
    uint32_t parent_ebp=current_pcb[active_task_idx]->parent_ebp;

    /* kill the current task */
    current_pcb[active_task_idx]->flag=TASK_NOT_PRESENT;

    /* restart the task if the inital shell was killed, which is PID 0 */
    if (current_pcb[active_task_idx]->pid == 0)
        execute((const uint8_t*)"shell");

    /* restore the parent's stack pointer and PCB */
    current_pcb[active_task_idx]=pcb_array[parent_pid];
    tss.ss0 = KERNEL_DS;
    tss.esp0 = parent_esp;

    //update paging to the parent's
    update_page_directory(current_pcb[active_task_idx]->pid + PAGE_DIR_USER_IDX_OFFSET);

    /* jump back to the exec function call of the parent, copy the return value 
    to eax and zero out upper 16 bits */
    asm volatile(
        "movl %0, %%esp;"
        "movl %1, %%ebp;"
        "movl %2, %%eax;"
        "jmp __halt_return_point;"
        :
        :"r"(parent_esp), "r"(parent_ebp), "r"(status)
        :"cc");

/* should not reach here */
    return ERR;
}

/*
 * do_execute
 *   DESCRIPTION: executes the desired command, blocks parent process until 
 *                completion
 *   INPUTS: command - filename of "ELF" style binary to be execute
 *           and associated arguments to that program
 *   OUTPUTS: none
 *   RETURN VALUE: -1 for fail, 256 if program died by exception, or 0 to 255 
 *                 if program exited via the halt syscall.
 *   SIDE EFFECTS: none
 */
int32_t do_execute (const uint8_t* command) {
    if(!command) {
        return ERR;
    }

    uint8_t fname[LINE_BUFFER_LEN];
    uint8_t args[LINE_BUFFER_LEN];
    memset(args, 0x00, LINE_BUFFER_LEN);
    int32_t i = 0;
    /* tokenize the command string by spaces as delimeters*/
    /* get the first token of the command, this is the program name */
    while((command[i]!= ' ')&&(command[i] != '\0')&&(command[i] != '\n')) {
        fname[i]=command[i];
        i++;
    }
    fname[i]='\0';
    // printf("%s\n", fname);

    /* get the arguments, we strip the prog name and the first null and return 
    as one unit to the program's PCB, program should use getargs syscall */
    if(command[i]!='\0') {
        i++;   //skip the space between prog name and commands
        int32_t j=0;
        while((command[i+j]!='\0')&&(command[i+j]!='\n')) {
            args[j]=command[i+j];
            j++;
        }
        args[j]='\0';
        // printf("%s\n", args);
    } else {
        args[0]='\0'; //no args, so return a null string
    }   //args got

    //command parsed
    //only the current active terminal can launch a new user program
    active_task_idx = active_terminal_idx;
    
    int32_t fd;
    fd = do_open(fname);

    // program does not exist
    if(fd==ERR) {
        return ERR;
    }   //fd got

    uint8_t exec_header[EXEC_HEADER_LEN];
    int32_t res=do_read(fd, exec_header, EXEC_HEADER_LEN);
    if(res==ERR) {
        do_close(fd);
        return ERR;  //the executable's header can not be read
    }
    //printf("%s\n", exec_header);

    // match the magic numbers in the executable header
    if(exec_header[MAGIC_NUM_0_IDX]!=MAGIC_NUM_0) {
        do_close(fd);
        return ERR;
    }
    if(exec_header[MAGIC_NUM_1_IDX]!=MAGIC_NUM_1) {
        do_close(fd);
        return ERR;
    }
    if(exec_header[MAGIC_NUM_2_IDX]!=MAGIC_NUM_2) {
        do_close(fd);
        return ERR;
    }
    if(exec_header[MAGIC_NUM_3_IDX]!=MAGIC_NUM_3) {
        do_close(fd);
        return ERR;
    }   //check if exec
    // printf("executable!\n");

    //!!!! need to reset file pos
    cli();

    uint32_t entry_point=0;
    for(i=0; i<PTR_SIZE; ++i) {
        entry_point=entry_point+(exec_header[ENTRY_POINT_START+i]<<(i)*
            ENTRY_POINT_OFFSET);
    }   //little endian???
    // printf("%d\n", entry_point);

    //set up a new PCB
    pcb_t* new_pcb_ptr=NULL;
    pcb_t* old_pcb_ptr=current_pcb[active_task_idx];

    /* find a slot in the PCB array, if more than 6 processe running, prevent 
    more processes from existing */
    for(i=0; i<MAX_PCB; ++i) {
        if(pcb_array[i]->flag==TASK_NOT_PRESENT && (i>TERM_2 || active_task_idx==i)) {
            new_pcb_ptr=pcb_array[i];
            break;
        }
    }

    if(!new_pcb_ptr) {
        do_close(fd);
        return ERR;
    }   //no more slots available

    //close fd from old pcb
    do_close(fd);

    //switch current pcb pointer to new pcb pointer, make it alive
    current_pcb[active_task_idx]=new_pcb_ptr;
    current_pcb[active_task_idx]->flag=TASK_ACTIVE;
    current_pcb[active_task_idx]->pid=i;
    current_pcb[active_task_idx]->parent_pid=old_pcb_ptr->pid;
    //inherit the current terminal
    current_pcb[active_task_idx]->terminal_idx=old_pcb_ptr->terminal_idx;
    //set default signal handlers
    current_pcb[active_task_idx]->signal_handler[DIV_ZERO] = signal_handler_default[DIV_ZERO];
    current_pcb[active_task_idx]->signal_handler[SEGFAULT] = signal_handler_default[SEGFAULT];
    current_pcb[active_task_idx]->signal_handler[INTERRUPT] = signal_handler_default[INTERRUPT];
    current_pcb[active_task_idx]->signal_handler[ALARM] = signal_handler_default[ALARM];
    current_pcb[active_task_idx]->signal_handler[USER1] = signal_handler_default[USER1];

    //unmask signals in pcb, important, especially when the previous program terminates by exception
    unmask_signals(active_task_idx);
    unpend_signals(active_task_idx);

    //copy the arguments to the new process
    memcpy(current_pcb[active_task_idx]->args, args, LINE_BUFFER_LEN*sizeof(uint8_t));

    // map 4MB page, flush TLB
    map_mega_page(KERNEL_START, KERNEL_START, current_pcb[active_task_idx]->pid + 
        PAGE_DIR_USER_IDX_OFFSET, KERNEL_DPL);

    map_kilo_page(VIDEO_MEM_START, VIDEO_MEM_START, current_pcb[active_task_idx]->pid + 
        PAGE_DIR_USER_IDX_OFFSET, KERNEL_DPL);
    //map al three video memory buffer, each buffer is assigned to a terminal
    map_kilo_page(VIDEO_BUFFER_BASE, VIDEO_BUFFER_BASE, 
        current_pcb[active_task_idx]->pid + PAGE_DIR_USER_IDX_OFFSET, KERNEL_DPL);
    //second video memory buffer
    map_kilo_page(VIDEO_BUFFER_BASE + VIDEO_MEM_SIZE, VIDEO_BUFFER_BASE + VIDEO_MEM_SIZE, 
        current_pcb[active_task_idx]->pid + PAGE_DIR_USER_IDX_OFFSET, KERNEL_DPL);
    //third video memory buffer
    map_kilo_page(VIDEO_BUFFER_BASE + 2 * VIDEO_MEM_SIZE, VIDEO_BUFFER_BASE + 2 * VIDEO_MEM_SIZE, 
        current_pcb[active_task_idx]->pid + PAGE_DIR_USER_IDX_OFFSET, KERNEL_DPL);
    //map user program image
    map_mega_page(USR_PRG_VIRTUAL_START, USR_PRG_PHY_BASE + (current_pcb[active_task_idx]->pid * 
        DIR_ADDRESSABLE), current_pcb[active_task_idx]->pid + PAGE_DIR_USER_IDX_OFFSET, USR_DPL);

    for(i = 0; i < NUM_SLABS; i++) {
        //need to be changed
        map_kilo_page(USR_PRG_VIRTUAL_END + (i+1) * VIDEO_MEM_SIZE, 32 * MEGA + (i + current_pcb[active_task_idx]->pid * NUM_SLABS) * VIDEO_MEM_SIZE, 
        current_pcb[active_task_idx]->pid + PAGE_DIR_USER_IDX_OFFSET, USR_DPL);
        current_pcb[active_task_idx]->slab_cache[i].bitmap = 0;
        current_pcb[active_task_idx]->slab_cache[i].cache_ptr = (void*) (USR_PRG_VIRTUAL_END + (i+1) * VIDEO_MEM_SIZE);//from map kilo page
    }

    update_page_directory(current_pcb[active_task_idx]->pid + PAGE_DIR_USER_IDX_OFFSET);

    /* copy program image to correct offset in 4MB page with virtual base 
    address of 128MB */
    void* program_image = (void*) PRG_OFFSET;

    //open fd in new pcb
    fd=do_open(fname);

    //read program image in new pcb
    if (do_read(fd, program_image, current_pcb[active_task_idx]->file_descriptors[fd].
        inode->length_in_byte)
        != current_pcb[active_task_idx]->file_descriptors[fd].inode->length_in_byte) {

        /* could not read the whole program image, restore the old 
        process's context */
        printf("read error\n");
        do_close(fd);
        current_pcb[active_task_idx]=old_pcb_ptr;
        update_page_directory(current_pcb[active_task_idx]->pid + PAGE_DIR_USER_IDX_OFFSET);
        return ERR;
    }

    do_close(fd);

    /* update the tss with the kmode stack in the new process, stack offset by 
    4 to avoid accessing next page's base addr */
    tss.ss0 = KERNEL_DS;
    tss.esp0 = KERNEL_END - (current_pcb[active_task_idx]->pid * 8 * KILO) - KMODE_STACK_OFFSET;
    //see task.c , each Kmode stack + PCB is 8KB large
    // printf("entering user mode\n");

    /* the following code is adapted from 
     * http://www.jamesmolloy.co.uk/tutorial_html/10.-User%20Mode.html
     * need double check
     */

    /* set up the stack for the fake iret. Make this atomic, we don't want other
    interrupts to mess up the stack */

    /* push the new kmode stack pointer */
    asm volatile(
        "movl %%ebp, %0;"
        "movl %%esp, %1;"
        :"=r"(current_pcb[active_task_idx]->parent_ebp), "=r"(current_pcb[active_task_idx]->parent_esp)
        :
        :"cc");

    asm volatile(
        "movw %w0, %%ax;"

        // set segment selectors to user data segment
        "movw %%ax, %%ds;"
        "movw %%ax, %%es;"
        "movw %%ax, %%fs;"
        "movw %%ax, %%gs;"

        // user data segment
        "pushl %1;"

        // stack pointer
        "pushl %2;"

        // push flags
        "pushfl;"

        // reenable interrupt by setting IF flag
        "popl %%eax;"
        "orl $0x200, %%eax;"
        "pushl %%eax;"

        // user code segment, change RPL to 3
        "pushl %3;"

        // EIP
        "pushl %4;"
        :
        :"r"(USER_DS), "r"(USER_DS), "r"(USR_PRG_VIRTUAL_END - 
            KMODE_STACK_OFFSET), 
            "r"(USER_CS), "r"(entry_point)
        :"cc", "eax");

    // user mode
    asm volatile(
        "iret;"
        :
        :
        :"cc");

    asm volatile("__halt_return_point:" ::);
    /* move the return value from halt into eax */
    int32_t retval;
    asm volatile(
        "movl %%eax, %0"
        :"=r"(retval)
        :
        :"cc");
    return retval;
}

/* do_read
 *   DESCRIPTION: read from the file
 *   INPUTS:
 *          fd: index in the file array
 *          buf: destination of the data
 *          nbytes: length in bytes to be read
 *   OUTPUTS: none
 *   RETURN VALUE: 0 for success, -1 for fail
 *   SIDE EFFECTS: depends on the device/file accessed - see specific functions
 */
int32_t do_read (int32_t fd, void* buf, int32_t nbytes) {
    /* validate arguments */
    if (fd < 0 || fd >= FD_MAX || fd == FD_STDOUT)
        return ERR;

    if(buf == NULL) {
        return ERR;
    }
    
    /* no process is running, should not occur */
    if(!current_pcb[active_task_idx]) {
        return ERR;
    }

    /* the fd was not yet opened, do not read it */
    if(!(current_pcb[active_task_idx]->file_descriptors[fd].flags & FLAG_IN_USE)) {
        return ERR;
    }

    return current_pcb[active_task_idx]->file_descriptors[fd].
        operations.read(fd, (void*)buf, nbytes);
}



 /* do_write
  *   DESCRIPTION: write to the file
  *   INPUTS:
  *          fd: index in the file array
  *          buf: source of the data
  *          nbytes: length in bytes to be write
  *   OUTPUTS: none
  *   RETURN VALUE: 0 for success, -1 for fail
  *   SIDE EFFECTS: depends on the device/file accessed - see specific functions
  */

int32_t do_write (int32_t fd, const void* buf, int32_t nbytes) {
    if (fd < 0 || fd >= FD_MAX || fd == FD_STDIN)
        return ERR;

    if(buf == NULL) {
        return ERR;
    }

    /* no process is running, should not occur */
    if(!current_pcb[active_task_idx]) {
        return ERR;
    }
    
    /* the fd was not yet opened, do not write it */
    if(!(current_pcb[active_task_idx]->file_descriptors[fd].flags & FLAG_IN_USE)) {
        return ERR;
    }

    return current_pcb[active_task_idx]->file_descriptors[fd].
        operations.write(fd, buf, nbytes);
}


/*
 * do_open
 *   DESCRIPTION: open the file with the fname, assign correct set of functions
 *   INPUTS:
 *          filename: filename to be opened
 *   OUTPUTS: none
 *   RETURN VALUE: file array idx for success, -1 for fail
 *   SIDE EFFECTS: file array updated
 */
int32_t do_open (const uint8_t* filename) {
    /* if the filename pointer is NULL, or if the first character is a NULL, 
    the filename is wrong */
    if(!filename||filename[0]=='\0') {
        return ERR;
    }
    return filesystem_open(filename);
}


/*
 * do_close
 *   DESCRIPTION: close the file
 *   INPUTS: fd: file descriptor index
 *   OUTPUTS: none
 *   RETURN VALUE: 0 for success, -1 for fail
 *   SIDE EFFECTS: changes flags field of associated fd in current process's PCB
 */
int32_t do_close (int32_t fd) {
    /* do not alllow stdin or stdout to be closed */
    if (fd <= FD_STDIO || fd >= FD_MAX)
        return ERR;

    /* no process is running, should not occur */
    if(!current_pcb[active_task_idx]) {
        return ERR;
    }

    /* the fd was not yet opened, do not close it */
    if(!(current_pcb[active_task_idx]->file_descriptors[fd].flags & FLAG_IN_USE)) {
        return ERR;
    }

    /* run filetype specific close function */
    if(current_pcb[active_task_idx]->file_descriptors[fd].
        operations.close(fd) == ERR)
        return ERR;

    current_pcb[active_task_idx]->file_descriptors[fd].flags &= ~(FLAG_IN_USE);
    return SUCCESS;
}


/*
 * do_getargs
 *   DESCRIPTION: get the arguments passed to the program - 
 *                tokenize the program by using spaces as delimter, 
 *                then strip out first token (is name of new program) and 
 *                copy rest to string to the args field of PCB
 *   INPUTS: buf: destination of the data
 8           nbytes: number of bytes to write
 *   OUTPUTS: none
 *   RETURN VALUE: 0 for success, -1 for fail, if the number of arguments is 
 *                 too long to fit in the PCB array.
 *   SIDE EFFECTS: none
 */
int32_t do_getargs (uint8_t* buf, int32_t nbytes) {
    if(buf == NULL) {
        return ERR;
    }

    /* no process is running, should not occur */
    if(!current_pcb[active_task_idx]) {
        return ERR;
    }

    /* don't allow to user to read/write kernel memory */
    if (!((uint32_t)buf >= USR_PRG_VIRTUAL_START && 
        (uint32_t)buf < USR_PRG_VIRTUAL_END))
        return ERR;

    int32_t args_len=0;
    while((current_pcb[active_task_idx]->args)[args_len]!='\0') {
        args_len++;
    }
    args_len++; //the last null

    if(nbytes<args_len) {
        return ERR;
    }
    int32_t i=0;
    for(i=0; i<args_len; ++i) {
        buf[i]=(current_pcb[active_task_idx]->args)[i];
    }
    return SUCCESS;
 }

/*
 * do_vidmap
 *   DESCRIPTION: map the physical video memory to a virtual user page to 
 *                implement protection
 *   INPUTS: none
 *   OUTPUTS: screen_start - user level pointer to the start of the virtual 
 *             memory page that is mapped to the physical page.
 *   RETURN VALUE: 0 for success, -1 for fail
 *   SIDE EFFECTS: changes page tables of running process
 */
 int32_t do_vidmap (uint8_t** screen_start) {
    if (screen_start==NULL)
        return ERR;

    /* no process is running, should not occur */
    if(!current_pcb[active_task_idx]) {
        return ERR;
    }

    /* don't allow to user to read/write kernel memory */
    if (!((uint32_t)screen_start >= USR_PRG_VIRTUAL_START && 
        (uint32_t)screen_start < USR_PRG_VIRTUAL_END))
        return ERR;

    cli();
    /* fix user page to starting at virtual 132MB */
    *screen_start = (uint8_t*) USR_VIDMAP_VIRTUAL_START;

    /* map the actual video memory to the user virtual page, 
    adjust permissions */
    map_kilo_page((uint32_t)*screen_start, VIDEO_MEM_START, 
        current_pcb[active_task_idx]->pid + PAGE_DIR_USER_IDX_OFFSET, USR_DPL);
    sti();
    return SUCCESS;
 }

/*
 * do_set_handler
 *   DESCRIPTION: set signal handler of the corresponding signum to handler provided by user
 *   INPUTS: 
 *   OUTPUTS: 
 *   RETURN VALUE: 0 for success, -1 for fail
 *   SIDE EFFECTS: 
 */
int32_t do_set_handler(int32_t signum, void* handler_address) {
    if (handler_address == NULL || signum < 0 || signum >= NUM_SIGNAL)
        return ERR;

    current_pcb[active_task_idx]->signal_handler[signum] = handler_address;

    return SUCCESS;
}


/*
 * do_sigreturn
 *   DESCRIPTION:
 *   INPUTS: 
 *   OUTPUTS: 
 *   RETURN VALUE: 0 for success, -1 for fail
 *   SIDE EFFECTS: 
 */
int32_t do_sigreturn() {
    uint32_t eax, ebp, usr_stack_top, context_top;
    asm volatile(
        "movl %%ebp, %0;"
        : "=r"(ebp)
        :
        : "cc");

    context_top = ebp + 2*4;
    //get original esp
    usr_stack_top = *(uint32_t*) (context_top + ESP_OFFSET);
    memcpy((void*)context_top, (void*)(usr_stack_top + 4), CONTEXT_SIZE);
    eax = *(uint32_t*) (context_top + EAX_OFFSET);

    unmask_signals(active_task_idx);

    asm volatile(
        "movl %0, %%eax;"
        "leave;"
        "ret;"
        :
        : "r"(eax)
        : "cc");
     /* should not be executed */
     return ERR;
}

 /*
 * launch_shell
 *   DESCRIPTION: set up the execution context of the inital shells, 
 *                i.e don't iret, so that the pit handler, when switching
 *                to the shells on the first try, can iret and start executing
 *                the new shells
 *   INPUTS: pid - pid of the shell to set up
 *   OUTPUTS: none
 *   RETURN VALUE: 0 for success, -1 for fail
 *   SIDE EFFECTS: sets up a new execution context for inital shell 
 *                 with specified pid
 */
 int32_t 
 launch_shell (uint32_t pid) {
    int32_t fd;

    active_task_idx = pid;

    fd = do_open((const uint8_t*)"shell");

    //!!!! need to reset file pos
    uint8_t exec_header[EXEC_HEADER_LEN];
    do_read(fd, exec_header, EXEC_HEADER_LEN);
    uint32_t entry_point=0;

    /* find the entry point */
    int i;
    for(i=0; i<PTR_SIZE; ++i) {
        entry_point=entry_point+(exec_header[ENTRY_POINT_START+i]<<(i)*
            ENTRY_POINT_OFFSET);
    }   //little endian???
    // printf("%d\n", entry_point);

    //close fd from old pcb
    do_close(fd);

    pcb_array[pid]->flag=TASK_ACTIVE;
    pcb_array[pid]->pid=pid;

    pcb_array[pid]->signal_handler[DIV_ZERO] = signal_handler_default[DIV_ZERO];
    pcb_array[pid]->signal_handler[SEGFAULT] = signal_handler_default[SEGFAULT];
    pcb_array[pid]->signal_handler[INTERRUPT] = signal_handler_default[INTERRUPT];
    pcb_array[pid]->signal_handler[ALARM] = signal_handler_default[ALARM];
    pcb_array[pid]->signal_handler[USER1] = signal_handler_default[USER1];

    //map kernel page
    map_mega_page(KERNEL_START, KERNEL_START, pid + PAGE_DIR_USER_IDX_OFFSET, KERNEL_DPL);
    //map vieo memory
    map_kilo_page(VIDEO_MEM_START, VIDEO_MEM_START, pid + PAGE_DIR_USER_IDX_OFFSET, KERNEL_DPL);
    //map all three video memory buffer, each buffer is assigned to a terminal
    map_kilo_page(VIDEO_BUFFER_BASE, VIDEO_BUFFER_BASE, pid + PAGE_DIR_USER_IDX_OFFSET, KERNEL_DPL);
    //second video memory buffer
    map_kilo_page(VIDEO_BUFFER_BASE + VIDEO_MEM_SIZE, VIDEO_BUFFER_BASE + VIDEO_MEM_SIZE, 
        pid + PAGE_DIR_USER_IDX_OFFSET, KERNEL_DPL);
    //third video memory buffer
    map_kilo_page(VIDEO_BUFFER_BASE + 2 * VIDEO_MEM_SIZE, VIDEO_BUFFER_BASE + 2 * VIDEO_MEM_SIZE, 
        pid + PAGE_DIR_USER_IDX_OFFSET, KERNEL_DPL);
    //map user program page
    map_mega_page(USR_PRG_VIRTUAL_START, USR_PRG_PHY_BASE + pid * DIR_ADDRESSABLE, pid + PAGE_DIR_USER_IDX_OFFSET, USR_DPL);
    //flush TLB
    update_page_directory(pid + PAGE_DIR_USER_IDX_OFFSET);

    /* copy program image to correct offset in 4MB page with virtual base 
    address of 128MB */
    void* program_image = (void*) PRG_OFFSET;

    //open fd in new pcb
    fd=do_open((const uint8_t*)"shell");
    do_read(fd, program_image, pcb_array[pid]->file_descriptors[fd].inode->length_in_byte);
    do_close(fd);
    
    //do not set tss here
    //see task.c , each Kmode stack + PCB is 8KB large
    // printf("entering user mode\n");

    /* the following code is adapted from 
     * http://www.jamesmolloy.co.uk/tutorial_html/10.-User%20Mode.html
     * need double check
     */

    /* set up the stack for the fake iret. Make this atomic, we don't want other
    interrupts to mess up the stack */

    uint32_t original_esp;
    uint32_t new_esp = KERNEL_END - (pid * 8 * KILO) - KMODE_STACK_OFFSET;

    /* push the new kmode stack pointer */
    asm volatile(
        "movl %%esp, %0;"
        :"=r"(original_esp)
        :
        :"cc");

    asm volatile(
        "movl %0, %%esp;"
        :
        :"r"(new_esp)
        :"cc");

    asm volatile(
        "movw %w0, %%ax;"

        // set segment selectors to user data segment
        "movw %%ax, %%ds;"
        "movw %%ax, %%es;"
        "movw %%ax, %%fs;"
        "movw %%ax, %%gs;"

        // user data segment
        "pushl %1;"

        // stack pointer
        "pushl %2;"

        // push flags
        "pushfl;"

        // reenable interrupt by setting IF flag
        "popl %%eax;"
        "orl $0x200, %%eax;"
        "pushl %%eax;"

        // user code segment, change RPL to 3
        "pushl %3;"

        // EIP
        "pushl %4;"
        :
        :"r"(USER_DS), "r"(USER_DS), "r"(USR_PRG_VIRTUAL_END - 
            KMODE_STACK_OFFSET), 
            "r"(USER_CS), "r"(entry_point)
        :"cc", "eax");

    // set up inital stack context so the first context switch works
    asm volatile(
        "pushl $0xFFFFFFFF;"
        "pushw %%fs;"
        "pushw %%gs;"               
        "pushw %%es;"               
        "pushw %%ds;"             
        "pushl %%eax;"
        "pushl %%ebp;" 
        "pushl %%esi;" 
        "pushl %%edi;"
        "pushl %%edx;"   
        "pushl %%ecx;"  
        "pushl %%ebx;"
        "pushl $pit_handler_32_ret;"
        "pushl $0;"  /* stored ebp for the pit handler */
        "movw %w0, %%cx;"
        "movw %%cx, %%ds;"
        :
        : "r"(KERNEL_DS)
        : "cc");

    /* save the new shell's inital esp and ebp */
    asm volatile(
        "movl %%esp, %0;"
        "movl %%esp, %1;"
        :"=r"(pcb_array[pid]->esp), "=r"(pcb_array[pid]->ebp)   //save esp to saved_ebp!!!
        :
        :"cc");

     /* go back to the inital kernel stack */
    asm volatile(
        "movl %0, %%esp;"
        :
        :"r"(original_esp)
        :"cc");

    return SUCCESS;
 }


void* do_malloc(int32_t size) {
    int32_t alloc_size;
    if(size > 1024) return NULL;
    int32_t i;
    for(i=0; i<4; i++) {
        if(size < (1 << (i+7))) {
            alloc_size = 1 << (i+7);
            break;
        }
    }

    int32_t j;
    for(j = 0; j < (SLAB_PAGE_SIZE / alloc_size); j++) {
        if(!(current_pcb[active_task_idx]->slab_cache[i].bitmap & (1 << j))) {
            current_pcb[active_task_idx]->slab_cache[i].bitmap |= (1 << j);
            return current_pcb[active_task_idx]->slab_cache[i].cache_ptr + alloc_size * j;
        }
    }

    return NULL;
}

int32_t do_free(void* ptr) {
    if(ptr == NULL) return ERR;

    int32_t ctr;
    for (ctr = 0; ctr < NUM_SLABS; ++ctr)
    {
        if(((uint32_t)ptr & TWENTY_HIGH_BIT_MASK) & (USR_PRG_VIRTUAL_END + (ctr+1) * VIDEO_MEM_SIZE)) {
            int32_t alloc_size = 1 << (ctr + 6);
            int32_t j = (ptr - current_pcb[active_task_idx]->slab_cache[ctr].cache_ptr) / alloc_size;
            current_pcb[active_task_idx]->slab_cache[ctr].bitmap &= ~(1 << j);
            return SUCCESS;
        }
    }
    return ERR;
}

int32_t do_touch(const uint8_t* filename) {
    //return err if file already exist
    if (do_open(filename) != -1)
        return ERR;

    return filesystem_touch(filename);
}

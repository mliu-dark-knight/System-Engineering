#include "task.h"
#include "terminal.h"
#include "lib.h"
#include "x86_desc.h"


// Important!!!!!!!! the first pcb pointer correspond to the base shell of three terminals
/* pointer to the currently running process's PCB */
//currently executing task on each terminal
pcb_t* current_pcb[NUM_TERMINAL];

/* array to all PCBs maximum is 6 simultaneous processes */
pcb_t* pcb_array[MAX_PCB];

//currently executing task 
int32_t active_task_idx=ERR;

//asm(".globl check_signals")

/*
 * init_pcb
 *   DESCRIPTION: initalize all of the PCBs stored in the PCB array to known 
 *				  values to ensure we don't crash and for security reasons by
 *				  avoiding uninitalized values.
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: 0 for success, -1 for fail
 *   SIDE EFFECTS: none
 */
void init_pcb() {
	int i;
	for (i=0; i<MAX_PCB; i++) {
		/* PID i occupies index i+1 in the PCB array, PCB array index 0 is
		reserved for the initial startup shell. Each PCB is 8KB large 
		(we include the kernel mode stack for each process as part of 
		the PCB) */
		pcb_array[i] = (pcb_t*)(KERNEL_END - (i + PID_USER_OFFSET) * (8 * KILO));

		/* clear the PCB for security reasons */
		memset(pcb_array[i], 0x00, sizeof(pcb_t));

		//set parent pid to empty task, and task state to no task running
		pcb_array[i]->parent_pid=INVALID_PID;
		pcb_array[i]->flag=TASK_NOT_PRESENT;
		pcb_array[i]->pid=i;

		int j;
		for (j=0; j<NUM_SIGNAL; j++) {
			pcb_array[i]->signal_flag[j]=SIGNAL_NOT_PENDING;
			pcb_array[i]->signal_mask[i]=SIGNAL_MASK_OFF;
		}
	}

	/* initalize the first three shells to run on terminal 0, and set the current
	process to PCB array index 0. Since the flag is still TASK_NOT_PRESENT there
	are no ill effects. We need an inital terminal number and inital process
	since the execute syscall assume it will be run from currently running 
	process so the first process needs to be special cased */
	pcb_array[TERM_0]->terminal_idx=TERM_0;
	pcb_array[TERM_1]->terminal_idx=TERM_1;
	pcb_array[TERM_2]->terminal_idx=TERM_2;

	// the first pcb pointer correspond to the base shell of three terminals
	current_pcb[TERM_0]=pcb_array[TERM_0];
	current_pcb[TERM_1]=pcb_array[TERM_1];
	current_pcb[TERM_2]=pcb_array[TERM_2];
}

/*
 * get_cur_pid
 *   DESCRIPTION: accessor function to retrieve the current running process's 
 *				  PID
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: the current process's PID. System calls are such that 
 *				   current_pcb[active_idx] is always valid once inital shell has started up.
 *   SIDE EFFECTS: none
 */
uint32_t get_cur_pid() {
	return current_pcb[active_task_idx]->pid;
}


static void handle_signal(uint32_t signum) {
	if (current_pcb[active_task_idx]->signal_handler[signum] == NULL)
		return;

	mask_signals(active_task_idx);

	uint32_t ebp, new_usr_stack_top, context_top, cs;

	asm volatile(
		"movl %%ebp, %0;"
		: "=r"(ebp)
		: 
		: "cc");

	ebp = *(uint32_t*)ebp;

	// printf("ebp = %x\n", ebp);

	context_top = ebp + 2*4;

	cs = *(uint32_t*) (context_top + CS_OFFSET);

	//check if we are returning to user space
	if (cs == KERNEL_CS) {
		unmask_signals(active_task_idx);
		return;
	}

	//get original esp
	new_usr_stack_top = *(uint32_t*) (context_top + ESP_OFFSET);
	// printf("new_usr_stack_top = %x\n", new_usr_stack_top);

	//INT 0x80, need implement
	new_usr_stack_top -= 4;
	*(uint32_t*) new_usr_stack_top = 0x8380CD00;
	//set eax, need implement
	new_usr_stack_top -= 4;
	*(uint32_t*) new_usr_stack_top = 0xAB8;

	//copy context to user space
	new_usr_stack_top -= CONTEXT_SIZE;
	memcpy((void*)new_usr_stack_top, (void*)context_top, CONTEXT_SIZE);

	//signal number
	new_usr_stack_top -= 4;
	*(uint32_t*) new_usr_stack_top = signum;

	//return address
	new_usr_stack_top -= 4;
	*(uint32_t*) new_usr_stack_top = new_usr_stack_top + 2*4 + CONTEXT_SIZE;

	//change CS and SS if using default handler
	if (current_pcb[active_task_idx]->signal_handler[signum] == signal_handler_default[signum]) {
		*(uint32_t*) (context_top + CS_OFFSET) = KERNEL_CS;
		*(uint32_t*) (context_top + SS_OFFSET) = KERNEL_DS;
	}
	//set new esp to new user stack top
	*(uint32_t*) (context_top + ESP_OFFSET) = new_usr_stack_top;

	//set new return address to signal handler
	asm volatile(
		"movl %0, (%1);"
		:
		: "r"(current_pcb[active_task_idx]->signal_handler[signum]), "r"(context_top + RETURN_ADDRESS_OFFSET)
		: "cc");

	//signal about to be handled, mark as not pending
	current_pcb[active_task_idx]->signal_flag[signum] = SIGNAL_NOT_PENDING;

	return;
}


void check_signals() {
	cli();
	//save return value from interrupt handler or system call
	uint32_t eax;
	asm volatile(
		"movl %%eax, %0;"
		: "=r"(eax)
		:
		: "cc");

	if (active_task_idx == ERR) {
		//restore return value
		asm volatile(
			"movl %0, %%eax;"
			"sti;"
			:
			: "r"(eax)
			: "cc");
		return;
	}

	uint32_t i;
	for (i=0; i<NUM_SIGNAL; i++) {
		if (current_pcb[active_task_idx]->signal_flag[i] == SIGNAL_PENDING &&
			current_pcb[active_task_idx]->signal_mask[i] == SIGNAL_MASK_OFF) {
			handle_signal(i);
			break;
		}
	}

	//restore return value
	asm volatile(
			"movl %0, %%eax;"
			"sti;"
			:
			: "r"(eax)
			: "cc");
	return;
}

void mask_signals(uint32_t task_idx) {
	int i;
	for (i=0; i<NUM_SIGNAL; i++)
		current_pcb[task_idx]->signal_mask[i] = SIGNAL_MASK_ON;
}

void unmask_signals(uint32_t task_idx) {
	int i;
	for (i=0; i<NUM_SIGNAL; i++)
		current_pcb[task_idx]->signal_mask[i] = SIGNAL_MASK_OFF;
}

void unpend_signals(uint32_t task_idx) {
	int i;
	for (i=0; i<NUM_SIGNAL; i++)
		current_pcb[task_idx]->signal_flag[i] = SIGNAL_NOT_PENDING;
}


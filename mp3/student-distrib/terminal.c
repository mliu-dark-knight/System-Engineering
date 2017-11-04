#include "keyboard.h"
#include "lib.h"
#include "terminal.h"
#include "syscall.h"
#include "i8259.h"
#include "task.h"

/* screen printing constants */
#define SCREEN_START_X 0
#define SCREEN_START_Y 0

/* keyboard response bytes */
#define ACK 0xFA

/* keycoodes for modifier keys */
#define BACK_SPACE 0x0E
#define CONTROL_PRESSED 0X1D
#define CONTROL_RELEASED 0x9D
#define CAPS_LOCK 0x3A

#define LEFT_SHIFT_RELEASED 0xAA
#define RIGHT_SHIFT_RELEASED 0xB6
#define LEFT_SHIFT_PRESSED 0x2A
#define RIGHT_SHIFT_PRESSED 0x36
#define ALT_PRESSED 0x38

/* max valid file descriptor */
#define FD_MAX 8

/* scancode map, scan set 1. Normal, then Shift, then Caps, then Shift + Caps */
static uint8_t key_map[NEW_FIRST_KEYS] = {
    '\0', '\0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    '0', '-', '=', '\b', '\0', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',
    '[', ']', '\n', '\0', 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'', '`', '\0', '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    '\0', '*', '\0', ' ', '\0',

    '\0', '\0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    '0', '-', '=', '\0', '\0', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P',
    '[', ']', '\n', '\0', 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';',
    '\'', '`', '\0', '\\', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/',
    '\0', '*', '\0', ' ', '\0',

    '\0', '\0', '!', '@', '#', '$', '%', '^', '&', '*', '(',
    ')', '_', '+', '\0', '\0', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P',
    '{', '}', '\n', '\0', 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    '\"', '~', '\0', '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',
    '\0', '*', '\0', ' ', '\0',

    '\0', '\0', '!', '@', '#', '$', '%', '^', '&', '*', '(',
    ')', '_', '+', '\0', '\0', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',
    '{', '}', '\n', '\0', 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ':',
    '\"', '~', '\0', '|', 'z', 'x', 'c', 'v', 'b', 'n', 'm', '<', '>', '?',
    '\0', '*', '\0', ' ', '\0'
};

/* stores the status, including what is on screen for each terminal */
static terminal_state_t terminal_state[NUM_TERMINAL];
/* index of currently displayed terminal */
int32_t active_terminal_idx = 0;

/* function prototypes for internal functions */
static void clear_buffer(uint32_t terminal_idx);
static void init_terminal_idx(uint32_t terminal_idx);


/*
 * clear_buffer
 *   DESCRIPTION: open the terminal of the input idx
 *   INPUTS: terminal_idx: the idx of the terminal whose buffer to be cleared
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: buffer of the terminal of the index cleared
 */
static void clear_buffer(uint32_t terminal_idx) {
    if (terminal_idx >= NUM_TERMINAL)
        return;
    /* clear buffer by storing with all nulls */
    memset(terminal_state[terminal_idx].buffer, '\0', BUFFER_LEN);

    /* reset next character position to top left */
    terminal_state[terminal_idx].buffer_location=0;
    terminal_state[terminal_idx].read_location=0;
    terminal_state[terminal_idx].buffer_end_location=0;
    terminal_state[terminal_idx].enter_pressed=false;
}


/*
 * init_terminal_idx
 *   DESCRIPTION: initialize the terminal of the input idx
 *   INPUTS: terminal_idx: the idx of the terminal to be initialized
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: terminal of the index initialized. Terminals default to
 *                 being closed except for terminal 0.
 */
static void init_terminal_idx(uint32_t terminal_idx) {
    if (terminal_idx >= NUM_TERMINAL)
        return;
    // Terminal 0 always defaults to being open.
    if (terminal_idx==TERM_0) {
        terminal_state[terminal_idx].open=true;
        int32_t flags;
        cli_and_save(flags);
        terminal_state[terminal_idx].cursor_y=get_screen_y();
        terminal_state[terminal_idx].cursor_x=get_screen_x();
        restore_flags(flags);
    }
    else
        terminal_state[terminal_idx].open=false;
    //set video_buffer
    terminal_state[terminal_idx].video_buffer = (int8_t*)(VIDEO_BUFFER_BASE + terminal_idx * VIDEO_MEM_SIZE);
    clear_buffer(terminal_idx);
}

/*
 * get_terminal_vid_buffer
 *   DESCRIPTION: none
 *   INPUTS: none
 *   OUTPUTS: video_buffer
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
int8_t* get_terminal_vid_buffer(uint32_t terminal_idx) {
    return terminal_state[terminal_idx].video_buffer;
}


/*
 * save_screen
 *   DESCRIPTION: Save the current screen of the terminal to its internal
 *                buffer. Save the cursor location.
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: Reads from video RAM.
 */
 void save_screen() {
    int32_t flags;
    cli_and_save(flags);
    //save screen bytes
    memcpy(terminal_state[active_terminal_idx].video_buffer, (int8_t*)VIDEO, VIDEO_MEM_SIZE);

    //Save cursor loc
    terminal_state[active_terminal_idx].cursor_y=get_screen_y();
    terminal_state[active_terminal_idx].cursor_x=get_screen_x();

    restore_flags(flags);
}

/*
 * restore_screen
 *   DESCRIPTION: restore screen to video buffer
 *   INPUTS: terminal_idx
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: update cursor
 */
 void restore_screen(uint32_t terminal_idx) {
    int32_t flags;
    cli_and_save(flags);
    /* rewrite the terminal with the data from the target terminal */
    memcpy((int8_t*)VIDEO, terminal_state[terminal_idx].video_buffer, VIDEO_MEM_SIZE);
    update_cursor(terminal_state[terminal_idx].cursor_y, terminal_state[terminal_idx].cursor_x);
    restore_flags(flags);
}



/*
 * init_terminal
 *   DESCRIPTION: initialize all terminals - default initalize 3 terminals, and 
 *                set active terminal to 0. For kernel.c use only.
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: terminals initialized, active terminal 0 displayed
 */
void init_terminal() {
    int32_t i;
    for (i=0; i<NUM_TERMINAL; i++)
        init_terminal_idx(i);

    /* set active terminal to terminal 0 */
    active_terminal_idx = 0;
}


/*
 * switch_terminal
 *   DESCRIPTION: callback function when the terminal is switched
 *   INPUTS: terminal_idx: the idx of the terminal
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: terminal switched, current terminal's buffers modified.
 *                 video RAM reloaded with data from target terminal's buffer
 */
void switch_terminal(uint32_t terminal_idx) {
    if (terminal_idx >= NUM_TERMINAL)
        return;
    /* do nothing if it is already the active one */
    if (terminal_idx == active_terminal_idx)
        return;
    /* save the current screen bytes to the internal buffer of the
    current terminal */
    
    int32_t flags;
    cli_and_save(flags);
    //diable interrupts, especially from pit
    save_screen();

    map_kilo_page(USR_VIDMAP_VIRTUAL_START, (uint32_t)terminal_state[active_terminal_idx].video_buffer,
        current_pcb[active_terminal_idx]->pid + PAGE_DIR_USER_IDX_OFFSET, USR_DPL);

    /* set the current terminal to the one we want to switch to */
    active_terminal_idx=terminal_idx;

    /* rewrite the terminal with the data from the target terminal */
    restore_screen(terminal_idx);

    map_kilo_page(USR_VIDMAP_VIRTUAL_START, VIDEO_MEM_START,
        current_pcb[active_terminal_idx]->pid + PAGE_DIR_USER_IDX_OFFSET, USR_DPL);

    restore_flags(flags);
}


/*
 * terminal_open
 *   DESCRIPTION: internal terminal open function
 *   INPUTS: filename: ignored - syscall open validates filename is correct
 *   OUTPUTS: none
 *   RETURN VALUE: 0 for success, -1 for fail
 *   SIDE EFFECTS: file array updated
 */
int32_t terminal_open (const uint8_t* filename) {
    return 0;
}


/*
 * terminal_close
 *   DESCRIPTION: close the active terminal (stdin/stdout).
 *   INPUTS: fd: unused - fd is closed by close syscall
 *   OUTPUTS: none
 *   RETURN VALUE: 0 for success, -1 for fail
 *   SIDE EFFECTS: none
 */
int32_t terminal_close (int32_t fd) {
    if (fd < 0 || fd >= FD_MAX)
        return ERR;
    //terminal_state[active_terminal_idx].open = false;
    return 0;
}


/*
 * terminal_read
 *   DESCRIPTION: read from the terminal buffer
 *   INPUTS:
 *          fd: index in the file array
 *          buf: destination of the data
 *          nbytes: length in bytes to be read
 *   OUTPUTS: none
 *   RETURN VALUE: a nonnegative number for number of bytes written, -1 for fail
 *   SIDE EFFECTS: none
 */
int32_t terminal_read (int32_t fd, void* buf, int32_t nbytes) {
    /* validate the file descriptor/ pointer to buf */
    if (fd < 0 || fd >= FD_MAX || buf == NULL)
        return ERR;

    /* block until user presses enter */
    while (terminal_state[active_task_idx].enter_pressed==false);

    int32_t flags;
    cli_and_save(flags);
    /* read as many bytes as possible */
    uint32_t boundary = nbytes > terminal_state[active_task_idx].buffer_end_location 
        - terminal_state[active_task_idx].read_location ? 
        terminal_state[active_task_idx].buffer_end_location - 
        terminal_state[active_task_idx].read_location : nbytes;

    uint8_t* buff=(uint8_t*)buf;
    memset(buff, '\0', boundary);

    memcpy(buf, terminal_state[active_task_idx].buffer + 
        terminal_state[active_task_idx].read_location, boundary);

    /* increment the internal buffer position counter */
    terminal_state[active_task_idx].read_location += boundary;

    /* block on the next call if we have no data left */
    if(terminal_state[active_task_idx].read_location >= terminal_state[active_task_idx].buffer_end_location-1) {
        clear_buffer(active_task_idx);
    }
    restore_flags(flags);
    
    return boundary;
}

/*
 * terminal_write
 *   DESCRIPTION: write to the terminal
 *   INPUTS:
 *          fd: index in the file array
 *          buf: source of the data
 *          nbytes: length in bytes to be write
 *   OUTPUTS: none
 *   RETURN VALUE: a nonnegative number for number of bytes written, -1 for fail
 *   SIDE EFFECTS: none
 */
int32_t terminal_write (int32_t fd, const void* buf, int32_t nbytes) {
    /* validate the file descriptor/ pointer to buf */
    if (fd < 0 || fd >= FD_MAX || buf == NULL)
        return ERR;
    int32_t i;
    uint8_t* buff = (uint8_t*)buf;

    /* print all characters - screen printing routines take care of advancing 
    screen so no risk of overflow */
    int32_t flags;
    cli_and_save(flags);
    //assume terminal write never write backspace, since backspace can only be typed by user
    for (i=0; i<nbytes; i++) {
        if (active_terminal_idx == active_task_idx)
            putc(buff[i]);
        else
            putchar(buff[i], terminal_state[active_task_idx].video_buffer, 
                    &(terminal_state[active_task_idx].cursor_x), 
                    &(terminal_state[active_task_idx].cursor_y));
    }
    restore_flags(flags);
    return nbytes;
}

/*
 * buffer_char
 *   DESCRIPTION: check if scancode represents a char to be saved to buffer
 *                (printable or needs to be processed)
 *   INPUTS: key: the scancode of the char
 *   OUTPUTS: none
 *   RETURN VALUE: 0 for false, 1 for true
 *   SIDE EFFECTS: none
 */
static uint32_t buffer_char(uint8_t key) {
    return ((key>=2 && key<=13) ||  /* is key numeric or - or = */
        (key>=16 && key<=28) || /* is key alphabetic or [ or ] or enter */  
        (key>=30 && key<=41) || /* is key alphabetic or ' or ` or " */ 
        (key>=43 && key<=53) || /* is key \ or alphabetic or , or . or / */
        (key==55 || key==57)); /* is key <space> or a control character */
}



/*
 * keyboard_to_terminal
 *   DESCRIPTION: callback function when the keyboard updated
 *   INPUTS: keyboard_state: the state of the keyboard
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: react to the changed state of the keyboard
 */
void keyboard_to_terminal (keyboard_state_t keyboard_state) {
    int32_t flags;
    cli_and_save(flags);

    // key pressed
    if (keyboard_state.last_key>0 && keyboard_state.last_key<FIRST_KEYS) {
        uint32_t current='\0';
        if (terminal_state[active_terminal_idx].enter_pressed && 
            buffer_char(keyboard_state.last_key))
            clear_buffer(active_terminal_idx);

        /* backspace pressed */
        if (keyboard_state.last_key == BACK_SPACE && 
            terminal_state[active_terminal_idx].buffer_location>0) {
            keyboard_backspace();
            if (terminal_state[active_terminal_idx].buffer_location>0)
                terminal_state[active_terminal_idx].buffer_location--;
            // put a null char on the screen
            terminal_state[active_terminal_idx].buffer[terminal_state[active_terminal_idx].
                buffer_location]='\0';
        }
        
        /* enter key */
        if (key_map[(uint32_t)(keyboard_state.last_key)]=='\n' || 
            key_map[(uint32_t)(keyboard_state.last_key)]=='\r') {
            terminal_state[active_terminal_idx].enter_pressed=true;
            terminal_state[active_terminal_idx].buffer[terminal_state[active_terminal_idx].
                buffer_location]='\n';
            terminal_state[active_terminal_idx].buffer_end_location = 
            terminal_state[active_terminal_idx].buffer_location + 1;
            /* so that we also copy the newline inserted at index 127, i.e. the 
            128th character in the buffer */
            terminal_state[active_terminal_idx].buffer_location=0;
            printf("%c", key_map[(uint32_t)(keyboard_state.last_key)]);
        }

        /* cntl+l -> should clear the screen */
        else if (keyboard_state.control_on && 
            key_map[(uint32_t)(keyboard_state.last_key)] == 'l')
            clear();

        /* character that can be put into buffer */
        else if (buffer_char(keyboard_state.last_key) &&
            terminal_state[active_terminal_idx].buffer_location<BUFFER_LEN - 1){
            /* ignore non-newline characters after buffer already has 127 
            characters*/
            if ((!keyboard_state.caps_lock_on)&&(!keyboard_state.shift_on)) {
                printf("%c", key_map[(uint32_t)(keyboard_state.last_key)]);
                current=key_map[(uint32_t)(keyboard_state.last_key)];
            }   //no shift no cap
            else if ((!keyboard_state.shift_on)&&(keyboard_state.caps_lock_on)) {
                printf("%c", key_map[(uint32_t)(keyboard_state.last_key) + 
                    FIRST_KEYS]);
                current=key_map[(uint32_t)(keyboard_state.last_key) + FIRST_KEYS];
            }   //no shift but cap
            else if ((keyboard_state.shift_on)&&(keyboard_state.caps_lock_on)) {
                printf("%c", key_map[(uint32_t)(keyboard_state.last_key) + 
                    THIRD_KEYS]);
                current=key_map[(uint32_t)(keyboard_state.last_key) + THIRD_KEYS];
            }   //shift and cap
            else {
                printf("%c", key_map[(uint32_t)(keyboard_state.last_key) + 
                    SECOND_KEYS]);
                current=key_map[(uint32_t)(keyboard_state.last_key) + SECOND_KEYS];
            }   //shift but no cap

            /* set buffer */
            terminal_state[active_terminal_idx].buffer[terminal_state[active_terminal_idx].
                buffer_location]=current;
            terminal_state[active_terminal_idx].buffer_location++;
        }
    }
    restore_flags(flags);
}


/*
 * test_terminal_read
 *   DESCRIPTION: test the terminal read
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: reads from buffer and advances contents in buffer
 */
void test_terminal_read() {
    int8_t local=4;
    int8_t buffer[local][BUFFER_LEN];
    int8_t num_char[local];

    int j;
    for (j=0; j<local; j++){
        num_char[j]=terminal_read(0, buffer[j], 16 * (j+1));
        printf("\nTesting read: ");

        /* print the characters back out */
        int i;
        for (i=0; i<num_char[j]; i++)
            putc(buffer[j][i]);
    }
}


/*
 * test_terminal_write
 *   DESCRIPTION: test the terminal write
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: prints to screen and buffer of terminal
 */
void test_terminal_write() {
    /* test backspace character functionality */
    int8_t* buffer="Testing\nTerminal\nWrite\nWrite\b\b\b\b\b\b\n!!!\n";
    terminal_write(0, buffer, strlen(buffer));

    int8_t* buffer1="Testing\nTerminal\nWrite";
    int32_t i;

    /* write a large amount of text to screen to test scroling */
    for (i = 0; i < 150; ++i)
    {
        terminal_write(0, buffer1, strlen(buffer1));
    }
}

#include "keyboard.h"
#include "lib.h"
#include "i8259.h"
#include "terminal.h"
#include "filesystem.h"
#include "task.h"

/* keyboard port constants */
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_CMD_REG 0x64

/* keyboard initalization commands */
#define DISABLE_DEV_1 0xAD
#define ENABLE_DEV_1 0xAE
#define SET_REPEAT 0xF3
#define RATE_30HZ_DELAY_250MS 0x00

/* keyboard response bytes */
#define ACK 0xFA
#define KEY_RELEASE_PREFIX 0xE0

#define BACK_SPACE 0x0E
#define CONTROL_PRESSED 0X1D
#define CONTROL_RELEASED 0x9D
#define CAPS_LOCK 0x3A

#define LEFT_SHIFT_RELEASED 0xAA
#define RIGHT_SHIFT_RELEASED 0xB6
#define LEFT_SHIFT_PRESSED 0x2A
#define RIGHT_SHIFT_PRESSED 0x36
#define ALT_PRESSED 0x38
#define ALT_RELEASED 0xB8
#define F1_PRESSED 0x3B
#define F2_PRESSED 0x3C
#define F3_PRESSED 0x3D
#define C_PRESSED 0x2E

//keyboard state
static volatile keyboard_state_t keyboard_state;
static uint8_t alt_on;

//set of operations
//==================================
/*
 * keyboard_read
 *   DESCRIPTION: output the last key inputed
 *   INPUTS: 
 *          fd: index in the file array
 *          nbytes: length in bytes to be read
 *   OUTPUTS: buf: destination of the data
 *   RETURN VALUE: -1 for fail, or a nonnegative number for number of bytes read
 *   SIDE EFFECTS: none
 */
int32_t keyboard_read (int32_t fd, void* buf, int32_t nbytes) {
    /* null ptr check and input size check */
    if(!buf || nbytes != sizeof(keyboard_state.last_key)) {
        return ERR;
    }

    if (fd < 0 || fd >= FD_MAX)
        return ERR;

    uint8_t* temp = (uint8_t*) buf;
    *temp = keyboard_state.last_key;
    return sizeof(keyboard_state.last_key);
}

/*
 * keyboard_write
 *   DESCRIPTION: write to the keyboard - not possible and so always fails
 *   INPUTS: All arguments ignored.
 *          fd: index in the file array
 *          buf: source of the data
 *          nbytes: length in bytes to be write
 *   OUTPUTS: none
 *   RETURN VALUE: always fails with -1
 *   SIDE EFFECTS: none
 */
int32_t keyboard_write (int32_t fd, const void* buf, int32_t nbytes) {
    return ERR;
}


/*
 * keyboard_open
 *   DESCRIPTION: keyboard specific initalization - does nothing
 *   INPUTS: 
 *          fname: filename
 *   OUTPUTS: none
 *   RETURN VALUE: 0 for success always
 *   SIDE EFFECTS: file array updated
 */
int32_t keyboard_open (const uint8_t* fname) {
    if(fname == NULL) return ERR;
    return SUCCESS;
}


/*
 * keyboard_close
 *   DESCRIPTION: keyboard specific close - does nothing
 *   INPUTS: fd: index in the file array
 *   OUTPUTS: none
 *   RETURN VALUE: 0 for success always
 *   SIDE EFFECTS: none
 */
int32_t keyboard_close (int32_t fd) {
    if (fd < 0 || fd >= FD_MAX)
        return ERR;
    return SUCCESS;
}
//==================================
//set of operations done




/*
 * init_keyboard
 *   DESCRIPTION: initzalize the keyboard
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: keyboard's irq is enabled, keyboard buffer flushed and
 *                 keyboard set to repeat keys.
 */
void init_keyboard () {
    disable_irq(IRQ_1); /* so reinitalization also works */

    /* temporarily disable commands so init is not messed up by scancodes sent 
    back */
    outb(DISABLE_DEV_1, KEYBOARD_CMD_REG);

    inb(KEYBOARD_DATA_PORT); /* flush the current data leftover in the buffer */

    outb(SET_REPEAT, KEYBOARD_CMD_REG);
    outb(RATE_30HZ_DELAY_250MS, KEYBOARD_DATA_PORT);

    outb(ENABLE_DEV_1, KEYBOARD_CMD_REG);

    inb(KEYBOARD_DATA_PORT); /* flush the current data leftover in the buffer */

    /* initalize the keyboard status struct with default values */
    keyboard_state.last_key = false;
    keyboard_state.caps_lock_on = false;
    keyboard_state.control_on = false;
    keyboard_state.shift_on = false;

	enable_irq(IRQ_1);
}


/*
 * keyboard_handler_33
 *   DESCRIPTION: keyboard interrupt handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: reads from KEYBOARD_DATA_PORT, prints to screen last key
 *                 pressed. keyboard status variable updated and terminal may be
 8                  switched
 */
void keyboard_handler_33() {
    // printf("keyboard interrupt\n");
    disable_irq(IRQ_1);
    send_eoi(IRQ_1);
    uint8_t curr_key = inb(KEYBOARD_DATA_PORT);

    if (keyboard_state.control_on && curr_key == C_PRESSED) {
        current_pcb[active_terminal_idx]->signal_flag[INTERRUPT] = SIGNAL_PENDING;
        enable_irq(IRQ_1);
        return;
    }

    /* skip scan code bytes that are headers only */
    if(curr_key != KEY_RELEASE_PREFIX && curr_key != ACK)
        keyboard_state.last_key = curr_key;        

    /* update internal status of keyboard keys */
    if (keyboard_state.last_key == CAPS_LOCK) {
        /* toggle the state of caps lock, since the release and pressed 
        keycodes are the same*/
        if(keyboard_state.caps_lock_on == true) {
            keyboard_state.caps_lock_on = false;
        } 
        else if(keyboard_state.caps_lock_on == false) {
            keyboard_state.caps_lock_on = true;
        }
    } 

    else if (keyboard_state.last_key == CONTROL_PRESSED)
        keyboard_state.control_on = true;

    else if (keyboard_state.last_key == CONTROL_RELEASED)
        keyboard_state.control_on = false;

    else if ((keyboard_state.last_key == LEFT_SHIFT_PRESSED)|| 
             (keyboard_state.last_key == RIGHT_SHIFT_PRESSED))
        keyboard_state.shift_on = true;

    else if ((keyboard_state.last_key == LEFT_SHIFT_RELEASED)|| 
             (keyboard_state.last_key == RIGHT_SHIFT_RELEASED))
        keyboard_state.shift_on = false;

    else if (keyboard_state.last_key == ALT_PRESSED)
        alt_on = true;

    else if (keyboard_state.last_key == ALT_RELEASED)
        alt_on = false;

    else if (alt_on && keyboard_state.last_key==F1_PRESSED) {
        enable_irq(IRQ_1);
        switch_terminal(TERM_0);
        //printf("switch to terminal 1\n");
    } 

    else if (alt_on && keyboard_state.last_key==F2_PRESSED) {
        enable_irq(IRQ_1);
        switch_terminal(TERM_1);
        // printf("switch to terminal 2\n");
    } 
    
    else if (alt_on && keyboard_state.last_key==F3_PRESSED) {
        enable_irq(IRQ_1);
        switch_terminal(TERM_2);
        //printf("switch to terminal 3\n");
    }

    /* terminal handles line editing */
    keyboard_to_terminal(keyboard_state);

    enable_irq(IRQ_1);
}

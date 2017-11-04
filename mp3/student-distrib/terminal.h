#ifndef _TERMINAL_H
#define _TERMINAL_H

#include "types.h"
#include "syscall.h"
#include "task.h"

/* constants for the ttys */
#define BUFFER_LEN 128
#define VIDEO_MEM_SIZE (1<<12) 

#define TERM_0 0
#define TERM_1 1
#define TERM_2 2

/* offsets for keymaps */
#define FIRST_KEYS 0x3B // shift
#define SECOND_KEYS 0x3B*2 // caps
#define THIRD_KEYS 0x3B*3 // caps + shift
#define NEW_FIRST_KEYS (FIRST_KEYS * 4) /* size of scancode map */

#define VIDEO_BUFFER_BASE 0xB9000

/*testing */
#define EVERY_EIGHT_KEYSTROKES 8

typedef struct keyboard_state_n {
	uint8_t last_key;
	uint8_t caps_lock_on;
	uint8_t control_on;
	uint8_t shift_on;
} keyboard_state_t;

typedef struct terminal_state_n {
	uint32_t open;
	uint32_t buffer_location;
	uint32_t buffer_end_location;
	uint32_t read_location;
	uint32_t enter_pressed;
	int32_t cursor_y;
	int32_t cursor_x;
	uint8_t buffer[BUFFER_LEN];
	int8_t* video_buffer;
} terminal_state_t;

extern int32_t active_terminal_idx;

extern void init_terminal();
extern int32_t terminal_open (const uint8_t* fname);
extern int32_t terminal_close (int32_t fd);
extern int32_t terminal_read (int32_t fd, void* buf, int32_t nbytes);
extern int32_t terminal_write (int32_t fd, const void* buf, int32_t nbytes);
extern void keyboard_to_terminal (keyboard_state_t keyboard_state);
extern int8_t* get_terminal_vid_buffer(uint32_t terminal_idx);
extern void save_screen();
extern void restore_screen(uint32_t terminal_idx);
extern void switch_terminal (uint32_t terminal_idx);

extern void test_terminal_read();
extern void test_terminal_write();

#endif /* _TERMINAL_H */

#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include "types.h"

//see c files for more
extern void init_keyboard ();
extern void keyboard_handler_33();
extern int32_t keyboard_read (int32_t fd, void* buf, int32_t nbytes);
extern int32_t keyboard_write (int32_t fd, const void* buf, int32_t nbytes);
extern int32_t keyboard_open (const uint8_t* fname);
extern int32_t keyboard_close (int32_t fd);

#endif /* _KEYBOARD_H */

#ifndef _PIT_H
#define _PIT_H

#include "types.h"
#include "x86_desc.h"
#include "terminal.h"

//see c file for more
extern void init_pit ();
extern void pit_handler_32();
extern void pit_change_freq(uint16_t freq);


#endif /* _PIT_H */

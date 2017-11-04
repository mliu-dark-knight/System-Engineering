#ifndef _IDT_H
#define _IDT_H

#include "types.h"
#include "x86_desc.h"

//init idt, see c file for more
extern void init_idt(idt_desc_t* idt);

#endif /* _IDT_H */

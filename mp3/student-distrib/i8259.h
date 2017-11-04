/* i8259.h - Defines used in interactions with the 8259 interrupt
 * controller
 * vim:ts=4 noexpandtab
 */

#ifndef _I8259_H
#define _I8259_H

#include "types.h"

/* Ports that each PIC sits on */
#define MASTER_8259_PORT 0x20
#define MASTER_8259_DATA (MASTER_8259_PORT + 1)
#define SLAVE_8259_PORT  0xA0
#define SLAVE_8259_DATA  (SLAVE_8259_PORT + 1)

/* Initialization control words to init each PIC.
 * See the Intel manuals for details on the meaning
 * of each word */
#define ICW1    	  0x11
#define ICW2_MASTER   0x20
#define ICW2_SLAVE    0x28
#define ICW3_MASTER   0x04
#define ICW3_SLAVE    0x02
#define ICW4          0x01

/* End-of-interrupt byte.  This gets OR'd with
 * the interrupt number and sent out to the PIC
 * to declare the interrupt finished */
#define EOI             0x60

/* constant for C functions to access IRQ 0 - PIT */
#define IRQ_0			0
/* bitmask to access master IR0 */
#define IRQ_0_MASK		(0x01 << 0)

/* constant for C functions to access IRQ 1 - keyboard */
#define IRQ_1			1
/* bitmask to access master IR2 */
#define IRQ_1_MASK		(0x01 << 1)

/* constant for C functions to access IRQ 2 - slave PIC */
#define IRQ_2			2
/* bitmask to access master IR2 */
#define IRQ_2_MASK		(0x01 << 2)

/* constant for C functions to access IRQ 8 - RTC */
#define IRQ_8			8
/* bitmask to access slave IR0 */
#define IRQ_8_MASK		(0x01 << 8)


/* Externally-visible functions */

/* Initialize both PICs */
void i8259_init(void);
/* Enable (unmask) the specified IRQ */
void enable_irq(uint32_t irq_num);
/* Disable (mask) the specified IRQ */
void disable_irq(uint32_t irq_num);
/* Send end-of-interrupt signal for the specified IRQ */
void send_eoi(uint32_t irq_num);

#endif /* _I8259_H */

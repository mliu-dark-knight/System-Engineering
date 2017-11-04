/* i8259.c - Functions to interact with the 8259 interrupt controller
 * vim:ts=4 noexpandtab

 */

#include "i8259.h"
#include "lib.h"

/* IRQ 8-15 are on slave IR 0-7 */
#define IRQ_NUM_SLAVE_OFFSET 8

/* mask all except IR2 so slave to master comm is already enabled */
#define MASTER_MASK_VALUE 0xFB

/* mask all interrupts on slave */
#define SLAVE_MASK_VALUE 0xFF

/*
 * delay_for_PIC
 *   DESCRIPTION: wait for IO
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: wait for IO
 */
static void delay_for_PIC(void)
{
    asm volatile ("nop" :::"memory");
}


/* Initialize the 8259 PIC */
/*
 * i8259_init
 *   DESCRIPTION: initialize 8259 PIC
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: PIC is initialized, Master/slave data/addr port accessed
 */
void
i8259_init(void)
{
    /* ICW1 */
    outb(ICW1, MASTER_8259_PORT);
    delay_for_PIC(); 
    /* wait, since PIC is asychronous and much slower than CPU */
    outb(ICW1, SLAVE_8259_PORT);
    delay_for_PIC();

    /* ICW2 */
    outb(ICW2_MASTER, MASTER_8259_DATA);
    delay_for_PIC();
    outb(ICW2_SLAVE, SLAVE_8259_DATA);
    delay_for_PIC();

    /* ICW3 */
    outb(ICW3_MASTER, MASTER_8259_DATA);
    delay_for_PIC();
    outb(ICW3_SLAVE, SLAVE_8259_DATA);
    delay_for_PIC();

    /* ICW 4 */
    outb(ICW4, MASTER_8259_DATA);
    delay_for_PIC();
    outb(ICW4, SLAVE_8259_DATA);
    delay_for_PIC();

    /* mask all execpt IRQ2 */
    outb(MASTER_MASK_VALUE, MASTER_8259_DATA);
    delay_for_PIC();
    outb(SLAVE_MASK_VALUE, SLAVE_8259_DATA);
    delay_for_PIC();

}


/* Enable (unmask) the specified IRQ */
/*
 * enable_irq
 *   DESCRIPTION: unmask the input irq
 *   INPUTS: irq_num: which irq to be unmasked
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: irq unmasked - IMR of master OR slave modified
 */
void
enable_irq(uint32_t irq_num)
{
    uint16_t port;
    uint8_t value;

    if (irq_num < IRQ_NUM_SLAVE_OFFSET) { //IRQ is IRx on master
        port = MASTER_8259_DATA;
    } else { //IRQ is IR2 on master
        port = SLAVE_8259_DATA;
        irq_num = irq_num - IRQ_NUM_SLAVE_OFFSET;
        /* IRQ 8-15 are on slave IR 0-7 */
    }

    value = inb(port) & ~(1 << irq_num);  //set bit in IMR to zero
    outb(value, port);
}


/* Disable (mask) the specified IRQ */
/*
 * disable_irq
 *   DESCRIPTION: mask irq
 *   INPUTS: irq_num: which irq to be masked
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: irq masked - IMR of master OR slave modified
 */
void
disable_irq(uint32_t irq_num)
{
    uint16_t port;
    uint8_t value;

    if (irq_num < IRQ_NUM_SLAVE_OFFSET) { //master
        port = MASTER_8259_DATA;
    } else { //slave
        port = SLAVE_8259_DATA;
        irq_num = irq_num - IRQ_NUM_SLAVE_OFFSET; 
        /* IRQ 8-15 are on slave IR 0-7 */
    }

    value = inb(port) | (1 << irq_num);   //set bit in IMR to one
    outb(value, port);
}


/* Send end-of-interrupt signal for the specified IRQ */
/*
 * send_eoi
 *   DESCRIPTION: send End of Interrupt
 *   INPUTS: irq_num: which irq to be sent
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: EOI is sent to the irq
 */
void
send_eoi(uint32_t irq_num)
{
    if (irq_num < IRQ_NUM_SLAVE_OFFSET) {
        outb(EOI|irq_num, MASTER_8259_PORT);
    } else { /* IRQ 8-15 are on slave IR 0-7 */
        outb(EOI|(irq_num-IRQ_NUM_SLAVE_OFFSET), SLAVE_8259_PORT);
        //need to EOI master to slave comm line also
        outb(EOI|IRQ_2, MASTER_8259_PORT);
    }
}

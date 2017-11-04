#include "pit.h"
#include "lib.h"
#include "i8259.h"
#include "task.h"
#include "paging.h"
#include "x86_desc.h"
#include "terminal.h"

/* PIT port/register constants */
#define PIT_CHAN_0_PORT	0x40
#define PIT_CMD_PORT	0x43

/* bitmasks to set approriate settings for periodic interrupts */
#define MODE_2 0x02
#define HIGH_BYTE 8
/* send lsb then msb */
#define CMD_CHAN_0_BOTH_BYTES 0x03

/* bitmasks to set clock divider to get correct frequency */
// 20000 * (1/1.1931816666MHz) = 16.76ms
const uint16_t TICKS_16_MS = 50000;

/*
 * init_pit
 *   DESCRIPTION: init programmable interval timer
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: pit's irq is enabled, PIT set to Mode 2 - periodic single pulse 
 *				   interrupts
 */
void init_pit () {
	disable_irq (IRQ_0); /* so reinitalization also works */
	/* mask interrupts so no handler gets called while
	settings are changed */

	/* initalize to 8ms periodic interrupts*/
	pit_change_freq(TICKS_16_MS);

	enable_irq (IRQ_0);
}

/*
 * pit_handler_32
 *   DESCRIPTION: pit interrupt handler - does context switch in round 
 *				  robin fashion
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: may change the currently running task.
 */
void pit_handler_32() {
	// avoid nesting pit handlers
	disable_irq(IRQ_0);
	send_eoi(IRQ_0);

	//save current stack ptr to pcb
	asm volatile(
		"movl %%ebp, %0;"
		"movl %%esp, %1;"
		: "=r"(current_pcb[active_task_idx]->ebp), "=r"(current_pcb[active_task_idx]->esp)
		: /* no inputs */
		: "cc");

	//regs already saved by handler, do not save registers here
	//save page tables of current process to pcb
	update_page_directory((get_cur_pid() + 1));

	//always run the task of the next terminal
	active_task_idx = (active_task_idx + 1) % NUM_TERMINAL;
	//ASSUMES at least ONE task is active!!!

	//registers restored by handler, do not resotre registers here

	//restore kmode stack ptr to new process
	//load new page tables
	update_page_directory((get_cur_pid() + 1));

	//no need to check if active_terminal_idx == active_task_idx

	//reload tss with the correct k stack values
	tss.ss0 = KERNEL_DS;
	tss.esp0 = KERNEL_END - (get_cur_pid() * 8 * KILO) - KMODE_STACK_OFFSET;

	//restore the new process's kernel stack context, iret will restore registers
	asm volatile(
		"movl %0, %%esp;"
		"movl %1, %%ebp;"
		: /* no outputs */
		: "r"(current_pcb[active_task_idx]->esp), "r"(current_pcb[active_task_idx]->ebp)
		: "cc");

	enable_irq(IRQ_0);

	//tear down stack frame and automatically pop all registers, then return 
	//to the interrupt handler wrapper to restore all registers
	// this avoids complications with local variables spread across on two 
	// kernel stacks
	asm volatile(
		"leave;"
		"ret;");
}


/*
 * pit_change_freq
 *   DESCRIPTION: change the frequency of the pit
 *   INPUTS: freq: the new frequency
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: the frequency of the pit changed
 */
void pit_change_freq(uint16_t freq) {
	disable_irq (IRQ_0); /* so reinitalization also works */

	/* set divisor to get 8ms interrupts */
	outb(CMD_CHAN_0_BOTH_BYTES|MODE_2, PIT_CMD_PORT);
	outb((TICKS_16_MS & LOW_BYTE_MASK), PIT_CHAN_0_PORT);
	outb((TICKS_16_MS >> HIGH_BYTE), PIT_CHAN_0_PORT);

	enable_irq (IRQ_0);
}

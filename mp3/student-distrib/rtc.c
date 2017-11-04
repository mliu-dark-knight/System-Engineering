#include "rtc.h"
#include "lib.h"
#include "i8259.h"
#include "terminal.h"
#include "task.h"

/* RTC port/register constants */
#define RTC_ADDR_PORT 0x70
#define RTC_DATA_PORT 0x71
#define RTC_REG_A 0x0A
#define RTC_REG_B 0x0B
#define RTC_REG_C 0x0C

/* bitmasks to set approriate settings for periodic interrupts with 32768 base 
clk */
#define DV_BIT_FOR_32768_HZ 0x20 /* PC standard is to use 32768Hz base clk */
#define REG_B_UPDATE_EN 0x80
#define REG_B_INTR_EN 0x40

#define DEFAULT_FREQ_HZ 1024
#define DEFAULT_FREQ_HZ_USER 2
/* bitmasks to set clock divider to get correct frequency */
#define BV_BIT_FOR_1024HZ 0x6
#define BV_BIT_FOR_512HZ 0x7
#define BV_BIT_FOR_256HZ 0x8
#define BV_BIT_FOR_128HZ 0x9
#define BV_BIT_FOR_64HZ 0xA
#define BV_BIT_FOR_32HZ 0xB
#define BV_BIT_FOR_16HZ 0xC
#define BV_BIT_FOR_8HZ 0xD
#define BV_BIT_FOR_4HZ 0xE
#define BV_BIT_FOR_2HZ 0xF

/* specified in Appendix B of MP3 spec - user program limited to max 1024HZ
interrupts to avoid DOSing*/
#define USER_MAX_FREQ_HZ 1024
#define ALARM_SIG_FREQ_HZ 16

/* struct to hold current RTC status */
typedef struct rtc_status {
	uint32_t curr_freq_hz; /* current RTC frequency in HZ */
	uint32_t rtc_alarm_ctr; /* counter for the alarm signal */
} rtc_status_t;

static rtc_status_t rtc_stat = {
	.curr_freq_hz = 0, //default to 1024hz, changed in rtc_init
	.rtc_alarm_ctr = 0
};

static int32_t rtc_change_freq_hz(int32_t freq);
static int32_t task_rtc_change_freq_hz(int32_t freq);


/*
 * rtc_open
 *   DESCRIPTION: open the RTC
 *   INPUTS: fname - unused - open in syscall.c validates that the RTC has been
 *			 opened by the current process
 *   OUTPUTS: none
 *   RETURN VALUE: always succeeds and returns 0
 *   SIDE EFFECTS: update the rtc fields in the current pcb approriately
 */
int32_t rtc_open (const uint8_t* fname) {
	/* no task is actually running */
	if(current_pcb[active_task_idx] == NULL) {
		return ERR;
	}
	
	int32_t flags;
	cli_and_save(flags);
	current_pcb[active_task_idx]->rtc_interrupt_occurred = false;
	/* initalize to the first interrupt has occurred already to prevent a 
	spurious printf of rtc read being unblocked by interrupt handler */
	current_pcb[active_task_idx]->rtc_counter = 0;
	/* reset the user level interrupt counter */

	/* default users to 2hz */
	current_pcb[active_task_idx]->rtc_freq = DEFAULT_FREQ_HZ_USER;
	current_pcb[active_task_idx]->using_rtc = true;
	restore_flags(flags);
	return SUCCESS;
}

/*
 * rtc_close
 *   DESCRIPTION: close the RTC
 *   INPUTS: fd - unused - open in syscall.c validates that the RTC has been
 *			 opened by the current process
 *   OUTPUTS: none
 *   RETURN VALUE: always succeeds and returns 0
 *   SIDE EFFECTS: update the rtc fields in the current pcb approriately
 */
int32_t rtc_close(int32_t fd) {
	/* no task is actually running */
	if(current_pcb[active_task_idx] == NULL) {
		return ERR;
	}

	int32_t flags;
	cli_and_save(flags);

	/* disable updating the rtc counter */
	current_pcb[active_task_idx]->using_rtc = false;
	restore_flags(flags);
	return SUCCESS;
}

/*
 * rtc_read
 *   DESCRIPTION: read from the RTC. Blocks until an interrupt occurs
 *   INPUTS: fd: unused - open in syscall.c validates that the RTC has been
 *			 opened by the current process
 *           buf: destination of the data
 *           nbytes: length in bytes to be read
*			 Both buf and nbytes are unused.
 *   OUTPUTS: none
 *   RETURN VALUE: always returns 0 for success, but blocks until an interrupt 
 *				   occurs
 *   SIDE EFFECTS: none, but depends on the interrupt handler updating a flag.
 */
int32_t rtc_read(int32_t fd, void* buf, int32_t nbytes) {
	/* no task is actually running */
	if(current_pcb[active_task_idx] == NULL) {
		return ERR;
	}

	/* this is a critical section since rtc_stat.interrupt_occurred is also 
	modified by the interrupt handler */
	int32_t flags;
	cli_and_save(flags);
	current_pcb[active_task_idx]->rtc_interrupt_occurred = false;
	restore_flags(flags);

	/* block until an interrupt occurs */
	while(true) {
		cli_and_save(flags);
		if(current_pcb[active_task_idx]->rtc_interrupt_occurred == true) {
			restore_flags(flags);
			break;
		}
		restore_flags(flags);
	};

	return SUCCESS;
}

/*
 * rtc_write
 *   DESCRIPTION: write to the RTC. Only allow 4 bytes to be written at a time.
 * 				  The four bytes are treated as a int32_t specifying the desired 
 *				  RTC interrupt frequency.
 *   INPUTS: fd: unused - open in syscall.c validates that the RTC has been
 *			 opened by the current process
 *           buf: source of the data - treated as a int32_t* with the frequency
 *			 in HZ of the desired RTC interrupt frequency. See
 *			 rtc_change_freq_hz for allowed values.
 *           nbytes: length in bytes to be written - must be 4
 *   OUTPUTS: none
 *   RETURN VALUE: 4 (num. of bytes written) for success, -1 for fail if nbytes 
 *				   was not 4, if buf was NULL, or if the value specified by buf
 *				   was invalid
 *   SIDE EFFECTS: RTC interrupt frequency changed for the task */
int32_t rtc_write(int32_t fd, const void* buf, int32_t nbytes) {
	/* no task is actually running */
	if(current_pcb[active_task_idx] == NULL) {
		return ERR;
	}

	/* validate input is correct */
	if(buf == NULL || nbytes != sizeof(int32_t)) {
		return ERR;
	}

	int32_t flags;
	cli_and_save(flags);

	int32_t freq = *((int32_t*)buf);

	if(task_rtc_change_freq_hz(freq) == SUCCESS) {
		/* the change to RTC succeeded, return the number of bytes written */
		restore_flags(flags);
		return sizeof(int32_t);
	} else {
		/* freq was an invalid frequency */
		restore_flags(flags);
		return ERR;
	}
}


/*
 * rtc_handler_40
 *   DESCRIPTION: rtc interrupt handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: Reads from RTC reg C, writes to RTC addr port. RTC state
 *				   changes to continue generating interrupts. Modifies
 *				   rtc_stat.interrupt_ocurred and causes pending rtc_reads to
 *				   unblock and return. updates the counter field for each user
 *				   program and triggers an interrupt when approriate
 */
void rtc_handler_40() {
    disable_irq(IRQ_8);
    send_eoi(IRQ_8);

    /* read reg C to ack this interrupt and tell RTC to continue generating
    interrupts */
    outb(RTC_REG_C, RTC_ADDR_PORT);
    inb(RTC_DATA_PORT);

    int32_t i;
    for (i = 0; i < NUM_TERMINAL; ++i)
    {	
    	/* don't update the counter if there is no active task on a terminal, 
    	or if that task is not using the rtc */
    	if(current_pcb[i] == NULL || current_pcb[i]->flag == TASK_NOT_PRESENT ||
    		current_pcb[i]->using_rtc == false) continue;
    	current_pcb[i]->rtc_counter++;

    	/* we have reached the correct number of counts, send a user level interrupt */
    	if(current_pcb[i]->rtc_counter >= (DEFAULT_FREQ_HZ / current_pcb[i]->rtc_freq)) {
    		current_pcb[i]->rtc_counter = 0;
    		current_pcb[i]->rtc_interrupt_occurred = true;
    	}
    }

    rtc_stat.rtc_alarm_ctr++;
    if(rtc_stat.rtc_alarm_ctr >= (DEFAULT_FREQ_HZ * ALARM_SIG_FREQ_HZ)) {
    	//disable rtc signal for now
        current_pcb[active_task_idx]->signal_flag[ALARM] = SIGNAL_PENDING;
        rtc_stat.rtc_alarm_ctr = 0;
    }

    enable_irq(IRQ_8);
}


/*
 * task_rtc_change_freq_hz
 *   DESCRIPTION: Changes the RTC interrupt frequency for each task. Allowed frequencies are
 *				  powers of 2 between 2 to 1024, inclusive. DOES NOT CHANGE THE real RTC 
 *				  frequency
 *   INPUTS: freq - the desired frequency in Hz
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: writes to the rtc_freq field of the current pcb.
 */
static int32_t task_rtc_change_freq_hz(int32_t freq) {
	disable_irq (IRQ_8); /* so reinitalization also works */

	int32_t retval = SUCCESS;

	/* specified in Appendix B of MP3 spec - user program limited to max 1024HZ
	interrupts to avoid DOSing */
	if(freq > USER_MAX_FREQ_HZ) {
		retval = ERR;
	}

	/* look up the correct bits to set in Reg A to correctly setup the clock 
	frequency divider */
	uint8_t divisor;
	switch (freq) {
		case 1024:
			divisor = BV_BIT_FOR_1024HZ;
			break;
		case 512:
			divisor = BV_BIT_FOR_512HZ;
			break;
		case 256:
			divisor = BV_BIT_FOR_256HZ;
			break;
		case 128:
			divisor = BV_BIT_FOR_128HZ;
			break;
		case 64:
			divisor = BV_BIT_FOR_64HZ;
			break;
		case 32:
			divisor = BV_BIT_FOR_32HZ;
			break;
		case 16:
			divisor = BV_BIT_FOR_16HZ;
			break;
		case 8:
			divisor = BV_BIT_FOR_8HZ;
			break;
		case 4:
			divisor = BV_BIT_FOR_4HZ;
			break;
		case 2:
			divisor = BV_BIT_FOR_2HZ;
			break;
		default: /* not an allowed frequency */
			retval = ERR;
			break;
	}

	/* if input frequency was valid i.e. was a power of 2 between 2Hz and
	1024Hz, inclusive */
	if(retval != ERR) {
		current_pcb[active_task_idx]->rtc_freq = freq;
	}

	enable_irq (IRQ_8);
	return retval;
}


/*
 * rtc_change_freq_hz
 *   DESCRIPTION: Changes the RTC interrupt frequency. Allowed frequencies are
 *				  powers of 2 between 2 to 1024, inclusive.
 *   INPUTS: freq - the desired frequency in Hz
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: Writes to RTC reg A, writes to RTC addr/data port.
 */
static int32_t rtc_change_freq_hz(int32_t freq) {
	disable_irq (IRQ_8); /* so reinitalization also works */

	int32_t retval = SUCCESS;

	/* specified in Appendix B of MP3 spec - user program limited to max 1024HZ
	interrupts to avoid DOSing */
	if(freq > USER_MAX_FREQ_HZ) {
		retval = ERR;
	}

	/* look up the correct bits to set in Reg A to correctly setup the clock 
	frequency divider */
	uint8_t divisor;
	switch (freq) {
		case 1024:
			divisor = BV_BIT_FOR_1024HZ;
			break;
		case 512:
			divisor = BV_BIT_FOR_512HZ;
			break;
		case 256:
			divisor = BV_BIT_FOR_256HZ;
			break;
		case 128:
			divisor = BV_BIT_FOR_128HZ;
			break;
		case 64:
			divisor = BV_BIT_FOR_64HZ;
			break;
		case 32:
			divisor = BV_BIT_FOR_32HZ;
			break;
		case 16:
			divisor = BV_BIT_FOR_16HZ;
			break;
		case 8:
			divisor = BV_BIT_FOR_8HZ;
			break;
		case 4:
			divisor = BV_BIT_FOR_4HZ;
			break;
		case 2:
			divisor = BV_BIT_FOR_2HZ;
			break;
		default: /* not an allowed frequency */
			retval = ERR;
			break;
	}

	/* if input frequency was valid i.e. was a power of 2 between 2Hz and
	1024Hz, inclusive */
	if(retval != ERR) {
		/* set base clk of 32768HZ, set divisor to get freq Hz interrupts */
		outb(RTC_REG_A, RTC_ADDR_PORT);
		outb((DV_BIT_FOR_32768_HZ|divisor), RTC_DATA_PORT);
	}

	enable_irq (IRQ_8);
	return retval;
}

/*
 * init_rtc
 *   DESCRIPTION: init real time clock
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: RTC's irq is masked for the duration of the function and at
 *				   the end, enabled, RTC register A/B modified, writes to
 *				   RTC addr/data port
 */
void init_rtc () {
	disable_irq (IRQ_8); /* so reinitalization also works */
	/* mask interrupts so no handler gets called while
	settings are changed */

	/* initalize to 2hz periodic interrupts - according to mp3.2 spec */
	rtc_change_freq_hz(DEFAULT_FREQ_HZ);
	rtc_stat.curr_freq_hz = DEFAULT_FREQ_HZ;

	outb(RTC_REG_B, RTC_ADDR_PORT);
	/* read-modify-write to keep other flags in RTC reg B unchanged */
	uint8_t data_reg_b_temp;
	data_reg_b_temp = inb(RTC_DATA_PORT);
	/* start the RTC and enable interrupt generation */
	outb((data_reg_b_temp|REG_B_INTR_EN|REG_B_UPDATE_EN), RTC_DATA_PORT);

	enable_irq (IRQ_8);
}

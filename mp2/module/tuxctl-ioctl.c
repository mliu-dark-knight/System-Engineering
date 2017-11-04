/* tuxctl-ioctl.c
 *
 * Driver (skeleton) for the mp2 tuxcontrollers for ECE391 at UIUC.
 *
 * Mark Murphy 2006
 * Andrew Ofisher 2007
 * Steve Lumetta 12-13 Sep 2009
 * Puskar Naha 2013
 */

#include <asm/current.h>
#include <asm/uaccess.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/miscdevice.h>
#include <linux/kdev_t.h>
#include <linux/tty.h>
#include <linux/spinlock.h>

#include "tuxctl-ld.h"
#include "tuxctl-ioctl.h"
#include "mtcp.h"

#define debug(str, ...) \
	printk(KERN_DEBUG "%s: " str, __FUNCTION__, ## __VA_ARGS__)

/************************ Protocol Implementation *************************/

static unsigned long orig_led_info;
static int button_info = 0x000000FF;
static spinlock_t button_lock;
static unsigned long button_flag;
static int led_usr_ack;
static int led_set_ack;
static spinlock_t led_lock;
static unsigned long led_flag;

int tuxctl_init(struct tty_struct* tty);
int tuxctl_set_led(struct tty_struct* tty, unsigned long arg);
int tuxctl_set_button(unsigned arg1, unsigned arg2);
int tuxctl_send_button(unsigned long arg);

/* tuxctl_handle_packet()
 * IMPORTANT : Read the header for tuxctl_ldisc_data_callback() in 
 * tuxctl-ld.c. It calls this function, so all warnings there apply 
 * here as well.
 */
void tuxctl_handle_packet (struct tty_struct* tty, unsigned char* packet)
{
    unsigned a, b, c;

    a = packet[0]; /* Avoid printk() sign extending the 8-bit */
    b = packet[1]; /* values when printing them. */
    c = packet[2];

    // printk("packet : %x %x %x\n", a, b, c);
    switch (a) {
        case MTCP_ACK:
            spin_lock_irqsave(&(led_lock), led_flag);
            if (!led_usr_ack)
            	led_usr_ack = 1;
            else
            	led_set_ack = 1;
            spin_unlock_irqrestore(&(led_lock), led_flag);
            break;
    	case MTCP_RESET:
            // printk("device reset\n");
            tuxctl_init(tty);
            if (!led_usr_ack || !led_set_ack)
                return;
            tuxctl_set_led(tty, orig_led_info);
            break;
    	case MTCP_BIOC_EVENT:
            tuxctl_set_button(b, c);
            break;
    	default:
    		return;
    }
    /*printk("packet : %x %x %x\n", a, b, c); */
}

/******** IMPORTANT NOTE: READ THIS BEFORE IMPLEMENTING THE IOCTLS ************
 *                                                                            *
 * The ioctls should not spend any time waiting for responses to the commands *
 * they send to the controller. The data is sent over the serial line at      *
 * 9600 BAUD. At this rate, a byte takes approximately 1 millisecond to       *
 * transmit; this means that there will be about 9 milliseconds between       *
 * the time you request that the low-level serial driver send the             *
 * 6-byte SET_LEDS packet and the time the 3-byte ACK packet finishes         *
 * arriving. This is far too long a time for a system call to take. The       *
 * ioctls should return immediately with success if their parameters are      *
 * valid.                                                                     *
 *                                                                            *
 ******************************************************************************/
int 
tuxctl_ioctl (struct tty_struct* tty, struct file* file, 
	      unsigned cmd, unsigned long arg)
{
    switch (cmd) {
	case TUX_INIT:
		return tuxctl_init(tty);
	case TUX_BUTTONS:
        return tuxctl_send_button(arg);
	case TUX_SET_LED:
        // printk("led_usr_ack = %d\n", led_usr_ack);
        if (!led_usr_ack || !led_set_ack)
            return 0;
        return tuxctl_set_led(tty, arg);
	case TUX_LED_ACK:
        return 0;
	case TUX_LED_REQUEST:
        return 0;
	case TUX_READ_LED:
        return 0;
	default:
	    return -EINVAL;
    }
}

/*
 * tuxctl_init
 *   DESCRIPTION: send MTCP_BIOC_ON and MTCP_LED_USR command to tux controller
 *   INPUTS:
 *   OUTPUTS: none
 *   RETURN VALUE: always return 0
 *   SIDE EFFECTS: clear led_usr_ack, set led_set_ack to 1, set button_info to empty
 */   
int tuxctl_init(struct tty_struct* tty) {
    unsigned char bioc_on = MTCP_BIOC_ON;
    unsigned char bioc_usr = MTCP_LED_USR;
    /* send MTCP_BIOC_ON and MTCP_LED_USR command to tux controller */
    tuxctl_ldisc_put(tty, &bioc_on, 1);
    tuxctl_ldisc_put(tty, &bioc_usr, 1);
    spin_unlock(&(led_lock));
    spin_unlock(&(button_lock));
    /* clear led_usr_ack, set led_set_ack to 1 */
    spin_lock_irqsave(&(led_lock), led_flag);
    led_usr_ack = 0;
    led_set_ack = 1;
    spin_unlock_irqrestore(&(led_lock), led_flag);
    /* set button_info to empty */
    spin_lock_irqsave(&(button_lock), button_flag);
    button_info = 0x000000FF;
    spin_unlock_irqrestore(&(button_lock), button_flag);
    return 0;
}

/*
 * tuxctl_set_led
 *   DESCRIPTION: send MTCP_LED_SET command and five extra arguments to tux controller
 *   INPUTS: tty
 *           arg -- information containing which led to set, number to be displayed and deciaml points
 *   OUTPUTS: none
 *   RETURN VALUE: always return 0
 *   SIDE EFFECTS: send MTCP_LED_SET command and five extra arguments to tux controller
 */   
int tuxctl_set_led(struct tty_struct* tty, unsigned long arg) {
    static unsigned char segment_map[16] = {0xE7, 0X06, 0XCB, 0X8F,
                                            0X2E, 0XAD, 0XED, 0X86,
                                            0XEF, 0XAF, 0XEE, 0X6D,
                                            0XE1, 0X4F, 0XE9, 0XE8};

    static unsigned long display_mask[4] = {0x0000000F, 0x000000F0, 0x00000F00, 0x0000F000};

    unsigned char number_to_display;
    unsigned int on_LED = (arg & (0x000F<<16)) >> 16;
    unsigned int decimal_points = (arg & (0x0F<<24)) >> 24;
    unsigned char bit_mask = 0x01;
    unsigned char dp_mask = 0x10;
    int i;

    unsigned char message_to_tux[6];

    // printk("led_set_ack = %d\n", led_set_ack);

    /* clear led_set_ack, wait until MTCP_ACK to set back to 1 */
    spin_lock_irqsave(&(led_lock), led_flag);
    led_set_ack = 0;
    spin_unlock_irqrestore(&(led_lock), led_flag);

    message_to_tux[0] = MTCP_LED_SET;
    /* second opcode to set all four LED's */
    message_to_tux[1] = 0x0F;

    /* set the remaining four messages */
    for (i=0; i<4; i++) {
        if ((on_LED>>i) & bit_mask) {
            number_to_display = segment_map[(arg & display_mask[i]) >> (4*i)];
            if ((decimal_points>>i) & bit_mask)
                message_to_tux[2+i] = number_to_display | dp_mask;
            else
                message_to_tux[2+i] = number_to_display;
        }
        else
            message_to_tux[2+i]=0x00;
    }

    tuxctl_ldisc_put(tty, message_to_tux, 6);
    orig_led_info = arg;
    return 0;
}

/*
 * tuxctl_set_button
 *   DESCRIPTION: save button info, do necessary bit shift
 *   INPUTS: arg1 -- button pressed
 *           arg2 -- button pressed
 *   OUTPUTS: none
 *   RETURN VALUE: always return 0
 *   SIDE EFFECTS: save button info, do necessary bit shift
 */   
int tuxctl_set_button(unsigned arg1, unsigned arg2) {
    unsigned mask = 0x000F;
    unsigned old_left, old_down, new_left, new_down;

    /* switch left and down */
    old_left = (arg2 & 0x02) << 4;
    old_down = (arg2 & 0x04) << 4;
    new_left = old_left << 1;
    new_down = old_down >> 1;
    spin_lock_irqsave(&(button_lock), button_flag);
    button_info = ((arg2 & mask) << 4) | (arg1 & mask);
    button_info &= 0x9F;
    button_info |= new_left;
    button_info |= new_down;
    spin_unlock_irqrestore(&(button_lock), button_flag);
    return 0;
}

/*
 * tuxctl_send_button
 *   DESCRIPTION: copy button info to user space
 *   INPUTS: arg -- user space pointer
 *   OUTPUTS: none
 *   RETURN VALUE: return 0 if copy successfully, -EFAULT otherwise
 *   SIDE EFFECTS: copy button info to user space
 */   
int tuxctl_send_button(unsigned long arg) {
    int message = button_info & 0x000000FF;
    int result;
    /* copy button info to user space */
    spin_lock_irqsave(&(button_lock), button_flag);
    result = copy_to_user((void*)arg, (void*)&message, sizeof(int));
    spin_unlock_irqrestore(&(button_lock), button_flag);
    return result? 0:-EFAULT;
}

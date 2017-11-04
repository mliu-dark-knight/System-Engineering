#include "idt.h"
#include "lib.h"
#include "x86_desc.h"

#include "idt_handler.h"
#include "interrupt.h"

/* constants to add specific handlers */
#define PIT_ENTRY       0x20
#define RTC_ENTRY       0x28
#define KEYBOARD_ENTRY  0x21
#define SYS_CALL_ENTRY  0x80

/* IDT loop constants - for different entry types */
#define EXCEPTION_NUM 32
#define IDT_LEN 256

/* constants for the IDT entries */
#define INT_GATE_32BIT 0xE
#define TRAP_GATE_32BIT 0xF
#define DPL_KERNEL 0
#define DPL_USER 3

#define HANDLER_PRESENT 1
#define HANDLER_ABSENT 0

/*
 * init_idt
 *   DESCRIPTION: init Interrupt Descriptor Table
 *   INPUTS: idt: pointer to a function pointer array with pointers to the
 *           interrupt/exception handler functions
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: idt is initialized with valid values. LIDT not done yet.
 */
void init_idt(idt_desc_t* idt) {
    /* fill IDT with default, valid settings */
    int i;
    for (i = 0; i < EXCEPTION_NUM; ++i) { //exceptions
        /* exception handlers are always present - that's the idt argument */
        SET_IDT_ENTRY(idt[i], idt_func[i]);
        idt[i].present = HANDLER_PRESENT;
        idt[i].dpl = DPL_KERNEL; /* handlers should run in kernel */
        /* size should be alawys be zero according to intel manual, 
        0xE for 32bit trap gate*/
        idt[i].reserved3 = 0;
        idt[i].reserved2 = 1;
        idt[i].reserved1 = 1;
        idt[i].size = 1;
        idt[i].reserved0 = 0;
        /* interrupt flag unchanged upon entry for exception 
        handlers */
        idt[i].seg_selector = KERNEL_CS;
    }

    for (i = EXCEPTION_NUM; i < IDT_LEN; ++i) { //interrupts
        SET_IDT_ENTRY(idt[i], NULL);
        /* default to no handler present for interrupts */
        idt[i].present = HANDLER_ABSENT;
        idt[i].dpl = DPL_KERNEL; /* handlers should run in kernel */
        /* size should be alawys be zero according to intel manual, 
        0xF for 32bit interrupt gate */
        idt[i].reserved3 = 1;
        idt[i].reserved2 = 1;
        idt[i].reserved1 = 1;
        idt[i].size = 1;
        idt[i].reserved0 = 0;
        /* automatically disable interrupt flag upon entry for interrupt 
        handlers */
        idt[i].seg_selector = KERNEL_CS;
    }

    //add pit handler
    SET_IDT_ENTRY(idt[PIT_ENTRY], __wrapped__pit_handler_32);
    idt[PIT_ENTRY].present=HANDLER_PRESENT;

    //add rtc handler
    SET_IDT_ENTRY(idt[RTC_ENTRY], __wrapped__rtc_handler_40);
    idt[RTC_ENTRY].present=HANDLER_PRESENT;

    //add keyboard handler
    SET_IDT_ENTRY(idt[KEYBOARD_ENTRY], __wrapped__keyboard_handler_33);
    idt[KEYBOARD_ENTRY].present=HANDLER_PRESENT;

    //add system call handler
    SET_IDT_ENTRY(idt[SYS_CALL_ENTRY], __wrapped__system_call_handler_128);
    idt[SYS_CALL_ENTRY].present=HANDLER_PRESENT;
    idt[SYS_CALL_ENTRY].dpl=DPL_USER;
    /* DPL is user to enable calling system calls from userspace */
    /* 0xE for 32bit trap gate*/
    idt[SYS_CALL_ENTRY].reserved3 = 0;
    idt[SYS_CALL_ENTRY].reserved2 = 1;
    idt[SYS_CALL_ENTRY].reserved1 = 1;
    idt[SYS_CALL_ENTRY].size = 1;
    idt[SYS_CALL_ENTRY].reserved0 = 0;
    /* interrupt flag unchanged upon entry for syscall handlers, so that interrupts for devices still work */
}

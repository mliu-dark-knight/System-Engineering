/* interrpt.S - wrapper for C interrupt handlers 
                saves registers */

/* interrupt gate -> auto cli, trap gate -> no auto cli */
/* iret auto sti */
/* # dwords pushed even -> keep aligned to 4 bytes */
#define ASM 1
#define DUMMY 0xFFFFFFFF
#include "x86_desc.h"
.extern check_signals
SYSCALL_NUM_MIN = 1
SYSCALL_NUM_MAX = 13
ERR = -1
PTR_SIZE_BYTE = 4
SYSCALL_VECTOR = 0x80
SYSCALL_ARG_ONE = 16
SYSCALL_ARG_TWO = 20
SYSCALL_ARG_THREE = 24
SYSCALL_ARG_FOUR = 28
SYSCALL_ARG_FIVE = 32

/* assembly linkage for interrupt/exception handlers - we need to save all 
registers and execute iret at end to go back to PL 3*/
/* change to kmode ds, kmode cs change was automatic via IDT */
#define intr_handler_with_dummy(idt_table_name, func_name, return_pt)            \
    .extern func_name           ;\
    .globl idt_table_name        ;\
    .globl return_pt            ;\
    idt_table_name:              ;\
        pushl $DUMMY            ;\
        pushw %fs               ;\
        pushw %gs               ;\
        pushw %es               ;\
        pushw %ds               ;\
        pushl %eax                   ;\
        pushl %ebp                   ;\
        pushl %esi                   ;\
        pushl %edi                   ;\
        pushl %edx                   ;\
        pushl %ecx                   ;\
        pushl %ebx                   ;\
        movw $KERNEL_DS, %cx        ;\
        movw %cx, %ds               ;\
        call func_name               ;\
    return_pt:                      ;\
        call check_signals              ;\
        popl %ebx                    ;\
        popl %ecx                    ;\
        popl %edx                    ;\
        popl %edi                    ;\
        popl %esi                    ;\
        popl %ebp                    ;\
        popl %eax                    ;\
        popw %ds               ;\
        popw %es               ;\
        popw %gs               ;\
        popw %fs               ;\
        addl $4, %esp          ;\
        iret                          

/* assembly linkage for interrupt/exception handlers - we need to save all 
registers and execute iret at end to go back to PL 3*/
/* change to kmode ds, kmode cs change was automatic via IDT */
#define intr_handler_without_dummy(idt_table_name, func_name, return_pt)            \
    .extern func_name           ;\
    .globl idt_table_name        ;\
    .globl return_pt            ;\
    idt_table_name:              ;\
        pushw %fs               ;\
        pushw %gs               ;\
        pushw %es               ;\
        pushw %ds               ;\
        pushl %eax                   ;\
        pushl %ebp                   ;\
        pushl %esi                   ;\
        pushl %edi                   ;\
        pushl %edx                   ;\
        pushl %ecx                   ;\
        pushl %ebx                   ;\
        movw $KERNEL_DS, %cx        ;\
        movw %cx, %ds               ;\
        call func_name               ;\
    return_pt:                      ;\
        call check_signals              ;\
        popl %ebx                    ;\
        popl %ecx                    ;\
        popl %edx                    ;\
        popl %edi                    ;\
        popl %esi                    ;\
        popl %ebp                    ;\
        popl %eax                    ;\
        popw %ds               ;\
        popw %es               ;\
        popw %gs               ;\
        popw %fs               ;\
        addl $4, %esp          ;\
        iret                          

/* use c calling convention don't save 
eax since it has syscall number/return value */

/* stack has (args) eip esi edi ebx, so args are at esp + 4*4, 
esp + 5*4, ... */

/* according to spec, syscalls use vector 0x80 */
#define do_syscall(func_name, syscall_num)    \
    .globl func_name        ;\
    func_name:              ;\
        pushl %esi                 ;\
        pushl %edi                 ;\
        pushl %ebx                 ;\
        movl SYSCALL_ARG_ONE(%esp), %ebx        ;\
        movl SYSCALL_ARG_TWO(%esp), %ecx        ;\
        movl SYSCALL_ARG_THREE(%esp), %edx        ;\
        movl SYSCALL_ARG_FOUR(%esp), %edi        ;\
        movl SYSCALL_ARG_FIVE(%esp), %esi        ;\
        movl $syscall_num, %eax    ;\
        int $SYSCALL_VECTOR                  ;\
        popl %ebx                  ;\
        popl %edi                  ;\
        popl %esi                  ;\
        ret

/* convention is to prepend __wrapped__ to the version of the handlers with assembly linkage */
intr_handler_with_dummy(__wrapped__divide_error_0, divide_error_0, divide_error_0_ret);
intr_handler_with_dummy(__wrapped__debug_1, debug_1, debug_1_ret);
intr_handler_with_dummy(__wrapped__nmi_2, nmi_2, nmi_2_ret);
intr_handler_with_dummy(__wrapped__int3_3, int3_3, int3_3_ret);
intr_handler_with_dummy(__wrapped__overflow_4, overflow_4, overflow_4_ret);
intr_handler_with_dummy(__wrapped__bounds_5, bounds_5, bounds_5_ret);
intr_handler_without_dummy(__wrapped__invalid_op_6, invalid_op_6, invalid_op_6_ret);
intr_handler_with_dummy(__wrapped__device_not_available_7, device_not_available_7, device_not_available_7_ret);
intr_handler_with_dummy(__wrapped__doublefault_fn_8, doublefault_fn_8, doublefault_fn_8_ret);
intr_handler_with_dummy(__wrapped__coprocessor_segment_overrun_9, coprocessor_segment_overrun_9, coprocessor_segment_overrun_9_ret);
intr_handler_with_dummy(__wrapped__invalid_tss_10, invalid_tss_10, invalid_tss_10_ret);
intr_handler_without_dummy(__wrapped__segment_not_present_11, segment_not_present_11, segment_not_present_11_ret_);
intr_handler_without_dummy(__wrapped__stack_segment_12, stack_segment_12, stack_segment_12_ret);
intr_handler_without_dummy(__wrapped__general_protection_13, general_protection_13, general_protection_13_ret);
intr_handler_without_dummy(__wrapped__page_fault_14, page_fault_14, page_fault_14_ret);
intr_handler_with_dummy(__wrapped__intel_reserved_15, intel_reserved_15, intel_reserved_15_ret);
intr_handler_with_dummy(__wrapped__coprocessor_error_16, coprocessor_error_16, coprocessor_error_16_ret);
intr_handler_with_dummy(__wrapped__alignment_check_17, alignment_check_17, alignment_check_17_ret);
intr_handler_with_dummy(__wrapped__machine_check_18, machine_check_18, machine_check_18_ret);
intr_handler_with_dummy(__wrapped__simd_coprocessor_error_19, simd_coprocessor_error_19, simd_coprocessor_error_19_ret);
intr_handler_with_dummy(__wrapped__intel_reserved_20, intel_reserved_20, __wrapped__intel_reserved_20_ret);
intr_handler_with_dummy(__wrapped__intel_reserved_21, intel_reserved_21, __wrapped__intel_reserved_21_ret);
intr_handler_with_dummy(__wrapped__intel_reserved_22, intel_reserved_22, __wrapped__intel_reserved_22_ret);
intr_handler_with_dummy(__wrapped__intel_reserved_23, intel_reserved_23, __wrapped__intel_reserved_23_ret);
intr_handler_with_dummy(__wrapped__intel_reserved_24, intel_reserved_24, __wrapped__intel_reserved_24_ret);
intr_handler_with_dummy(__wrapped__intel_reserved_25, intel_reserved_25, __wrapped__intel_reserved_25_ret);
intr_handler_with_dummy(__wrapped__intel_reserved_26, intel_reserved_26, __wrapped__intel_reserved_26_ret);
intr_handler_with_dummy(__wrapped__intel_reserved_27, intel_reserved_27, __wrapped__intel_reserved_27_ret);
intr_handler_with_dummy(__wrapped__intel_reserved_28, intel_reserved_28, __wrapped__intel_reserved_28_ret);
intr_handler_with_dummy(__wrapped__intel_reserved_29, intel_reserved_29, __wrapped__intel_reserved_29_ret);
intr_handler_with_dummy(__wrapped__intel_reserved_30, intel_reserved_30, __wrapped__intel_reserved_30_ret);
intr_handler_with_dummy(__wrapped__intel_reserved_31, intel_reserved_31, __wrapped__intel_reserved_31_ret);

intr_handler_with_dummy(__wrapped__pit_handler_32, pit_handler_32, pit_handler_32_ret);
intr_handler_with_dummy(__wrapped__rtc_handler_40, rtc_handler_40, rtc_handler_40_ret);
intr_handler_with_dummy(__wrapped__keyboard_handler_33, keyboard_handler_33, keyboard_handler_33_ret);

/* syscall numbers from 1 to 8 - see the ece391syscall.h source code */
do_syscall(halt, 1);
do_syscall(execute, 2);
do_syscall(read, 3);
do_syscall(write, 4);
do_syscall(open, 5);
do_syscall(close, 6);
do_syscall(getargs, 7);
do_syscall(vidmap, 8);
do_syscall(set_handler, 9);
do_syscall(sigreturn, 10);
do_syscall(malloc, 11);
do_syscall(free, 12);
do_syscall(touch, 13);

/*
 * system_call_handler_128
 *   DESCRIPTION: system call handler
 *   INPUTS: system call calling convention - arguments passing in registers,
 *           depends on specific syscall
 *   OUTPUTS: depends on specific syscall
 *   RETURN VALUE: int32_t, meaning depends on specific syscall
 *   SIDE EFFECTS: executes the specified system call handler - see syscall.c
 */

/* assembly linkage - we need to save all 
registers and execute iret at end to go back to PL 3
Also use direct assembly here so we don't create extra stack frames */
 .globl __wrapped__system_call_handler_128  
__wrapped__system_call_handler_128:
    sti /* reenable interrupts so drivers work (let driver interrupts preempt syscall)*/
    pushl $DUMMY
    pushw %fs              
    pushw %gs              
    pushw %es              
    pushw %ds
    pushl %eax              
    pushl %ebp                   
    pushl %esi                   
    pushl %edi                   
    pushl %edx
    pushl %ecx                   
    pushl %ebx
    /* make sure syscall is valid number */           
    cmpl $SYSCALL_NUM_MIN, %eax
    jl system_call_handler_128_rtn_err
    cmpl $SYSCALL_NUM_MAX, %eax
    jg system_call_handler_128_rtn_err
    movw $KERNEL_DS, %cx       
    movw %cx, %ds              
         /* change to kmode ds, kmode cs change was automatic via IDT */
    call *syscall_func(, %eax, PTR_SIZE_BYTE)
    /* we put a blank entry in syscall func, so we don't need to subtract 1 */
    jmp system_call_handler_128_rtn
system_call_handler_128_rtn_err:
    /* return -1 if system call is invalid or not implemented */
    movl $ERR, %eax
system_call_handler_128_rtn:
    call check_signals
    popl %ebx                    
    popl %ecx                    
    popl %edx                    
    popl %edi                    
    popl %esi                    
    popl %ebp
    addl $4, %esp #pop out eax
    popw %ds               
    popw %es               
    popw %gs               
    popw %fs
    addl $4, %esp               
    iret

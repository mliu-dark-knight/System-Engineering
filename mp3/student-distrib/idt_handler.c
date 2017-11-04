#include "idt_handler.h"
#include "lib.h"
#include "i8259.h"    
#include "syscall.h"
#include "task.h"
#include "interrupt.h"                        

//0-7
/*
 * divide_error_0
 *   DESCRIPTION: divide by zero handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void divide_error_0() {
    printf("divide_error_0\n");
    current_pcb[active_task_idx]->signal_flag[DIV_ZERO] = SIGNAL_PENDING;
    return;
}

/*
 * debug_1
 *   DESCRIPTION: debug exception handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void debug_1() {
    printf("debug_1\n");
    do_halt_exception(); //return to parent with code 256
}

/*
 * nmi_2
 *   DESCRIPTION: nmi handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void nmi_2() {
    printf("nmi_2\n");
    do_halt_exception(); //return to parent with code 256
}

/*
 * int3_3
 *   DESCRIPTION: breakpoint handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void int3_3() {
    printf("int3_3\n");
    do_halt_exception(); //return to parent with code 256
}

/*
 * overflow_4
 *   DESCRIPTION: math overflow handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void overflow_4() {
    printf("overflow_4\n");
    do_halt_exception(); //return to parent with code 256
}

/*
 * bounds_5
 *   DESCRIPTION: bound range exceeded handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void bounds_5() {
    printf("bounds_5\n");
    do_halt_exception(); //return to parent with code 256
}

/*
 * invalid_op_6
 *   DESCRIPTION: invalid opcode handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void invalid_op_6() {
    printf("invalid_op_6\n");
    current_pcb[active_task_idx]->signal_flag[SEGFAULT] = SIGNAL_PENDING;
    return; //return to parent with code 256
}

/*
 * device_not_available_7
 *   DESCRIPTION: no FPU exists handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void device_not_available_7() {
    printf("device_not_available_7\n");
    do_halt_exception(); //return to parent with code 256
}

//8-15
/*
 * doublefault_fn_8
 *   DESCRIPTION: double fault handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void doublefault_fn_8() {
    printf("doublefault_fn_8\n");
    do_halt_exception(); //return to parent with code 256
}

/*
 * coprocessor_segment_overrun_9
 *   DESCRIPTION: coprocessor segment overrun handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void coprocessor_segment_overrun_9() {
    printf("coprocessor_segment_overrun_9\n");
    current_pcb[active_task_idx]->signal_flag[SEGFAULT] = SIGNAL_PENDING;
    return; //return to parent with code 256
}

/*
 * invalid_tss_10
 *   DESCRIPTION: invalid TSS handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void invalid_tss_10() {
    printf("invalid_tss_10\n");
    do_halt_exception(); //return to parent with code 256
}

/*
 * segment_not_present_11
 *   DESCRIPTION: segment not present handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void segment_not_present_11() {
    printf("segment_not_present_11\n");
    current_pcb[active_task_idx]->signal_flag[SEGFAULT] = SIGNAL_PENDING;
    return; //return to parent with code 256
}

/*
 * stack_segment_12
 *   DESCRIPTION: stack segment error handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void stack_segment_12() {
    printf("stack_segment_12\n");
    current_pcb[active_task_idx]->signal_flag[SEGFAULT] = SIGNAL_PENDING;
    return; //return to parent with code 256
}

/*
 * general_protection_13
 *   DESCRIPTION: general protection fault handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void general_protection_13() {
    printf("general_protection_13\n");
    current_pcb[active_task_idx]->signal_flag[SEGFAULT] = SIGNAL_PENDING;
    return;
}

/*
 * page_fault_14
 *   DESCRIPTION: page fault handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void page_fault_14() {
    printf("page_fault_14\n");
    current_pcb[active_task_idx]->signal_flag[SEGFAULT] = SIGNAL_PENDING;
    return; //return to parent with code 256
}

/*
 * intel_reserved_15
 *   DESCRIPTION: intel_reserved_15 handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void intel_reserved_15() {
    printf("intel_reserved_15\n");
    do_halt_exception(); //return to parent with code 256
}

//16-23
/*
 * coprocessor_error_16
 *   DESCRIPTION: coprocessor error handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void coprocessor_error_16() {
    printf("coprocessor_error_16\n");
    do_halt_exception(); //return to parent with code 256
}

/*
 * alignment_check_17
 *   DESCRIPTION: memory alignment check failed handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void alignment_check_17() {
    printf("alignment_check_17\n");
    do_halt_exception(); //return to parent with code 256
}

/*
 * machine_check_18
 *   DESCRIPTION: machine check failed handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void machine_check_18() {
    printf("machine_check_18\n");
    do_halt_exception(); //return to parent with code 256
}

/*
 * simd_coprocessor_error_19
 *   DESCRIPTION: simd coprocessor error handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void simd_coprocessor_error_19() {
    printf("simd_coprocessor_error_19\n");
    do_halt_exception(); //return to parent with code 256
}

/*
 * intel_reserved_20
 *   DESCRIPTION: intel_reserved_20handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void intel_reserved_20() {
    printf("intel_reserved_20\n");
    do_halt_exception(); //return to parent with code 256
}

/*
 * intel_reserved_21
 *   DESCRIPTION: intel_reserved_21 handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void intel_reserved_21() {
    printf("intel_reserved_22\n");
    do_halt_exception(); //return to parent with code 256
}

/*
 * intel_reserved_22
 *   DESCRIPTION: intel_reserved_22 handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void intel_reserved_22() {
    printf("intel_reserved_22\n");
    do_halt_exception(); //return to parent with code 256
}

/*
 * intel_reserved_23
 *   DESCRIPTION: intel_reserved_23 handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void intel_reserved_23() {
    printf("intel_reserved_23\n");
    do_halt_exception(); //return to parent with code 256
}
//24-31
/*
 * intel_reserved_24
 *   DESCRIPTION: intel_reserved_24 handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void intel_reserved_24() {
    printf("intel_reserved_24\n");
    do_halt_exception(); //return to parent with code 256
}

/*
 * intel_reserved_25
 *   DESCRIPTION: intel_reserved_25handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void intel_reserved_25() {
    printf("intel_reserved_25\n");
    do_halt_exception(); //return to parent with code 256
}

/*
 * intel_reserved_26
 *   DESCRIPTION: intel_reserved_26 handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void intel_reserved_26() {
    printf("intel_reserved_26\n");
    do_halt_exception(); //return to parent with code 256
}

/*
 * intel_reserved_27
 *   DESCRIPTION: intel_reserved_27 handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void intel_reserved_27() {
    printf("intel_reserved_27\n");
    do_halt_exception(); //return to parent with code 256
}
/*
 * intel_reserved_28
 *   DESCRIPTION: intel_reserved_28 handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void intel_reserved_28() {
    printf("intel_reserved_28\n");
    do_halt_exception(); //return to parent with code 256
}

/*
 * intel_reserved_29
 *   DESCRIPTION: intel_reserved_29 handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void intel_reserved_29() {
    printf("intel_reserved_29\n");
    do_halt_exception(); //return to parent with code 256
}

/*
 * intel_reserved_30
 *   DESCRIPTION: intel_reserved_30 handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void intel_reserved_30() {
    printf("intel_reserved_30\n");
    do_halt_exception(); //return to parent with code 256
}

/*
 * intel_reserved_31
 *   DESCRIPTION: intel_reserved_31 handler
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: return to parent with code 256, prints message
 */
void intel_reserved_31() {
    printf("intel_reserved_31\n");
    do_halt_exception(); //return to parent with code 256
}

/* table of function pointers for all exception handlers */
void* idt_func[NUM_EXCEPTIONS] = {
    __wrapped__divide_error_0,
    __wrapped__debug_1,
    __wrapped__nmi_2,
    __wrapped__int3_3,
    __wrapped__overflow_4,
    __wrapped__bounds_5,
    __wrapped__invalid_op_6,
    __wrapped__device_not_available_7,

    __wrapped__doublefault_fn_8,
    __wrapped__coprocessor_segment_overrun_9,
    __wrapped__invalid_tss_10,
    __wrapped__segment_not_present_11,
    __wrapped__stack_segment_12,
    __wrapped__general_protection_13,
    __wrapped__page_fault_14,
    __wrapped__intel_reserved_15,

    __wrapped__coprocessor_error_16,
    __wrapped__alignment_check_17,
    __wrapped__machine_check_18,
    __wrapped__simd_coprocessor_error_19,
    __wrapped__intel_reserved_20,
    __wrapped__intel_reserved_21,
    __wrapped__intel_reserved_22,
    __wrapped__intel_reserved_23,

    __wrapped__intel_reserved_24,
    __wrapped__intel_reserved_25,
    __wrapped__intel_reserved_26,
    __wrapped__intel_reserved_27,
    __wrapped__intel_reserved_28,
    __wrapped__intel_reserved_29,
    __wrapped__intel_reserved_30,
    __wrapped__intel_reserved_31
};

#ifndef _IDT_HANDLER_H
#define _IDT_HANDLER_H

#include "types.h"

#define NUM_EXCEPTIONS 32

//see c file for detains
extern void divide_error_0();
extern void debug_1();
extern void nmi_2();
extern void int3_3();
extern void overflow_4();
extern void bounds_5();
extern void invalid_op_6();
extern void device_not_available_7();

extern void doublefault_fn_8();
extern void coprocessor_segment_overrun_9();
extern void invalid_tss_10();
extern void segment_not_present_11();
extern void stack_segment_12();
extern void general_protection_13();
extern void page_fault_14();
extern void intel_reserved_15();

extern void coprocessor_error_16();
extern void alignment_check_17();
extern void machine_check_18();
extern void simd_coprocessor_error_19();
extern void intel_reserved_20();
extern void intel_reserved_21();
extern void intel_reserved_22();
extern void intel_reserved_23();

extern void intel_reserved_24();
extern void intel_reserved_25();
extern void intel_reserved_26();
extern void intel_reserved_27();
extern void intel_reserved_28();
extern void intel_reserved_29();
extern void intel_reserved_30();
extern void intel_reserved_31();

extern void* idt_func[NUM_EXCEPTIONS];

#endif /* _IDT_HANDLER_H */

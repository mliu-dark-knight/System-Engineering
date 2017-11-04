#ifndef _INTERRUPT_H
#define _INTERRUPT_H

extern void __wrapped__divide_error_0();
extern void __wrapped__debug_1();
extern void __wrapped__nmi_2();
extern void __wrapped__int3_3();
extern void __wrapped__overflow_4();
extern void __wrapped__bounds_5();
extern void __wrapped__invalid_op_6();
extern void __wrapped__device_not_available_7();

extern void __wrapped__doublefault_fn_8();
extern void __wrapped__coprocessor_segment_overrun_9();
extern void __wrapped__invalid_tss_10();
extern void __wrapped__segment_not_present_11();
extern void __wrapped__stack_segment_12();
extern void __wrapped__general_protection_13();
extern void __wrapped__page_fault_14();
extern void __wrapped__intel_reserved_15();

extern void __wrapped__coprocessor_error_16();
extern void __wrapped__alignment_check_17();
extern void __wrapped__machine_check_18();
extern void __wrapped__simd_coprocessor_error_19();
extern void __wrapped__intel_reserved_20();
extern void __wrapped__intel_reserved_21();
extern void __wrapped__intel_reserved_22();
extern void __wrapped__intel_reserved_23();

extern void __wrapped__intel_reserved_24();
extern void __wrapped__intel_reserved_25();
extern void __wrapped__intel_reserved_26();
extern void __wrapped__intel_reserved_27();
extern void __wrapped__intel_reserved_28();
extern void __wrapped__intel_reserved_29();
extern void __wrapped__intel_reserved_30();
extern void __wrapped__intel_reserved_31();
//save and restore registers for the handlers
extern void __wrapped__pit_handler_32();
extern void __wrapped__rtc_handler_40();
extern void __wrapped__keyboard_handler_33();
extern void __wrapped__system_call_handler_128();

extern void system_call_handler_128();

/* syscalls */
extern int32_t halt(uint8_t status);
extern int32_t execute(const uint8_t* command);
extern int32_t getargs(uint8_t* buf, int32_t nbytes);
extern int32_t vidmap(uint8_t** screen_start);
extern int32_t read(int32_t fd, void* buf, int32_t nbytes);
extern int32_t write(int32_t fd, const void* buf, int32_t nbytes);
extern int32_t open(const uint8_t* filename);
extern int32_t close(int32_t fd);
extern int32_t set_handler (int32_t signum, void* handler_address);
extern int32_t sigreturn (void);

#endif /* _INTERRUPT_H */

#ifndef _SYSCALL_H_
#define _SYSCALL_H_
#include "types.h"
#include "filesystem.h"

#define NUM_SYSCALLS 13

/* filesystem file descriptor flag bitmasks */
#define CLEAR_ALL_FLAGS 0x00
#define FLAG_IN_USE 0x01
#define FLAG_RTC 0x02
#define FLAG_FILE 0x04
#define FLAG_DIR 0x08
#define FILE_ARRAY_LENGTH 8
#define USR_PRG_VIRTUAL_START (128 * MEGA)
#define USR_PRG_VIRTUAL_END (132 * MEGA)
#define USR_VIDMAP_VIRTUAL_START USR_PRG_VIRTUAL_END
#define USR_PRG_PHY_BASE 0x0800000
#define USR_DPL 3
#define KERNEL_DPL 0
#define PRG_OFFSET 0x08048000

/* sizes of various buffers */
#define FILE_NAME_LEN 32
#define LINE_BUFFER_LEN 128
#define EXEC_HEADER_LEN 40
#define FILE_ARRAY_LENGTH 8

#define MAX_PCB 6

/* ELF header magic numbers */
#define MAGIC_NUM_0_IDX 0
#define MAGIC_NUM_1_IDX 1
#define MAGIC_NUM_2_IDX 2
#define MAGIC_NUM_3_IDX 3
#define MAGIC_NUM_0 0x7F
#define MAGIC_NUM_1 0x45
#define MAGIC_NUM_2 0x4C
#define MAGIC_NUM_3 0x46
#define PAGE_DIR_USER_IDX_OFFSET 1
#define KMODE_STACK_OFFSET 4
#define PTR_SIZE 4
#define ENTRY_POINT_OFFSET 8

/* entry point starts at 24 bytes */
#define ENTRY_POINT_START 24

/* see task.h */
#define TASK_NOT_PRESENT    -1
#define TASK_NOT_ACTIVE     0
#define TASK_ACTIVE         1
#define FD_STDIO 1
#define SYSCALL_NUM_NULL_ENTRY_OFFSET 1

#define NUM_SIGNAL 5
#define SLAB_PAGE_SIZE (1<<12)

// for accessing jump table operations
#define EXCEPTION_RETVAL 256

#define TWENTY_HIGH_BIT_MASK	0xFFFFF000		/* mask to get 20 high bits of linear address */

typedef int32_t (*syscall_func_t)();

//see c file for more
extern int32_t init_syscall();

extern int32_t do_halt(uint8_t status);
extern int32_t do_halt_exception();
extern int32_t do_execute(const uint8_t* command);

extern int32_t do_getargs(uint8_t* buf, int32_t nbytes);
extern int32_t do_vidmap(uint8_t** screen_start);
extern int32_t do_set_handler(int32_t signum, void* handler_address);
extern int32_t do_sigreturn();

extern int32_t do_read(int32_t fd, void* buf, int32_t nbytes);
extern int32_t do_write(int32_t fd, const void* buf, int32_t nbytes);
extern int32_t do_open(const uint8_t* filename);
extern int32_t do_close(int32_t fd);
extern void* do_malloc(int32_t size);
extern int32_t do_free(void* ptr);
extern int32_t do_touch(const uint8_t* filename);


extern int32_t launch_shell (uint32_t pid);

extern void syscall_test();


extern int32_t (*syscall_func[NUM_SYSCALLS + SYSCALL_NUM_NULL_ENTRY_OFFSET])();

extern void* signal_handler_default[NUM_SIGNAL];


asm(".globl __halt_return_point");

#endif /* _SYSCALL_H_ */

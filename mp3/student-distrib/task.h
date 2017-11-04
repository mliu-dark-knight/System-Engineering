
#ifndef _TASK_H
#define _TASK_H

#include "types.h"
#include "syscall.h"
#include "filesystem.h"
#include "paging.h"
#include "terminal.h"
#include "lib.h"

#define FILE_ARRAY_LENGTH 8
#define REGISTERS_NUM 8
#define LINE_BUFFER_LEN 128
#define MAX_PCB 6
#define PID_USER_OFFSET 1
#define JUMP_TABLE_ENTRIES 4
#define NUM_TERMINAL 3	//constant defined here to avoid recursive include

//signals
#define NUM_SIGNAL 5

#define DIV_ZERO 0
#define SEGFAULT 1
#define INTERRUPT 2
#define ALARM 3
#define USER1 4

#define SIGNAL_MASK_ON 1
#define SIGNAL_MASK_OFF 0
#define SIGNAL_PENDING 1
#define SIGNAL_NOT_PENDING 0

#define TASK_NOT_PRESENT	-1
#define TASK_NOT_ACTIVE		0
#define TASK_ACTIVE 		1

#define INIT_TASK 0
#define INVALID_PID -1

//be careful of dummy code
#define CONTEXT_SIZE 60
#define EAX_OFFSET 24
#define RETURN_ADDRESS_OFFSET 40
#define CS_OFFSET 44
#define ESP_OFFSET 52
#define SS_OFFSET 56

//for malloc
#define NUM_SLABS 4

struct inode_n;
struct file_desc_n;
struct all_regs;
struct pcb_n;

typedef struct operations_n {
	int32_t (*open)();
	int32_t (*read)();
	int32_t (*write)();
	int32_t (*close)();
} operations_t;

typedef struct file_desc_n {
	uint32_t flags;
	uint32_t pos;
	struct inode_n *inode;
	operations_t operations;
} file_desc_t;

typedef struct slab_n
{
	int32_t bitmap;
	void* cache_ptr;
} slab_t;

typedef struct pcb_n {
	uint32_t flag;
	uint32_t pid;
	uint32_t esp;
	uint32_t ebp;
	uint32_t parent_pid;
	uint32_t parent_esp;
	uint32_t parent_ebp;
	
	uint8_t args[LINE_BUFFER_LEN];
	uint32_t terminal_idx;

	volatile int32_t using_rtc;
	volatile uint32_t rtc_counter;
	volatile int32_t rtc_freq;
	volatile int32_t rtc_interrupt_occurred;

	uint32_t signal_flag[NUM_SIGNAL];
	uint32_t signal_mask[NUM_SIGNAL];
	
	void(*signal_handler[NUM_SIGNAL])();
	file_desc_t file_descriptors[FILE_ARRAY_LENGTH];

	slab_t slab_cache[NUM_SLABS];
	
} pcb_t;

// Important!!!!!!!! the first three pcb pointer correspond to the base shell of three terminals
extern pcb_t* pcb_array[MAX_PCB];
extern pcb_t* current_pcb[NUM_TERMINAL];
extern int32_t active_task_idx;

extern uint32_t get_cur_pid();
extern void init_pcb();
extern void preempt();
extern void check_signals();
extern void unmask_signals(uint32_t task_idx);
extern void mask_signals(uint32_t task_idx);
extern void unpend_signals(uint32_t task_idx);

#endif /* _TASK_H */


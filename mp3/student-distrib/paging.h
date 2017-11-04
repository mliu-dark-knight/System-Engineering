#ifndef _PAGING_H
#define _PAGING_H

#include "types.h"
#include "idt_handler.h"
#include "lib.h"

#define NUM_PDE				1024
#define NUM_PTE				1024
#define KILO				0x0400
#define MEGA				0x00100000
#define PAGE_DIR_SIZE		(KILO * 4)
#define PAGE_TABLE_SIZE		(KILO * 4)
#define PAGE_SIZE 			(KILO * 4)
#define DIR_ADDRESSABLE		(4 * MEGA)	/* page directory is 4MB addressable */
#define TABLE_ADDRESSABLE	(4 * KILO)	/* page table is 4KB addressable */
#define KERNEL_START		(4 * MEGA)	/* physical address of kernel is 4MB -- 8MB, exclusive!!! */
#define KERNEL_END			(8 * MEGA)
#define VIDEO_MEM_START		VIDEO 		/* 0xA0000 base addr of vid mem VGA */
#define VIDEO_MEM_END		(VIDEO_MEM_START + 1 * PAGE_SIZE)	
/* end at 0xAFFFF 64k planes * 4 planes / 4 planes = > 64k addrs = > 64k / 4k => 16 pages */

#define NUM_TASK			6
#define NUM_PAGE_DIR		(NUM_TASK + 1)
#define NUM_PAGE_TABLE		(NUM_TASK + 1)

#define DPL_USER 3
#define DPL_KERNEL 0
#define TASK_KERNEL 0
#define VADDR_PTE_NUM 12
#define VADDR_PDE_NUM 22


//see c file for details
extern void init_paging (void);

/* be careful that user program task_id should start from 1, task_id 0 reserved to kernel */
extern void map_mega_page (uint32_t virtual_addr, uint32_t physical_addr, uint32_t task_id, uint32_t dpl);
extern void map_kilo_page (uint32_t virtual_addr, uint32_t physical_addr, uint32_t task_id, uint32_t dpl);
extern void update_page_directory(uint32_t task_id);

#endif /* _PAGING_H */


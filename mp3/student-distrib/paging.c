#include "paging.h"
#include "lib.h"

#define TEN_HIGH_BIT_MASK		0xFFC00000		/* mask to get 10 high bits of linear address */
#define TEN_MID_BIT_MASK		0x003FF000		/* mask to get 10 mid bits of linear address */
#define TWENTY_HIGH_BIT_MASK	0xFFFFF000		/* mask to get 20 high bits of linear address */

#define PRESENT_DEFAULT			(~0x00000001)	/* & with this number to set present to default -- not present */
#define READ_WRITE_DEFAULT		0x00000002		/* | with this number to set read/write to default -- read/write */
#define USR_SPVR_DEFUALT		(~0x00000004)	/* & with this number to set user/supervisor to default -- supervisor */
#define WRITE_DEFAUTL 			0x00000008 		/* | with this number to enable cache write through by default */
#define CACHE_DEFAULT			0x00000010 		/* | with this number to disable cache by defualt*/
#define ACCESSED_DEFAULT		(~0x00000020) 	/* & with this number to set accessed to default -- not accessed */
#define DIRTY_DEFAULT			(~0x00000040) 	/* & with this number to set dirty to defualt -- not written to */
#define SIZE_DEFUALT			0x00000080 		/* | with this number to set page size to default -- 4MB */
#define GLOBAL_DEFAULT			(~0x00000100) 	/* & with this number to set global to default -- unset */

#define PAGE_DIR_LOW_DEFAULT	0x0000009A		/* | with this number to set the 10 low bits of PDE to default */
#define PAGE_TABLE_LOW_DEFAULT	0x0000001A		/* | with this number to set the 10 low bits of PTE to default */

#define PRESENT_FLAG			0x00000001		/* | with this flag to set page to be present */
#define READ_WRITE_FLAG			(~0x00000002)	/* & with this flag to set page to read only */
#define USR_SPVR_FLAG			0x00000004		/* | with this flag to set user/supervisor to user */
#define WRITE_FLAG 				(~0x00000008) 	/* & with this flag to enable cache write-back */
#define CACHE_FLAG				(~0x00000010) 	/* & with this flag to enable cache */
#define ACCESSED_FLAG			0x00000020 		/* | with this flag to set accessed to accessed */
#define DIRTY_FLAG				0x00000040 		/* | with this flag to set dirty to written */
#define SIZE_FLAG				(~0x00000080) 	/* & with this flag to set page size to 4KB */
#define GLOBAL_FLAG				0x00000100 		/* | with this flag to set global */

#define PAGE_DIRECTORY 			0x0FFFF000		/* base physical address of page directory */
#define PAGE_TABLE 				0x0FC00000 		/* base physical address of page table */

/* control register bitmasks */
#define CR0_PG 					(0x01<<31)		/* bit 31 of CR0 enables paging 
												*/
#define CR4_PSE					(0x01<<4)		/* bit 4 of CR4 enables large 
												pages - 4MB */
#define CR4_PGE					(0x01<<7)		/* bit 4 of CR4 enables global 
												pages - not flushed from TLB 
												upon process switch - 4MB */

/* array storing all page directories for all processes */
uint32_t page_directory[NUM_PAGE_DIR][NUM_PDE] 
	__attribute__((aligned (PAGE_DIR_SIZE)));
/* array storing all page tables for all processes, map video memory 
only since other pages are large pages */
uint32_t page_table[NUM_PAGE_TABLE][NUM_PTE] 
	__attribute__((aligned (PAGE_TABLE_SIZE)));

/*
 * init_paging
 *   DESCRIPTION: init page tables to known 
 *				  values to ensure we don't crash and for security reasons by
 *				  avoiding uninitalized values.
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: map kernel space and video memory to itself, video memory
 *				   to 1 4kb page and kernel to 1 4mb page, 
 8				    paging enabled and TLB flushed, CR3 changed
 */
void init_paging() {

	/* clear all page tables for all processes to ensure we don't crash and 
	for security reasons by avoiding uninitalized values. */
	memset(page_directory, 0x00, PAGE_DIR_SIZE * NUM_PAGE_DIR);
	memset(page_table, 0x00, PAGE_TABLE_SIZE * NUM_PAGE_TABLE);

	/*map kernel space and video memory to itself, video memory
 	to 1 4kb page and kernel to 1 4mb page, with kernel space 
 	permissions to allow kernel direct access */
	map_mega_page(KERNEL_START, KERNEL_START, TASK_KERNEL, DPL_KERNEL);
	map_kilo_page(VIDEO_MEM_START, VIDEO_MEM_START, TASK_KERNEL, DPL_KERNEL);

	/* enable paging */
	asm volatile (
		"movl $page_directory, %%eax\n\t" 
		/* "orl $0x7FFFF000, %%eax\n\t" */
		"movl %%eax, %%cr3\n\t" /* move page directory base addr to CR3 and set 
								   caching bits to zero */

		"movl %%cr4, %%eax\n\t"  /* read-modify-write to ensure other flags 
								and reserved bits are unchanged */
		"orl $0x00000010, %%eax\n\t" /* bit 4 of CR4 enables large pages - 4MB*/
		"movl %%eax, %%cr4\n\t" /* activate large pages */

		"movl %%cr0, %%eax\n\t"
		"orl $0x80000000, %%eax\n\t"  /* bit 31 of CR0 enables paging */
		"movl %%eax, %%cr0\n\t" /* paging is now ON */
		: /* no outputs */
		: /* no inputs */
		: "cc", "eax"
	);

	return;
}

/*
 * map_mega_page
 *   DESCRIPTION: map a single 4MB phys page to a given base virtual address
 *   INPUTS: virtual_addr: base virtual address of the page, must be 4MB aligned
 *			 physical_addr: base physical address of the page, 
 *		     must be 4MB aligned
 *			 task_id: PID of the task's page table to do the mapping on
 *			 dpl: privilege level for this mapping
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: page table entry/page directory entry changed.
 *			       To avoid stale mappings, we always use change page directory
 *				   after using this function so TLB is flushed.
 */
void map_mega_page (uint32_t virtual_addr, uint32_t physical_addr, uint32_t 
	task_id, uint32_t dpl) {

	/* set PDE to default settings for a 4MB page and correct base addr, 
	privilege level, and set it to be present */
	uint32_t PDE = PAGE_DIR_LOW_DEFAULT;
	PDE |= PRESENT_FLAG;
	if (dpl == DPL_USER)
		PDE |= USR_SPVR_FLAG;
	PDE |= (physical_addr & TEN_HIGH_BIT_MASK);
	/* task 0 is kernel */
	page_directory[task_id][virtual_addr>>VADDR_PDE_NUM] = PDE;
}

/*
 * map_kilo_page
 *   DESCRIPTION: map a single 4KB phys page to a given base virtual address.
 *				  Limit one page per process, as teh current memory layout only
 *			      need 1 4KB page for vidmapping
 *   INPUTS: virtual_addr: base virtual address of the page, must be 4KB aligned
 *			 physical_addr: base physical address of the page, 
 *		     must be 4KB aligned
 *			 task_id: PID of the task's page table to do the mapping on
 *			 dpl: privilege level for this mapping
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: page table entry/page directory entry changed.
 *			       To avoid stale mappings, we always use change page directory
 *				   after using this function so TLB is flushed.
 */
void map_kilo_page (uint32_t virtual_addr, uint32_t physical_addr, uint32_t 
	task_id, uint32_t dpl) {

	/* set PDE to default settings for pointing to 1024 page tables and 
	correct base addr, privilege level, and set it to be present */
	uint32_t PDE = PAGE_DIR_LOW_DEFAULT;
	PDE |= PRESENT_FLAG;

	/* not large page unlike default */
	PDE &= SIZE_FLAG;
	if (dpl == DPL_USER)
		PDE |= USR_SPVR_FLAG;

	/* we store page tables in a seperate array than page directories, for the 
	current page layout we should only need 1 4MB page and 1 4KB page per task*/
	/* page table is aligned by 4KB */
	PDE |= (uint32_t)page_table[task_id];
	/* task 0 is kernel */
	page_directory[task_id][(virtual_addr) >> VADDR_PDE_NUM] = PDE;

	/* set PTE to default settings and correct base addr, 
	privilege level, and set it to be present */
	uint32_t PTE = PAGE_TABLE_LOW_DEFAULT;
	PTE |= PRESENT_FLAG;
	if (dpl == DPL_USER)
		PTE |= USR_SPVR_FLAG;
	PTE |= (physical_addr & TWENTY_HIGH_BIT_MASK);

	page_table[task_id][(virtual_addr & TEN_MID_BIT_MASK) >> VADDR_PTE_NUM] = PTE;
}

/*
 * update_page_directory
 *   DESCRIPTION: change the page tables to that of PID task_id
 *   INPUTS: task_id L PID of desired task to change virtual memory space to
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: changes CR3, flushes TLB
 */
void update_page_directory(uint32_t task_id) {
	/* get a reference to the new PT tree from the global PT array */
	uint32_t new_page_directory=(uint32_t)page_directory[task_id];
	/* update cr3, TLB auto flushed */
	asm volatile (
		"movl %0, %%cr3;" 
		: /* no outputs */
		:"r"(new_page_directory)
		:"cc"
	);
	return;
}


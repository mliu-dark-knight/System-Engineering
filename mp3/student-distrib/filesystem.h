#ifndef _FILESYSTEM_H
#define _FILESYSTEM_H

#include "types.h"
#include "syscall.h"
#include "task.h"
#include "terminal.h"

/* constants for the filesystem structure - see mp3 appendix a/b */
#define DATA_BLOCKS_PER_INODE (1024-1) //because each block to be 4kb
#define FILE_NAME_LEN 32 // 32 char + 1 null
#define INODE_RESERVED_LEN 24
#define BOOT_BLOCK_RESERVED_LEN 52
#define NUM_INODE 63

/* filetype flags, as specified in the directory entry */
#define TYPE_RTC 0
#define TYPE_DIR 1
#define TYPE_FILE 2

/* fd 0 and fd 1 are always the keyboard and screen respectively */
#define FD_STDIN 0
#define FD_STDOUT 1
#define FD_MAX 8

#define LARGE_BUFFER 10000

typedef struct dentry_n {
	int8_t file_name[FILE_NAME_LEN];
	uint32_t file_type;
	uint32_t num_inode;
	uint8_t reserved[INODE_RESERVED_LEN];
} dentry_t;


typedef struct boot_block_n {
	uint32_t num_dir_entries;
	uint32_t num_inodes;
	uint32_t num_data_blocks;
	uint8_t reserved[BOOT_BLOCK_RESERVED_LEN];
	dentry_t dentries[NUM_INODE];
} boot_block_t;


typedef struct inode_n {
	uint32_t length_in_byte;
	uint32_t data_blocks[DATA_BLOCKS_PER_INODE];
} inode_t;


typedef struct filesystem_info_n {
	uint32_t disk_start_addr;
	boot_block_t* boot_block_ptr;
	inode_t* first_inode_ptr;
} filesystem_info_t;


typedef struct directory_info_n {
	int8_t dir_name[FILE_NAME_LEN];
	struct directory_info_n* parent;
} directory_info_t;


extern filesystem_info_t filesys_info;

//see c file for detains
extern int32_t filesystem_read(int32_t fd, void* buf, int32_t nbytes);
extern int32_t filesystem_write(int32_t fd, const void* buf, int32_t nbytes);
extern int32_t filesystem_open(const uint8_t* filename);
extern int32_t filesystem_close(int32_t fd);
extern int32_t filesystem_touch(const uint8_t* filename);

//see c file for detains
extern int32_t regular_file_read(int32_t fd, void* buf, int32_t nbytes);
extern int32_t regular_file_write(int32_t fd, const void* buf, int32_t nbytes);
extern int32_t regular_file_open(const uint8_t* filename);
extern int32_t regular_file_close(int32_t fd);

//see c file for detains
extern int32_t directory_read(int32_t fd, void* buf, int32_t nbytes);
extern int32_t directory_write(int32_t fd, const void* buf, int32_t nbytes);
extern int32_t directory_open(const uint8_t* filename);
extern int32_t directory_close(int32_t fd);

//see c file for detains
extern int32_t read_dentry_by_name (const int8_t* fname, dentry_t* dentry);
extern int32_t read_dentry_by_index (uint32_t index, dentry_t* dentry);
extern int32_t read_data (uint32_t inode, uint32_t offset, uint8_t* buf, uint32_t length);

//see c file for detains
extern int32_t validate_filename_and_trunc(int8_t* fname, int8_t* fname_truncated);
extern int32_t read_regular_file(inode_t* cur_inode_ptr, uint32_t offset, uint8_t* buf, uint32_t length);
extern int32_t read_directory(uint32_t offset, uint8_t* buf, uint32_t length);

//for extra credit
extern int32_t write_regular_file(inode_t* cur_inode_ptr, uint32_t offset, 
	const uint8_t* buf, uint32_t length);

//see c file for detains
extern void init_filesystem (uint32_t disk_start_addr);
extern void test_filesystem ();
#endif /* _FILESYSTEM_H */


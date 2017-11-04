#include "filesystem.h"
#include "lib.h"
#include "rtc.h"
#include "keyboard.h"
#include "syscall.h"
#include "task.h"
#include "terminal.h"

/* FS constants */
#define BLOCK_SIZE 4096
#define FILE_TYPE_RTC 0
#define FILE_TYPE_DIR 1
#define FILE_TYPE_REG 2
#define MAX_FILE_NUM 62
#define MAX_DIR_LEN 62*32
#define BOOT_BLOCK_SKIP 1

/* bitmasks for the file descriptor flags field */
#define CLEAR_ALL_FLAGS 0x00
#define FLAG_IN_USE 0x01
#define FLAG_RTC 0x02
#define FLAG_FILE 0x04
#define FLAG_DIR 0x08
#define FILE_ARRAY_LENGTH 8
#define MAX_DATA_BLOCK 200
#define MAX_INODE 22

/* FS info struct */
filesystem_info_t filesystem_info;

/* rtc fops table */
static operations_t rtc_operations = {
	rtc_open,
	rtc_read,
	rtc_write,
	rtc_close
};

/* directory fops table */
static operations_t dir_operations= {
	directory_open,
	directory_read,
	directory_write,
	directory_close
};

/* file fops table */
static operations_t file_operations= {
	regular_file_open,
	regular_file_read,
	regular_file_write,
	regular_file_close
};

//set of operations
//==================================
/*
 * filesystem_read
 *   DESCRIPTION: read the file to the buffer for nbytes bytes
 *   INPUTS:
 *			fd: index in the file array - syscall validates if fd is valid
 *
 *			nbytes: length in bytes to be read
 *   OUTPUTS: buf: destination of the data
 *   RETURN VALUE: a nonnegative value if number of bytes were read, -1 for fail
 *   SIDE EFFECTS: pos advanced for that fd by number of bytes read
 */
int32_t filesystem_read(int32_t fd, void* buf, int32_t nbytes) {
	if (fd < 0 || fd >= FD_MAX)
        return ERR;

	if(buf == NULL) {
		return ERR;
	}

	return current_pcb[active_task_idx]->file_descriptors[fd].
		operations.read(fd, (void*)buf, nbytes);
}


/*
 * filesystem_write
 *   DESCRIPTION: Not implemented. FS is read only.
 *   INPUTS:
 *			fd: index in the file array
 *			buf: source of the data
 *			nbytes: length in bytes to be write
*
*				All arguments are ignored - see description
 *   OUTPUTS: none
 *   RETURN VALUE: always fails, -1 for fail
 *   SIDE EFFECTS: none
 */
int32_t filesystem_write(int32_t fd, const void* buf, int32_t nbytes) {
	return ERR;
}


/*
 * filesystem_open
 *   DESCRIPTION: open the file with the fname, assign correct set of functions
 *								to the descriptor jump table
 *   INPUTS: fname: filename
 *   OUTPUTS: none
 *   RETURN VALUE: file array idx for success, -1 for fail
 *   SIDE EFFECTS: file array updated
 */
int32_t filesystem_open(const uint8_t* fname) {
    int i;
	dentry_t dentry;

	/* get rid of leading ./ or leading / */
	int8_t* fname_normalized[FILE_NAME_LEN];
	if(validate_filename_and_trunc((int8_t*)fname,
		(int8_t*)fname_normalized) == ERR) return ERR;

	/* find a open file descriptor */
	for (i = 0; (current_pcb[active_task_idx]->file_descriptors[i].flags & FLAG_IN_USE) &&
		i < FILE_ARRAY_LENGTH; ++i);

	/* cannot open any more files, there exist no free file descriptors */
	if (i == FILE_ARRAY_LENGTH)
		return ERR;

	/* file not found */
    if (read_dentry_by_name((const int8_t*)fname_normalized, &dentry) == ERR) {
    	return ERR;
    }


    // Set flags based on file type - dev file, dir file, or normal file
    if (dentry.file_type == TYPE_RTC) {
        int j;

        /* Has RTC has already been opened? */
        for (j = 0; j < FILE_ARRAY_LENGTH; ++j) {
            if (current_pcb[active_task_idx]->file_descriptors[j].flags & FLAG_RTC)
                return ERR;
        }

        /* set the jump table and other status values based on the type of the
        file */
        current_pcb[active_task_idx]->file_descriptors[i].operations = rtc_operations;
    	current_pcb[active_task_idx]->file_descriptors[i].inode = NULL;
    	current_pcb[active_task_idx]->file_descriptors[i].pos = 0;
    	current_pcb[active_task_idx]->file_descriptors[i].flags = FLAG_RTC | FLAG_IN_USE;
    } else if(dentry.file_type == TYPE_DIR) {
    	current_pcb[active_task_idx]->file_descriptors[i].operations = dir_operations;
    	current_pcb[active_task_idx]->file_descriptors[i].inode = filesystem_info.first_inode_ptr +
    		dentry.num_inode;
    	current_pcb[active_task_idx]->file_descriptors[i].pos = 0;
    	current_pcb[active_task_idx]->file_descriptors[i].flags = FLAG_DIR | FLAG_IN_USE;
    } else if(dentry.file_type == TYPE_FILE) {
    	current_pcb[active_task_idx]->file_descriptors[i].operations = file_operations;
    	current_pcb[active_task_idx]->file_descriptors[i].inode = filesystem_info.first_inode_ptr +
    		dentry.num_inode;
    	current_pcb[active_task_idx]->file_descriptors[i].pos = 0;
    	current_pcb[active_task_idx]->file_descriptors[i].flags = FLAG_FILE | FLAG_IN_USE;
    }

    /* perform file type specific initalization */
	int32_t retval = current_pcb[active_task_idx]->file_descriptors[i].operations.open(fname_normalized);

	if(retval != SUCCESS) {
		/* close the file descriptor if the device specific initalization
		failed */
		current_pcb[active_task_idx]->file_descriptors[i].flags = CLEAR_ALL_FLAGS;
		return ERR;
	}
	return i;
}


/*
 * filesystem_close
 *   DESCRIPTION: close the file
 *   INPUTS: fd: index in the file array
 *   OUTPUTS: none
 *   RETURN VALUE: 0 for success, -1 for fail
 *   SIDE EFFECTS: none
 */
int32_t filesystem_close(int32_t fd) {
	if (fd < 0 || fd >= FD_MAX)
        return ERR;
	/* perform file type specific close */
	if(current_pcb[active_task_idx]->file_descriptors[fd].operations.close(fd) != SUCCESS) {
		/* leave the file descriptor open if the device specific close
		failed */
		return ERR;
	} else {
		current_pcb[active_task_idx]->file_descriptors[fd].flags = CLEAR_ALL_FLAGS;
		return SUCCESS;
	}
}
//==================================
//set of operations done




//set of operations for regular file
//==================================
/*
 * regular_file_read
 *   DESCRIPTION: read the file to the buffer for nbytes bytes - regular file
 *   INPUTS:
 *			fd: index in the file array
 *
 *			nbytes: length in bytes to be read
 *   OUTPUTS: buf: destination of the data
 *   RETURN VALUE: 0 for success, -1 for fail
 *   SIDE EFFECTS: pos advanced
 */
int32_t regular_file_read(int32_t fd, void* buf, int32_t nbytes) {
	if (fd < 0 || fd >= FD_MAX)
        return ERR;

	if(!buf) {
		return ERR;
	}

	uint32_t num_bytes_read;
	if(((current_pcb[active_task_idx]->file_descriptors[fd]).flags)&FLAG_DIR) {	//is dir
		return ERR;
	}else if(((current_pcb[active_task_idx]->file_descriptors[fd]).flags)&FLAG_FILE) {	//is regular file
		num_bytes_read=read_regular_file((current_pcb[active_task_idx]->file_descriptors[fd]).inode,
			(uint32_t)((current_pcb[active_task_idx]->file_descriptors[fd]).pos), (uint8_t*)buf,
			(uint32_t)nbytes);
		(current_pcb[active_task_idx]->file_descriptors[fd]).pos+=num_bytes_read;
	}else {
		/* not a filesystem file or flags incorrect */
		return ERR;
	}
	return num_bytes_read;
}


/*
 * regular_file_write
 *   DESCRIPTION: write to the file from the buffer for nbytes bytes,
 *				  regular file specific ops - FS read only so always fails
 *   INPUTS:
 *			fd: index in the file array
 *			buf: source of the data
 *			nbytes: length in bytes to be write
 *   OUTPUTS: none
 *   RETURN VALUE: 0 for success, -1 for fail
 *   SIDE EFFECTS: none
 */
int32_t regular_file_write(int32_t fd, const void* buf, int32_t nbytes) {
	if (fd < 0 || fd >= FD_MAX)
        return ERR;

	if(buf == NULL) {
		return ERR;
	}

	uint32_t num_bytes_write;

	filesystem_info.boot_block_ptr->num_data_blocks+=1;

	if(((current_pcb[active_task_idx]->file_descriptors[fd]).flags)&FLAG_DIR) {	//is dir
		return ERR;
	}else if(((current_pcb[active_task_idx]->file_descriptors[fd]).flags)&FLAG_FILE) {	//is regular file
		num_bytes_write=write_regular_file((current_pcb[active_task_idx]->file_descriptors[fd]).inode,
			(uint32_t)((current_pcb[active_task_idx]->file_descriptors[fd]).pos), (const uint8_t*)buf,
			(uint32_t)nbytes);
		(current_pcb[active_task_idx]->file_descriptors[fd]).pos+=num_bytes_write;
	}else {
		/* not a filesystem file or flags incorrect */
		return ERR;
	}
	return num_bytes_write;
}


/*
 * regular_file_open
 *   DESCRIPTION: open a file with the filename fname, regular file specific ops
 *   INPUTS:
 *			fname: filename
 *   OUTPUTS: none
 *   RETURN VALUE: 0 for success, -1 for fail
 *   SIDE EFFECTS: none
 */
int32_t regular_file_open(const uint8_t* fname) {
	if(fname == NULL) return ERR;
	return SUCCESS;
}


/*
 * regular_file_close
 *   DESCRIPTION: close the file with the fd - regular file specific ops
 *   INPUTS:
 *			fd: file array idx
 *   OUTPUTS: none
 *   RETURN VALUE: 0 for success, -1 for fail
 *   SIDE EFFECTS: none
 */
int32_t regular_file_close(int32_t fd) {
	if (fd < 0 || fd >= FD_MAX)
        return ERR;
	return SUCCESS;
}
//==================================
//set of operations for regular file done




//set of operations for directory
//==================================
/*
 * directory_read
 *   DESCRIPTION: read the file as its type to the buffer for nbytes bytes -
 *				  directory specific ops
 *   INPUTS:
 *			fd: index in the file array
 *
 *			nbytes: length in bytes to be read
 *   OUTPUTS: buf: destination of the data
 *   RETURN VALUE: 0 for success, -1 for fail
 *   SIDE EFFECTS: pos advanced
 */
int32_t directory_read(int32_t fd, void* buf, int32_t nbytes) {
	if(!buf) {
		return ERR;
	}

	if (fd < 0 || fd >= FD_MAX)
        return ERR;

	uint32_t num_bytes_read;
	if(((current_pcb[active_task_idx]->file_descriptors[fd]).flags)&FLAG_DIR) {	//is dir
		num_bytes_read=read_directory((uint32_t)((current_pcb[active_task_idx]->file_descriptors[fd]).pos),
			(uint8_t*)buf, (uint32_t)nbytes);
		(current_pcb[active_task_idx]->file_descriptors[fd]).pos+=FILE_NAME_LEN;	//because must read one name only

	}else if(((current_pcb[active_task_idx]->file_descriptors[fd]).flags)&FLAG_FILE) {	//is regular file
		return ERR;
	}else {
		/* not a filesystem file or flags incorrect */
		return ERR;
	}
	return num_bytes_read;
}


/*
 * directory_write
 *   DESCRIPTION: write to the file as its type from the buffer for nbytes bytes
 *				  - directory specific ops - always fails as FS is read only
 *   INPUTS: All arguments unused
 *			fd: index in the file array
 *			buf: source of the data
 *			nbytes: length in bytes to be write
 *   OUTPUTS: none
 *   RETURN VALUE: always fails with -1
 *   SIDE EFFECTS: none
 */
int32_t directory_write(int32_t fd, const void* buf, int32_t nbytes) {
	return ERR;
}


/*
 * directory_open
 *   DESCRIPTION: open the file with the filename - directory specific ops
 *   INPUTS: All arguments unused
 *			fname: filename
 *   OUTPUTS: none
 *   RETURN VALUE: 0 for success, -1 for fail
 *   SIDE EFFECTS: none
 */
int32_t directory_open(const uint8_t* fname) {
	if(fname == NULL) return ERR;
	return SUCCESS;
}


/*
 * directory_close
 *   DESCRIPTION: close the file with the fd - directory specific ops
 *   INPUTS: All arguments unused
 *			fd: file array idx
 *   OUTPUTS: none
 *   RETURN VALUE: 0 for success, -1 for fail
 *   SIDE EFFECTS: none
 */
int32_t directory_close(int32_t fd) {
	if (fd < 0 || fd >= FD_MAX)
        return ERR;
	return SUCCESS;
}
//==================================
//set of operations for directory done




//required by 3.2
//==================================
/*
 * read_dentry_by_name
 *   DESCRIPTION: get the dentry by the name
 *   INPUTS:
 *			fname: file name
 *   OUTPUTS: dentry: pointer of the destination
 *   RETURN VALUE: 0 for success, -1 for fail
 *   SIDE EFFECTS: none
 */
int32_t read_dentry_by_name (const int8_t* fname, dentry_t* dentry) {
	if(dentry == NULL || fname == NULL) return ERR;

	uint32_t i;

	/* normalize the filename */
	int8_t* fname_normalized[FILE_NAME_LEN];
	if(validate_filename_and_trunc((int8_t*)fname,
		(int8_t*)fname_normalized) == ERR) return ERR;
	//printf("normalized name: %s\n", fname_normalized);

	/* read each filename from the root directory and look for a match */
	for (i=0; i<NUM_INODE; i++) {
		if ((strncmp((const int8_t*)fname_normalized,
			filesystem_info.boot_block_ptr->dentries[i].file_name,
			strlen((const int8_t*)fname_normalized)) == SUCCESS) &&
			(strncmp((const int8_t*)fname_normalized,
				filesystem_info.boot_block_ptr->dentries[i].file_name,
				strlen(filesystem_info.boot_block_ptr->dentries[i].file_name))
				 == SUCCESS)) {
			/* if found, copy the directory entry into the user supplied
			buffer */
			memcpy((void*)dentry,
				(void*)&(filesystem_info.boot_block_ptr->dentries[i]),
				sizeof(dentry_t));
			return SUCCESS;
		}
	}
	return ERR;
}


/*
 * read_dentry_by_index
 *   DESCRIPTION: get the dentry by the index
 *   INPUTS:
 *			index: index of the file
 *
 *   OUTPUTS: dentry: pointer of the destination
 *   RETURN VALUE: 0 for success, -1 for fail
 *   SIDE EFFECTS: none
 */
int32_t read_dentry_by_index (uint32_t index, dentry_t* dentry) {
	if(dentry == NULL) return ERR;

	/* check that the inode number is valid */
	if (filesystem_info.boot_block_ptr->dentries[index].file_name==0 ||
		filesystem_info.boot_block_ptr->dentries[index].num_inode < 0 ||
		filesystem_info.boot_block_ptr->dentries[index].num_inode >=
		filesystem_info.boot_block_ptr->num_inodes) {
		return ERR;
	}

	memcpy((void*)dentry, (void*)&
		(filesystem_info.boot_block_ptr->dentries[index]), sizeof(dentry_t));
	return SUCCESS;
}


/*
 * read_data
 *   DESCRIPTION: read data from the given inode into a buffer in memory
 *   INPUTS:
 *			inode: inode of the file
 *			offset: starting address to be read
 *			length: length in bytes to be read
 *   OUTPUTS: buf: destination of the data
 *   RETURN VALUE: length of the data read
 *   SIDE EFFECTS: none
 */
int32_t read_data (uint32_t inode, uint32_t offset, uint8_t* buf,
	uint32_t length) {
	if(buf == NULL) return ERR;

	inode_t* cur_inode_ptr=(filesystem_info.first_inode_ptr+inode);
	/* printf("cur_inode_ptr->length_in_byte: %d\n",
		cur_inode_ptr->length_in_byte); */
	if(offset>=cur_inode_ptr->length_in_byte) {
		return SUCCESS;
	} //end of the file reached

	uint32_t read_len=0;
	uint32_t cur_byte_idx=offset+read_len;
	uint32_t cur_block_idx=0;
	uint8_t* cur_block_ptr=NULL;

	/* copy the file to the local buffer one byte at a time and increment
	counter of number of bytes read */

	while((read_len<length)&&(cur_byte_idx<(cur_inode_ptr->length_in_byte))) {
		cur_block_idx=(cur_byte_idx)/BLOCK_SIZE;

		/* check the data block is within bounds */
		if (cur_inode_ptr->data_blocks[cur_block_idx] < 0 ||
			cur_inode_ptr->data_blocks[cur_block_idx] >=
			filesystem_info.boot_block_ptr->num_data_blocks) {
			return ERR;
		}

		/* read blocks */
		cur_block_ptr=(uint8_t*)(filesystem_info.disk_start_addr+BLOCK_SIZE*
			(BOOT_BLOCK_SKIP+(filesystem_info.boot_block_ptr)->num_inodes +
				cur_inode_ptr->data_blocks[cur_block_idx]));
		memcpy(buf+read_len, cur_block_ptr+(cur_byte_idx%BLOCK_SIZE),
			sizeof(uint8_t));

		/* increment number of bytes read and advance pointer */
		++read_len;
		cur_byte_idx=offset+read_len;
	}

	return read_len;
}
//==================================
//required by 3.2 done




//helper for filesystem_read
//==================================
/*
 * read_regular_file
 *   DESCRIPTION: read the regular file
 *   INPUTS:
 *			cur_inode_ptr: ptr to the inode of the file
 *			offset: starting address to be read
 *
 *			length: length in bytes to be read
 *   OUTPUTS: buf: destination of the data
 *   RETURN VALUE: length of the data read
 *   SIDE EFFECTS: none
 */
int32_t read_regular_file(inode_t* cur_inode_ptr, uint32_t offset, uint8_t* buf,
	uint32_t length) {
	if(buf == NULL || cur_inode_ptr == NULL) return ERR;

	if(offset>=cur_inode_ptr->length_in_byte) {
		return 0;
	} //end of the file reached, no bytes read

	uint32_t read_len=0;
	uint32_t cur_byte_idx=offset+read_len;
	uint32_t cur_block_idx=0;
	uint8_t* cur_block_ptr=NULL;

	/* copy the file to the local buffer one byte at a time and increment
	counter of number of bytes read */
	while((read_len<length)&&(cur_byte_idx<(cur_inode_ptr->length_in_byte))) {
		cur_block_idx=(cur_byte_idx)/BLOCK_SIZE;

		/* check the data block is within bounds */
		if (cur_inode_ptr->data_blocks[cur_block_idx] < 0 ||
			cur_inode_ptr->data_blocks[cur_block_idx] >=
			filesystem_info.boot_block_ptr->num_data_blocks) {
			return ERR;
		}

		/* read blocks */
		cur_block_ptr=(uint8_t*)(filesystem_info.disk_start_addr+
			BLOCK_SIZE*(BOOT_BLOCK_SKIP+(filesystem_info.boot_block_ptr)->
				num_inodes + cur_inode_ptr->data_blocks[cur_block_idx]));
		memcpy(buf+read_len, cur_block_ptr+(cur_byte_idx%BLOCK_SIZE),
			sizeof(uint8_t));

		/* increment number of bytes read and advance pointer */
		++read_len;
		cur_byte_idx=offset+read_len;
	}
	return read_len;
}


/*
 * read_directory
 *   DESCRIPTION: read the directory
 *   INPUTS:
 *			cur_inode_ptr: ptr to the inode of the file
 *			offset: starting address to be read
 *
 *			length: length in bytes to be read
 *   OUTPUTS: buf: destination of the data
 *   RETURN VALUE: length of the data read
 *   SIDE EFFECTS: none
 */
int32_t read_directory(uint32_t offset, uint8_t* buf, uint32_t length) {
	uint8_t local_buf[MAX_DIR_LEN];
	int32_t cur=0;
	int32_t i;

	if(buf == NULL) return ERR;

	/* look for the matching filename */
	for(i=0; i<=MAX_FILE_NUM; ++i) {
		if(!((filesystem_info.boot_block_ptr->dentries[i].file_type==0) &&
			(filesystem_info.boot_block_ptr->dentries[i].num_inode==0))) {
			int j = 0;

			/* copy the directory to a local buffer */
			while((filesystem_info.boot_block_ptr->dentries[i].file_name)[j]
				!= '\0') {
				local_buf[cur]=(uint8_t)
					((filesystem_info.boot_block_ptr->dentries[i].file_name)[j]);
				j++;
				cur++;
			}

			for(j=j; j<FILE_NAME_LEN; ++j) {
				local_buf[cur]=(uint8_t)('\0');
				cur++;
			}
		}
	}

	if(length>FILE_NAME_LEN) {
		length=FILE_NAME_LEN;
		//each read dir call return only one dir according to doc
	}

	if(offset >= cur) {
		return 0;
	} //end of the directory reached, no bytes read

	uint32_t read_len=0;
	uint32_t cur_byte_idx = offset+read_len;

	/* copy the file to the local buffer one byte at a time and increment
	counter of number of bytes read */
	while((read_len<length)&&(cur_byte_idx<cur)) {
		memcpy(buf+read_len, local_buf+cur_byte_idx, sizeof(uint8_t));
		++read_len;
		cur_byte_idx=offset+read_len;
	}
	return read_len;
}


/*
 * validate_filename_and_trunc
 *   DESCRIPTION: cap the name to be length of 32, and remove leading ../, ./, /
 *   INPUTS: fname: file name
 *   OUTPUTS: fname_truncated: file name truncated and normalized
 *   RETURN VALUE: 0 for success, -1 for fail
 *   SIDE EFFECTS: fname_truncated written to
 */
int32_t validate_filename_and_trunc(int8_t* fname, int8_t* fname_truncated) {
	int8_t* fname_normalized;

	if(fname == NULL || fname_truncated == NULL) {
		return ERR;
	}

	fname_normalized = fname;

	/* get rid of leading extraneous path parts */
	if(!strncmp(fname_normalized, "../", strlen("../"))) {
		fname_normalized += strlen("../");
	} else if(!strncmp(fname_normalized, "./", strlen("./"))) {
		fname_normalized += strlen("./");
	} else if(!strncmp(fname_normalized, "/", strlen("/"))) {
		fname_normalized += strlen("/");
	}

	/* copy the normalized string to the output pointer */
	memcpy(fname_truncated, fname_normalized, FILE_NAME_LEN-1);
	*(fname_truncated + FILE_NAME_LEN) = NULL;

	return SUCCESS;
}
//==================================
//helper for filesystem_read done


//filesystem test functions
//==================================
/*
 * print_filesystem_info
 *   DESCRIPTION: print the filesystem's info
 *   INPUTS: none
 *   OUTPUTS: info of the filesystem
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
void print_filesystem_info() {
	printf("num_dir_entries is: %d\n",
		filesystem_info.boot_block_ptr->num_dir_entries);
	printf("num_inodes is: %d\n", filesystem_info.boot_block_ptr->num_inodes);
	printf("num_data_blocks is: %d\n",
		filesystem_info.boot_block_ptr->num_data_blocks);
	return;
}


/*
 * filesystem_test_1_by_id
 *   DESCRIPTION: print the data in the file
 *   INPUTS: idx: idx of the file
 *   OUTPUTS: data in the file
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
void filesystem_test_1_by_id(uint32_t idx) {
	printf("------Filesystem test 1: print out a file by id------\n");
	dentry_t cur_dentry;

	/* try to read the file */
	int res=read_dentry_by_index(idx, &cur_dentry);
	if(res==ERR) {
		printf("No file found!\n");
		return;
	}
	printf("length: %d\ninode idx: %d\nname: %s\n",
		(filesystem_info.first_inode_ptr+cur_dentry.num_inode)->length_in_byte,
		cur_dentry.num_inode, cur_dentry.file_name);
	uint8_t buf[LARGE_BUFFER];
	res=read_data(cur_dentry.num_inode, 0, buf, LARGE_BUFFER);
	buf[LARGE_BUFFER-1]='\0'; //make the string null terminated
	printf("bytes read: %d\n %s\n", res, buf);
	printf("-----------Filesystem test 1 done-----------------\n");
	return;
}


/*
 * filesystem_test_1_by_name
 *   DESCRIPTION: print the data in the file
 *   INPUTS: fname: filename of the file
 *   OUTPUTS: none
 *   RETURN VALUE: data in the file
 *   SIDE EFFECTS: none
 */
void filesystem_test_1_by_name(const int8_t* fname) {
	printf("------Filesystem test 1: print out a file by filename------\n");
	dentry_t cur_dentry;

	/* try to read the file */
	int res=read_dentry_by_name(fname, &cur_dentry);
	if(res==ERR) {
		printf("No file found!\n");
		return;
	}
	printf("length: %d\ninode idx: %d\nname: %s\n",
		(filesystem_info.first_inode_ptr+cur_dentry.num_inode)->length_in_byte,
		cur_dentry.num_inode, cur_dentry.file_name);
	uint8_t buf[LARGE_BUFFER];
	res=read_data(cur_dentry.num_inode, 0, buf, LARGE_BUFFER);
	buf[LARGE_BUFFER-1]='\0'; //make the string null terminated
	printf("bytes read: %d\n %s\n", res, buf);
	printf("-----------Filesystem test 1 done-----------------\n");
	return;
}


/*
 * filesystem_test_2_by_id
 *   DESCRIPTION: print the length of the file
 *   INPUTS: idx: idx of the file
 *   OUTPUTS: none
 *   RETURN VALUE: length of the file in bytes
 *   SIDE EFFECTS: none
 */
void filesystem_test_2_by_id(uint32_t idx) {
	printf("----Filesystem test 2: print out size of a file by id------\n");
	dentry_t cur_dentry;

	/* try to read the file */
	int res=read_dentry_by_index(idx, &cur_dentry);
	if(res==ERR) {
		printf("No file found!\n");
		return;
	}

	printf("cur file length in bytes: %d\n",
		(filesystem_info.first_inode_ptr+cur_dentry.num_inode)->length_in_byte);
	printf("-----------Filesystem test 2 done-----------------\n");
	return;
}


/*
 * filesystem_test_2_by_name
 *   DESCRIPTION: print the length of the file
 *   INPUTS: fname: filename of the file
 *   OUTPUTS: none
 *   RETURN VALUE: number of bytes in the file
 *   SIDE EFFECTS: none
 */
void filesystem_test_2_by_name(const int8_t* fname) {
	printf("----Filesystem test 2: print out size of a file by filename----\n");

	dentry_t cur_dentry;

	/* try to read the directory entry */
	int res=read_dentry_by_name(fname, &cur_dentry);
	if(res==ERR) {
		printf("No file found!\n");
		return;
	}

	printf("cur file length in bytes: %d\n",
		(filesystem_info.first_inode_ptr+cur_dentry.num_inode)->length_in_byte);
	printf("-----------Filesystem test 2 done-----------------\n");
	return;
}


/*
 * filesystem_test_3
 *   DESCRIPTION: print all filenames
 *   INPUTS: none
 *   OUTPUTS: print all filenames
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
void filesystem_test_3() {
	printf("-------Filesystem test 3: print all filenames-------\n");
	int i=0;
	for(i=0; i<=MAX_FILE_NUM; ++i) {	//don't print the root dir
		if(!((filesystem_info.boot_block_ptr->dentries[i].file_type==0) &&
			(filesystem_info.boot_block_ptr->dentries[i].num_inode==0))) {
			printf("%d is: %s\n", i,
				filesystem_info.boot_block_ptr->dentries[i].file_name);
		}
	}
	printf("-----------Filesystem test 3 done-----------------\n");
}

/*
 * filesystem_test_4
 *   DESCRIPTION: reads directory names via read_directory
 *   INPUTS: none
 *   OUTPUTS: print all filenames to screen
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
void filesystem_test_4(int num) {
	printf("-------Filesystem test 4: read directory-------\n");
	int i=0;
	int j=0;
	int res=0;
	int fd = filesystem_open((uint8_t*)".");
	for(i=0; i<num; ++i) {
		uint8_t buf[MAX_DIR_LEN];

		/* read the directory using a specialized function */
		res=filesystem_read(fd, buf, MAX_DIR_LEN);

		/* should return 0 once all directories read */
		if(res==0) {
			printf("End of directory reached");
		}

		/* print the directory name */
		printf("%s\n", buf);

		/* clear the temporary buffer for the next iteration */
		for(j=0; j<MAX_DIR_LEN; ++j) {
			buf[j]='\0';
		}
	}
	printf("-----------Filesystem test 4 done-----------------\n");
	filesystem_close(fd);
	return;
}
//==================================
//filesystem test functions done


//filesystem write functions
//==================================


/*Because inode and data blocks are consecutive in the read only
 * system. These functions are reserved for potentially further implementation
 * of the more serious writable file system.
 */
static uint32_t inode_bit_map[MAX_INODE];
static uint32_t data_block_bit_map[MAX_DATA_BLOCK];
void init_write() {
	uint32_t cur_inode_idx=0;
	uint32_t cur_data_block_idx=0;
	uint32_t i=0;

	inode_bit_map[0]=1;
	data_block_bit_map[0]=1;
	for(i=0; i<MAX_INODE; ++i) {
		inode_bit_map[i]=0;
	}
	for(i=0; i<MAX_DATA_BLOCK; ++i) {
		data_block_bit_map[i]=0;
	}

	for(i=0; i<=MAX_FILE_NUM; ++i) {
		if(filesystem_info.boot_block_ptr->dentries[i].num_inode!=0) {
			inode_t* cur_inode_ptr=(filesystem_info.first_inode_ptr+filesystem_info.boot_block_ptr->dentries[i].num_inode);
			cur_inode_idx=filesystem_info.boot_block_ptr->dentries[i].num_inode;
			inode_bit_map[cur_inode_idx]=1;
			uint32_t j=0;
			for(j=0; j<NUM_INODE; ++j) {	//!!!!!!!magic number --fixed
				if((cur_inode_ptr->data_blocks[j])!=0) {
					cur_data_block_idx=cur_inode_ptr->data_blocks[j];
					data_block_bit_map[cur_data_block_idx]=1;
				}
			}
		}
	}

	return;
}


uint32_t get_next_dentry_idx() {
	uint32_t ret=0;
	uint32_t i=0;
	for(i=0; i<=MAX_FILE_NUM; ++i) {
		if((filesystem_info.boot_block_ptr->dentries[i].file_type==0) && (filesystem_info.boot_block_ptr->dentries[i].num_inode==0)) {
			ret=i;
			return ret;
		}
	}
	return 0;
}


uint32_t require_next_inode_idx() {
	uint32_t ret=-1;
	uint32_t i=0;
	for(i=0; i<MAX_INODE; ++i) {
		if(inode_bit_map[i]==0) {
			inode_bit_map[i]=1;
			ret=i;
			return ret;
		}
	}
	return ret;
}


uint32_t require_next_data_block_idx() {
	uint32_t ret=-1;
	uint32_t i=0;
	for(i=0; i<MAX_DATA_BLOCK; ++i) {
		if(data_block_bit_map[i]==0) {
			data_block_bit_map[i]=1;
			ret=i;
			return ret;
		}
	}
	return ret;
}


int32_t filesystem_touch(const uint8_t* fname) {
	if(fname==NULL) {
		return ERR;
	}

	uint32_t dentry_idx=get_next_dentry_idx();

	uint32_t inode_idx=require_next_inode_idx();
	if(inode_idx==-1) {
		return ERR;
	}

	//setup dentry
	int i=0;
	while((fname[i]!='\0')&&(i<FILE_NAME_LEN)) {
		filesystem_info.boot_block_ptr->dentries[dentry_idx].file_name[i]=fname[i];
		i+=1;
	}
	filesystem_info.boot_block_ptr->dentries[dentry_idx].file_type=TYPE_FILE;
	filesystem_info.boot_block_ptr->dentries[dentry_idx].num_inode=inode_idx;
	//filesystem_info.boot_block_ptr->num_data_blocks+=1;

	//setup inode
	inode_t* inode_ptr=(inode_t*)(filesystem_info.first_inode_ptr+inode_idx);
	inode_ptr->length_in_byte=0;

	return SUCCESS;
}


int32_t write_regular_file(inode_t* cur_inode_ptr, uint32_t offset, const uint8_t* buf, uint32_t length) {
	if(buf == NULL || cur_inode_ptr == NULL) return ERR;

	if(length==0) {
		return 0;
	}

	uint32_t data_block_idx=-1;

	if(offset==0) {	//need to assign datablock
		data_block_idx=require_next_data_block_idx();
		cur_inode_ptr->data_blocks[0]=data_block_idx;	//4kb only
	}else {
		data_block_idx=cur_inode_ptr->data_blocks[0];	//4kb only
	}

	uint32_t write_len=0;
	uint32_t cur_byte_idx=offset+write_len;
	uint8_t* data_block_ptr=(uint8_t*)(filesystem_info.disk_start_addr+
			BLOCK_SIZE*(BOOT_BLOCK_SKIP+(filesystem_info.boot_block_ptr)->
				num_inodes + cur_inode_ptr->data_blocks[0]));

	/* copy the file to the local buffer one byte at a time and increment
	counter of number of bytes written */
	while((write_len<length)&&(cur_inode_ptr->length_in_byte<BLOCK_SIZE)) {
		/* writing blocks */
		data_block_ptr=(uint8_t*)(filesystem_info.disk_start_addr+
			BLOCK_SIZE*(BOOT_BLOCK_SKIP+(filesystem_info.boot_block_ptr)->
				num_inodes + cur_inode_ptr->data_blocks[data_block_idx]));
		memcpy(data_block_ptr+(cur_byte_idx%BLOCK_SIZE), buf+write_len, sizeof(uint8_t));
		/* increment number of bytes written and advance pointer */
		++write_len;
		cur_byte_idx=offset+write_len;
		cur_inode_ptr->length_in_byte+=1;
	}

	return write_len;
}


int32_t create_directory(const uint8_t* fname) {
	if(fname==NULL) {
		return ERR;
	}

	uint32_t dentry_idx=get_next_dentry_idx();

	uint32_t inode_idx=require_next_inode_idx();
	if(inode_idx==-1) {
		return ERR;
	}

	//setup dentry
	int i=0;
	while((fname[i]!='\0')&&(i<FILE_NAME_LEN)) {
		filesystem_info.boot_block_ptr->dentries[dentry_idx].file_name[i]=fname[i];
		i+=1;
	}
	filesystem_info.boot_block_ptr->dentries[dentry_idx].file_type=TYPE_DIR;
	filesystem_info.boot_block_ptr->dentries[dentry_idx].num_inode=inode_idx;

	//setup inode
	inode_t* inode_ptr=(inode_t*)(filesystem_info.first_inode_ptr+inode_idx);
	inode_ptr->length_in_byte=0;

	return SUCCESS;
}


//funcitons to be called in the kernel
//==================================
/*
 * init_filesystem
 *   DESCRIPTION: initialize the filesystem
 *   INPUTS: disk_start_addr: starting address of the file image
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
void init_filesystem (uint32_t disk_start_addr) {
	/* put all the pointers to key data blocks into memory */
	filesystem_info.disk_start_addr=disk_start_addr;
	filesystem_info.boot_block_ptr=(boot_block_t*)disk_start_addr;
	filesystem_info.first_inode_ptr=(inode_t*)(disk_start_addr + BLOCK_SIZE);
	init_write();
}


/*
 * test_filesystem
 *   DESCRIPTION: call test functions
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: file descriptor array modified
 */
void test_filesystem() {
	print_filesystem_info();
	// filesystem_test_2_by_id(1);
	// filesystem_test_2_by_name("frame0.txt");
	// filesystem_test_2_by_name("./frame0.txt");
	// filesystem_test_2_by_name("/frame0.txt");
	// filesystem_test_2_by_name("../frame0.txt");
	// filesystem_test_2_by_name(NULL);
	// filesystem_test_4(20);
	// read_directory(NULL, NULL, NULL);
	// filesystem_test_1_by_name("frame1.txt");
	// filesystem_test_1_by_id(13);
	// filesystem_test_2_by_id(1);
	// filesystem_test_2_by_name("frame0.txt");
	//filesystem_test_3();
	return;
}
//==================================
//functions to be called in the kernel done

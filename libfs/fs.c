#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define SUCCESS 0
#define ERROR -1
#define SUPERBLOCK_PADDING 4079
#define ROOT_PADDING 10
#define FAT_EOC 255
#define ECS150FS 0x5346303531534345

// Super block
typedef struct
{
	uint64_t signature;       //signature. MUST BE EQUAL TO "ECS150FS"
	uint16_t disk_blocks;     //total ammount of blocks of virtual disk
	uint16_t root;            //root directory block index
	uint16_t data_start;      //data block start index
	uint16_t data_blocks;     //amount of data block
	uint8_t  fat_blocks;      //number of blocks for FAT
	uint8_t  padding[SUPERBLOCK_PADDING];		//unused padding
} __attribute__((packed)) SuperBlock;

// FAT 
typedef struct
{
	uint16_t *file_array;
}__attribute__((packed)) FAT;

// A Node of Root Directory
typedef struct
{
	uint8_t filename[FS_FILENAME_LEN];  //max filename size is 16
	uint32_t file_size;					//file's size
	uint16_t first_block_id;     		//index of the first data block
	uint8_t padding[ROOT_PADDING];		//unused padding
}__attribute__((packed)) RD_Node;

// Root Directory Table
typedef struct 
{
	RD_Node table[FS_FILE_MAX_COUNT];	//Max is 128 files. No subdirectories
} RD;

//A node of file descriptor
typedef struct {
    char filename[FS_FILENAME_LEN];		
    size_t file_offset;
} FD_Node;

// file descriptor table
typedef struct
{
	FD_Node list[FS_OPEN_MAX_COUNT];
} FD;

//
// End struct declaration
//

SuperBlock sb;
FAT fat;
RD rootdir;
FD open_files;

int free_blocks_count;
int free_files_count;

//
// helper functions
//

// Assumes that the disk is mounted!!
// Returns -1 if the file isn't opened or doesn't exist.
// Otherwise, returns the index of the file node in root directory
static int rd_find(int fd)
{
	char* filename = open_files.list[fd].filename;
	if(filename[0] == '\0')
	{
		return -1;
	}

	int i;
	for(i = 0; i < FS_FILE_MAX_COUNT; i++)
	{
		if(rootdir.table[i].filename[0] != '\0' && 
			strcmp((char*)rootdir.table[i].filename, filename) == 0)
		{
			return i;
		}
	}

	return -1;
}

// Returns -1 if not disk is loaded; otherwise, returns 0;
static int is_disk_mounted()
{
	if(sb.signature == 0)
	{
	        return -1;
	}

	return 0;
}

static int find_free_block()
{
	int fat_length = block_disk_count();
	int i;
	uint16_t retVal = FAT_EOC;
	for(i = 0; i < fat_length && retVal == FAT_EOC; i++)
	{
		if(fat.file_array[i] == 0)
		{
			retVal = i;
			return retVal;
		}
	}

	return retVal;
}

//Returns how many free blocks are in the disk
// Assumes that disk is mounted.
static int count_free_blocks()
{
	int i;
	int block_count = block_disk_count();
	int free = 0;
	for(i = 0; i < block_count; i++)
	{
		if(fat.file_array[i] == 0)
			free++;
	}
	return free;
}

//returns the current fat index of according the file offset.
static int find_block_from_offset(int start_id, int offset)
{
	int i;
	int hops = offset/BLOCK_SIZE + (offset % BLOCK_SIZE != 0);
	for(i = start_id; i != FAT_EOC && hops-- > 0; i = fat.file_array[i]);
	return i;	
}

static int find_end_block(int start_id)
{
	int i, prev;
	for(i = start_id; i != FAT_EOC; i = fat.file_array[i])
	{
		prev = i;
	}
	return prev;	
}

// Counts how many blocks the file occupies. 
// 
// static int count_file_blocks(int rd_table_id)
// {
// 	int i;
// 	int start_id = rootdir.table[rd_table_id].first_block_id;
// 	int block_count = block_disk_count();
// 	int count = 0;
// 	for(i = start_id; i < block_count && i != FAT_EOC; i = fat.file_array[i])
// 	{	
// 		printf("File spans block %d\n", i);
// 		count++;
// 	}
// 	return count;
// }

static int is_file_open(const char* filename)
{
	int i;
	for(i = 0; i < FS_OPEN_MAX_COUNT; i++)
	{
		if(strncmp(filename, open_files.list[i].filename, 16) == 0)
		{
			return 0;
		}
	}

	return -1;
}

//
// end helper functions
//

int fs_mount(const char *diskname)
{

	if (diskname == NULL)
	{
		fprintf(stderr, "diskname == NULL\n");
		return -1;
	}

	if (strcmp(diskname, "") == 0)
	{
		fprintf(stderr, "diskname == \"\"\n");
		return -1;
	}

	if (block_disk_open(diskname) == -1)
	{
		fprintf(stderr, "Error openning disk\n");
	    return -1;
	}

	if (block_read(0, &sb) == -1)
	{
		fprintf(stderr, "Error reading SuperBlock\n");
	    return -1;
	}

	// ECS15FS is a defined macro
	if (sb.signature != ECS150FS)
	{
		fprintf(stderr, "Disk's signature doesn't match ECS150FS\n");
	    return -1;
	}

	fat.file_array =  malloc(sb.fat_blocks * BLOCK_SIZE);

	if (fat.file_array == NULL)
	{
		return -1;
	}

	for (int i = 0; i < sb.fat_blocks; i++)
	{
		if (block_read((i + 1), fat.file_array + (2048 * i)) == -1)
		{
			return -1;
		}
	}

	if (block_read(sb.root, rootdir.table) == -1)
	{
		return -1;
	}

	for (int i = 0; i < sb.data_blocks; i++)
	{
		if (fat.file_array[i] == 0)
			free_blocks_count++;
	}

	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (rootdir.table[i].filename[0] == '\0')
			free_files_count++;
	}

	return 0;
}

int fs_umount(void)
{

	if (sb.signature == 0)
	{
		perror("fs_unmount(): signature mismatch.\n");
		return -1;
		
	}

	if (block_write(0, &sb))
	{
		perror("fs_unmount(): cannot flush superblock.\n");
		return -1;
	}

	for (int i = 0; i < FS_OPEN_MAX_COUNT; i++)
	{
		if (open_files.list[i].filename[0] != '\0')
		{	
			perror("fs_unmount(): File descriptor table not empty.\n");
			return -1;
		}
	}

	for (int i = 0; i < sb.fat_blocks; i++)
	{
		if (block_write((i + 1), fat.file_array + (2048 * i)) == -1)
		{
			perror("fs_unmount(): failed to flush fat table.\n");
			return -1;
		}
	}

	if (block_write(sb.root, &rootdir.table) == -1) {
		perror("fs_unmount(): fail writing to root dir.\n");
		return -1;
	}

	
	if (block_disk_close() == -1)
	{	
		perror("fs_unmount(): fail to close disk.\n");
		return -1;
	}

	// closes out proces
	sb.signature = 0;
	return 0;
}

int fs_info(void)
{
	if (sb.signature == 0)
	{
	 // no virtual disk loaded
	        return -1;
	}

	printf("FS Info:\n");
	printf("total_blk_count=%d\n", sb.disk_blocks);
	printf("fat_blk_count=%d\n", sb.fat_blocks);
	printf("rdir_blk=%d\n", sb.root);
	printf("data_blk=%d\n", sb.data_start);
	printf("data_blk_count=%d\n", sb.data_blocks);
	printf("fat_free_ratio=%d/%d\n", free_blocks_count, sb.data_blocks);
	printf("rdir_free_ratio=%d/128\n", free_files_count);

	return 0;
}

int fs_create(const char *filename)
{
	if(is_disk_mounted() < 0 )
		return -1;

	int fname_size = strlen(filename) + 1;
	if(fname_size > FS_FILENAME_LEN)
	{
		return -1; //invalid filename
	}

	int i;
	for(i = 0; i < FS_FILE_MAX_COUNT; i++)
	{
		//the file already exists
		if(strcmp((char*)rootdir.table[i].filename, filename) == 0)
		{
			return -1; 
		}
		//else we found an empty file slot
		else if(rootdir.table[i].filename[0] == '\0')
		{
			strcpy((char*)(rootdir.table[i].filename), filename);
			rootdir.table[i].file_size = 0;
			rootdir.table[i].first_block_id = (uint8_t)FAT_EOC;
			return 0;
		}
	}

  return -1; //root dir contains maximum files.
}

int fs_delete(const char *filename)
{
	if(is_disk_mounted() < 0)
	{
		perror("disk isn't mounted.\n");
		return -1;
	}

	if(is_file_open(filename) == 0)
	{
		// File is opened
		perror("file is opened\n");
		return -1;
	}
	
	if(filename == NULL)
	{
		perror("invalid filename\n");
		return -1;
	}

	int fname_size = strlen(filename) + 1;
	if(fname_size > FS_FILENAME_LEN)
	{
		perror("filename's length > FS_FILENAME_LEN\n");
		return -1; //invalid filename
	}

	int i;
	for(i = 0; i < FS_FILE_MAX_COUNT; i++)
	{
		//find match by filename.
		if(strcmp((char*)rootdir.table[i].filename, filename) == 0)
		{
			memset(rootdir.table[i].filename, 0, sizeof(rootdir.table[i].filename));
			rootdir.table[i].file_size = 0;
			rootdir.table[i].first_block_id = (uint8_t)FAT_EOC;
			return 0; 
		}
	}

	//didn't find the file in the rootdir
	return -1;
}

int fs_ls(void)
{
	if(sb.signature == 0)
	{
	 // no virtual disk loaded
	        return -1;
	}

	printf("FS Ls:\n");
	int i = 0;
	for(i = 0; i < FS_FILE_MAX_COUNT; i++)
	{
		if(rootdir.table[i].filename[0] != '\0')
		{
			char* filename = (char*)(rootdir.table[i].filename);
			int size = rootdir.table[i].file_size;
			int data_block = rootdir.table[i].first_block_id;

			printf("file: %s, size: %d, data_blk: %d\n", filename, size, data_block);
		}
	}
	return 0;
}

int fs_open(const char *filename)
{

	//bounds checking filename
	if ( (filename) == NULL ||
	strlen(filename) >= FS_FILENAME_LEN ||
	strcmp ("",filename) == 0 )
	{
		return -1;
	}

	int rd_table_id = -1;
	for (int i = 0; i< FS_FILE_MAX_COUNT; i++)
	{
		if (strcmp(filename, (char*)rootdir.table[i].filename) == 0)
		{
			rd_table_id = i;
		}
	}
	if(rd_table_id == -1)
	{
		return -1;
	}

	int fd = -1;
	for (int i = 0; i < FS_OPEN_MAX_COUNT; i++)
	{
		if (open_files.list[i].filename[0] == '\0')
		{
			fd = i;
			break;
		}
	}
	if(fd == -1)
	{
		return -1;
	}

	open_files.list[fd].file_offset = 0;
	strncpy(open_files.list[fd].filename, filename, FS_FILENAME_LEN);

	return fd;
}



int fs_close(int fd)
{

	// file descriptor invalid because it is non exitent/  less than 0.
	if (fd < 0)
	{	
		perror("fs_close(): fs < 0\n");
		return -1;
	}
	// file desciptor in valid because the number is more than we can open at once
	if (fd >= FS_OPEN_MAX_COUNT)
	{
		perror("fs_close(): fs >= FS_OPEN_MAX_COUNT\n");
		return -1;
	}
		// file descriptor not open
	if (open_files.list[fd].filename[0] == '\0')
	{
		perror("fs_close(): file doesn't exist\n");
		return -1;
	}

	// close file descriptor, return success
	open_files.list[fd].filename[0] = '\0';
	open_files.list[fd].file_offset = 0;
	return 0;
}

int fs_stat(int fd)
{
	if(is_disk_mounted() < 0)
		return -1;

	// file descriptor invalid because it is non exitent/  less than 0.
	if (fd < 0)
	{
		return -1;
	}
	// file desciptor in valid because the number is more than we can open at once
	if (fd >= FS_OPEN_MAX_COUNT)
	{
		return -1;
	}
		// file descriptor not open
	if (open_files.list[fd].filename[0] == '\0')
	{
		return -1;
	}

	int rd_table_id = rd_find(fd);
	if(rd_table_id == -1)
		return -1;

	int fd_size = rootdir.table[rd_table_id].file_size;

	if (fd_size < 0)
	{
		return -1;
	}

	return fd_size;
}

int fs_lseek(int fd, size_t offset)
{
	if(is_disk_mounted() < 0)
		return -1;

	// file descriptor invalid because it is non exitent/  less than 0.
	if (fd < 0)
	{
		return -1;
	}
	// file desciptor in valid because the number is more than we can open at once
	if (fd >= FS_OPEN_MAX_COUNT)
	{
		return -1;
	}
		// file descriptor not open
	if (open_files.list[fd].filename[0] == '\0')
	{
		return -1;
	}

	int rd_table_id = rd_find(fd);
	if(rd_table_id == -1)
		return -1;

	int f_size = rootdir.table[rd_table_id].file_size;
	open_files.list[fd].file_offset = offset;
	if(offset > f_size)
	{
		open_files.list[fd].file_offset = f_size;
		return 0;
	}

	return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
	if(is_disk_mounted() != SUCCESS)
	{
		return -1;
	}

	if(count == 0)
	{
		return 0;
	}

	int rd_id = rd_find(fd);
	if(rd_id < 0)
	{
		return -1;
	}

	int free_blocks = count_free_blocks();
	if(free_blocks == 0)
	{
		return 0;
	}


	FD_Node outf = open_files.list[fd];	
	size_t offset = outf.file_offset;

	uint8_t bounce_buf[BLOCK_SIZE];
	int bytes_left = count;

	//Allocated datablock for empty file
	if(rootdir.table[rd_id].first_block_id == FAT_EOC)
	{	
		// printf("allocating datablock for new file\n");
		uint16_t free_block_id = find_free_block();
		if(free_block_id == FAT_EOC)
		{
			return 0;
		}

		rootdir.table[rd_id].first_block_id = free_block_id;
		fat.file_array[free_block_id] = FAT_EOC;
	}

	int cur_block = find_block_from_offset(rootdir.table[rd_id].first_block_id, offset);
	int buf_offset = 0;
	int bounce_offset = 0;
	int bytes_to_write;
	int initial_offset = (offset % BLOCK_SIZE);
	int end_block = find_end_block(rootdir.table[rd_id].first_block_id);

	
	// printf("filename = %s\n", rootdir.table[rd_id].filename);
	// printf("first index = %d\n", rootdir.table[rd_id].first_block_id);
	// printf("rd_id = %d\n", rd_id);
	// printf("offset = %ld\n", offset);
	// printf("count = %ld\n", count);

	// if(offset + count <= BLOCK_SIZE)
	// {
	// 	memcpy(&bounce_buf[initial_offset], buf, count);
	// 	// printf("buf[0] = %c\n", ((char*)buf)[0]);
	// 	// printf("bounce_buf[0] = %c\n", (char)bounce_buf[0]);
	// 	block_write(cur_block + sb.data_start, bounce_buf);
	// 	rootdir.table[rd_id].file_size = rootdir.table[rd_id].file_size + count;
	// 	return count;
	// }
	
	if(count >= BLOCK_SIZE)
		bytes_to_write = BLOCK_SIZE;
	else
		bytes_to_write = count;

	// Core Logic. The Juicy Stuff is here.
	while(bytes_left > 0)
	{	
		// Reached end of chain. Try to Allocate more blocks. Update fat table.
		if(cur_block == FAT_EOC)
		{
			int free_block = find_free_block();
			if(free_block == FAT_EOC)
			{
				//exit loop cause we want to still want to update file size at the end
				break;  
			}

			//printf("end_block = %d\n", end_block);
			fat.file_array[end_block] = free_block;
			fat.file_array[free_block] = FAT_EOC;
			end_block = free_block;
			cur_block = free_block;
		}

		//special case: copying part of the block due to offset
		if(initial_offset > 0 )	
		{
			initial_offset = 0;
			block_read(cur_block + sb.data_start, bounce_buf);
			memcpy(bounce_buf + initial_offset, buf + buf_offset, BLOCK_SIZE - initial_offset);
			block_write(cur_block + sb.data_start, bounce_buf);

			bytes_left = bytes_left - BLOCK_SIZE - initial_offset;
			buf_offset = BLOCK_SIZE - initial_offset;

			if(bytes_left >= BLOCK_SIZE)
				bytes_to_write = BLOCK_SIZE;
			else
				bytes_to_write = bytes_left;
		}

		// regular operation, write one block at a time or less
		//  if bytes less in buf is < block size.
		else 
		{
			memcpy(bounce_buf, buf + buf_offset, bytes_to_write);
			block_write(cur_block + sb.data_start, bounce_buf);

			buf_offset = BLOCK_SIZE - bounce_offset;

			//bytes left = bytes left - bytes we just wrote
			bytes_left = bytes_left - bytes_to_write;	

			if(bytes_left >= BLOCK_SIZE)
				bytes_to_write = BLOCK_SIZE;
			else
				bytes_to_write = bytes_left;
		}
		memset(bounce_buf, 0, BLOCK_SIZE);
		cur_block = fat.file_array[cur_block];
	} // end while

	// int i;
	// printf("fat array for this stucture is ->");
	// for(i = rootdir.table[rd_id].first_block_id; i != FAT_EOC; i = fat.file_array[i])
	// {
	// 	printf("%d  ", i);
	// }
	// printf("\n");
	rootdir.table[rd_id].file_size = rootdir.table[rd_id].file_size + count - bytes_left;
	return count - bytes_left;
}

int fs_read(int fd, void *buf, size_t count)
{
	// printf("Executing fs_read()\n");
	if(is_disk_mounted() != SUCCESS)
	{
			return -1;
	}

	if(count == 0)
	{
		return 0;
	}

	int rd_id = rd_find(fd);
	if(rd_id < 0)
	{
		return -1;
	}

	FD_Node outf = open_files.list[fd];	

	size_t offset = outf.file_offset;

	uint8_t bounce_buf[BLOCK_SIZE];
	int bytes_left = count;

	int cur_block = find_block_from_offset(rootdir.table[rd_id].first_block_id, offset);
	int buf_offset = 0;
	int initial_offset = (offset % BLOCK_SIZE);
	int bytes_to_read;

	if(count >= BLOCK_SIZE)
		bytes_to_read = BLOCK_SIZE;
	else
		bytes_to_read = count;

	while(bytes_left > 0)
	{
		if(cur_block == FAT_EOC)
		{
			break;
		}

		if(initial_offset > 0 )	
		{
			initial_offset = 0;
			block_read(cur_block + sb.data_start, bounce_buf);
			memcpy(buf + buf_offset, bounce_buf + initial_offset, BLOCK_SIZE - initial_offset);

			bytes_left = bytes_left - BLOCK_SIZE - initial_offset;
			buf_offset = BLOCK_SIZE - initial_offset;

			if(bytes_left >= BLOCK_SIZE)
				bytes_to_read = BLOCK_SIZE;
			else
				bytes_to_read = bytes_left;
		}

		else 
		{

			block_read(cur_block + sb.data_start, bounce_buf);
			memcpy(buf + buf_offset, bounce_buf, bytes_to_read);

			buf_offset = buf_offset + bytes_to_read;

			//bytes left = bytes left - bytes we just wrote
			bytes_left = bytes_left - bytes_to_read;	

			if(bytes_left >= BLOCK_SIZE)
				bytes_to_read = BLOCK_SIZE;
			else
				bytes_to_read = bytes_left;
		}

		memset(bounce_buf, 0, BLOCK_SIZE);
		cur_block = fat.file_array[cur_block];

	}

	// printf("buf[0] = %c\n", ((char*)buf)[0]);

	open_files.list[fd].file_offset = offset + count - bytes_left;
	if(open_files.list[fd].file_offset > rootdir.table[rd_id].file_size)
	{
		perror("In fs_read(): file offset > file size");
		outf.file_offset = rootdir.table[rd_id].file_size;
	}
	return count - bytes_left;
}
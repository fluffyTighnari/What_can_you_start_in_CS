//help.h
#ifndef HELP_H
#define HELP_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <stddef.h>
#include <stdbool.h>

#define block_size 512U
#define size_of_disk (1024U * 1024U)
#define total_block 2048
#define max_files 64
#define name_length 28U
#define inodes_per_block 8
#define directors_per_block 16
#define magic_num 0xDEADBEEFU

//??????
typedef struct{
uint32_t magic;
uint32_t total_blocks;
uint32_t inodemap_blocks;
uint32_t bitmap_blocks;
uint32_t inodetable_blocks;
uint32_t free_blocks;
uint32_t data_blocks;
uint32_t root_inode;//????0
uint32_t free_inodes;
char supplements[476];//512-9*4 = 512
}Super_block;

//???????
typedef enum{
    inode_free = 0,
    inode_file = 1,
    inode_director = 2
}Inode_type;

//????????
typedef struct{
    Inode_type type;
    uint32_t size;
    uint32_t block;
    uint32_t create_time;
    uint32_t modify_time;
    uint32_t pointer[6];
    uint32_t in_pointer;
    uint32_t double_pointer;
    char supplements[12];
}Inode;

//????
typedef struct{
    uint32_t inode_num;
    char filename[name_length];
}Director_entry;

//???????????
typedef struct{
    FILE* disk_fp;
    Super_block super_block;//1
    uint8_t* inodemap;
    uint8_t* bitmap;
    Inode* inodetable;
    uint32_t begin_block_of_inodemap;//2
    uint32_t begin_block_of_bitmap;//3
    uint32_t begin_block_of_inodetable;//4,??????????8????64*64 = 8 * 512
    uint32_t begin_block_of_data;//12
}FScontext;

extern FScontext *fscontext;

void map_set(uint8_t* map,uint32_t bit);

void map_clear(uint8_t* map,uint32_t bit);

int map_check(uint8_t* map,uint32_t bit);

void fs_init_inode(uint32_t inode_num,Inode_type type);

uint32_t fs_alloc_inode();

/* ?????????????? inode?????? fs_mkdir */
uint32_t fs_alloc_inode_for_dir();

void fs_free_inode(uint32_t inode_num);

int fs_read_block(uint32_t block_num,void* buffer);

int fs_write_block(uint32_t block_num,void* buffer);

uint32_t fs_alloc_data_block();

void fs_free_data_block(uint32_t data_num);

uint32_t fs_get_file_block(Inode* inode,uint32_t num);

uint32_t fs_set_file_block(Inode* inode,uint32_t num,uint32_t block_index);
#endif

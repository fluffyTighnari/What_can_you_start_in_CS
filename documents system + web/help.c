//help.c
#ifndef HELP_C
#define HELP_C
#include "help.h"
#endif

void map_set(uint8_t* map,uint32_t bit){
    if(!map){
        return;
    }
    map[bit / 8] |= (1 << (bit % 8));
}

void map_clear(uint8_t* map,uint32_t bit){
    if(!map){
        return;
    }
    map[bit / 8] &= ~(1 << (bit % 8));
}

int map_check(uint8_t* map,uint32_t bit){
    if(!map){
        return -1;
    }
    return (map[bit / 8] >> (bit % 8)) & 1;
}


void fs_init_inode(uint32_t inode_num,Inode_type type){
    Inode* inode = &fscontext->inodetable[inode_num];
    memset(inode,0,sizeof(Inode));
    inode->type = type;
    inode->size = 0;
    inode->block = 0;
    inode->create_time = (uint32_t)time(NULL);
    inode->modify_time = (uint32_t)time(NULL);
    for(int i = 0;i < 6;i++){
        inode->pointer[i] = 0;
    }
    inode->in_pointer = 0;
    inode->double_pointer = 0;
}

uint32_t fs_alloc_inode(){
    if(fscontext->super_block.free_inodes == 0){
        perror("无空闲inode");
        return (uint32_t)-1;
    }
    uint32_t inode_num = 0;
    for(int i = 1;i < max_files;i++){
        if(map_check(fscontext->inodemap,i) == 0){
            inode_num = i;
            map_set(fscontext->inodemap,i);
            break;
        }
    }
    fs_init_inode(inode_num,inode_file);
    fscontext->super_block.free_inodes--;
    fs_write_block(fscontext->begin_block_of_inodemap,fscontext->inodemap);

    uint32_t target_block = inode_num / 8;
    fs_write_block(target_block + fscontext->begin_block_of_inodetable,fscontext->inodetable + target_block * 8);

    return inode_num;
}

uint32_t fs_alloc_inode_for_dir(){
    if(fscontext->super_block.free_inodes == 0){
        perror("无空闲inode");
        return (uint32_t)-1;
    }
    uint32_t inode_num = 0;
    for(int i = 1;i < max_files;i++){
        if(map_check(fscontext->inodemap,i) == 0){
            inode_num = i;
            map_set(fscontext->inodemap,i);
            break;
        }
    }
    fs_init_inode(inode_num,inode_director);
    fscontext->super_block.free_inodes--;
    fs_write_block(fscontext->begin_block_of_inodemap,fscontext->inodemap);
    uint32_t target_block = inode_num / 8;
    fs_write_block(target_block + fscontext->begin_block_of_inodetable,fscontext->inodetable + target_block * 8);
    return inode_num;
}

void fs_free_inode(uint32_t inode_num){
    if(inode_num >= max_files){
        return;
    }

    Inode* inode = &fscontext->inodetable[inode_num];
    for(int i = 0;i < 6;i++){
        if(inode->pointer[i] != 0){
            fs_free_data_block(inode->pointer[i]);
            inode->pointer[i] = 0;
        }
    }

    if(inode->in_pointer != 0){
        uint32_t blocks_free[block_size / sizeof(uint32_t)] = {0};
        if(fs_read_block(inode->in_pointer,blocks_free) == 0){
            for(uint32_t i = 0;i < block_size / sizeof(uint32_t);i++){
                if(blocks_free[i] != 0){
                    fs_free_data_block(blocks_free[i]);
                }
            }
        }
        fs_free_data_block(inode->in_pointer);
        inode->in_pointer = 0;
    }

    if(inode->double_pointer != 0){
        uint32_t level1[block_size / sizeof(uint32_t)] = {0};
        if(fs_read_block(inode->double_pointer, level1) == 0){
            for(uint32_t i = 0;i < block_size / sizeof(uint32_t);i++){
                if(level1[i] != 0){
                    uint32_t level2[block_size / sizeof(uint32_t)] = {0};
                    if(fs_read_block(level1[i], level2) == 0){
                        for(uint32_t j = 0;j < block_size / sizeof(uint32_t);j++){
                            if(level2[j] != 0){
                                fs_free_data_block(level2[j]);
                            }
                        }
                    }
                    fs_free_data_block(level1[i]);
                }
            }
        }
        fs_free_data_block(inode->double_pointer);
        inode->double_pointer = 0;
    }

    map_clear(fscontext->inodemap,inode_num);
    memset(inode,0,sizeof(Inode));
    fscontext->super_block.free_inodes++;

    fs_write_block(fscontext->begin_block_of_inodemap,fscontext->inodemap);
    uint32_t target_block = inode_num / 8;
    fs_write_block(fscontext->begin_block_of_inodetable + target_block,fscontext->inodetable + target_block * 8);
}


int fs_read_block(uint32_t block_num,void* buffer){
    if(!fscontext->disk_fp || block_num >= total_block){
        return -1;
    }

    if(fseek(fscontext->disk_fp,block_num * block_size,SEEK_SET) != 0){
        return -1;
    }

    return fread(buffer,block_size,1,fscontext->disk_fp) == 1 ? 0 : -1;
}

int fs_write_block(uint32_t block_num,void* buffer){
    if(!fscontext->disk_fp || block_num >= total_block){
        return -1;
    }

    if(fseek(fscontext->disk_fp,block_num * block_size,SEEK_SET) != 0){
        return -1;
    }

    return fwrite(buffer,block_size,1,fscontext->disk_fp) == 1 ? 0 : -1;
}

uint32_t fs_alloc_data_block(){
    if(fscontext->super_block.free_blocks == 0){
        perror("无空闲块");
        return (uint32_t)-1;
    }
    uint32_t data_num = 0;
    int found = 0;
    for(uint32_t i = 0; i < fscontext->super_block.data_blocks; i++){
        if(map_check(fscontext->bitmap,i) == 0){
            data_num = i;
            map_set(fscontext->bitmap,i);
            found = 1;
            break;
        }
    }
    if (!found) {
        return (uint32_t)-1;
    }
    char new_block[block_size] = {0};
    fscontext->super_block.free_blocks--;
    fs_write_block(fscontext->begin_block_of_data + data_num,new_block);

    fs_write_block(fscontext->begin_block_of_bitmap,fscontext->bitmap);

    return data_num + fscontext->begin_block_of_data;
}

void fs_free_data_block(uint32_t data_num){
    if(data_num < fscontext->begin_block_of_data || data_num >= fscontext->begin_block_of_data + fscontext->super_block.data_blocks){
        return;
    }
    uint32_t data_index = data_num - fscontext->begin_block_of_data;

    if(map_check(fscontext->bitmap,data_index) == 1){
        map_clear(fscontext->bitmap,data_index);
        fscontext->super_block.free_blocks++;
        fs_write_block(fscontext->begin_block_of_bitmap,fscontext->bitmap);
    }
}

uint32_t fs_get_file_block(Inode* inode,uint32_t num){
    if(num < 1 || num > 6 + (block_size/sizeof(uint32_t))){
        return 0;
    }
    if(num <= 6){
        return inode->pointer[num-1];
    }
    uint32_t maxnum = 6 + block_size / sizeof(uint32_t);
    if(num < maxnum){
        if(inode->in_pointer == 0){
            return 0;
        }
        uint32_t in_blocks[block_size / sizeof(uint32_t)];
        if(fs_read_block(inode->in_pointer,in_blocks) != 0){
            return 0;
        }
        return in_blocks[num-7];
    }
    return 0;
}

uint32_t fs_set_file_block(Inode* inode,uint32_t num,uint32_t block_index){
    if(num < 1 || num > 6 + (block_size/sizeof(uint32_t))){
        return -1;
    }
    if(num <= 6){
        inode->pointer[num-1] = block_index;
        return 0;
    }
    uint32_t maxnum = 6 + block_size / sizeof(uint32_t);
    if(num < maxnum){
        if(inode->in_pointer == 0){
            inode->in_pointer = fs_alloc_data_block();
            if(inode->in_pointer == (uint32_t)-1){
                return -1;
            }
            uint32_t zero_block[block_size / sizeof(uint32_t)] = {0};
            fs_write_block(inode->in_pointer,zero_block);
        }
        uint32_t buffer[block_size / sizeof(uint32_t)] = {0};
        if(fs_read_block(inode->in_pointer,buffer) != 0){
            return -1;
        }
        buffer[num-7] = block_index;
        fs_write_block(inode->in_pointer,buffer);
        return 0;
    }
    return -1;
}
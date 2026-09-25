//interface.c
#ifndef INTERFACE_C
#define INTERFACE_C
#include "help.h"
#include "interface.h"
#include <pthread.h>
#endif

FScontext *fscontext = NULL;
pthread_mutex_t fs_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ?? parent ??????????? name ???????????? inode ???0 ???????? */
static void write_back_inode(Inode* inode) {
    uint32_t inode_idx = (uint32_t)(inode - fscontext->inodetable);
    uint32_t target_block = inode_idx / inodes_per_block;
    fs_write_block(fscontext->begin_block_of_inodetable + target_block,
                   fscontext->inodetable + target_block * inodes_per_block);
}

static uint32_t get_inode_num_in_dir(Inode* parent, const char* name) {
    Director_entry directors[directors_per_block];
    uint32_t block_num = parent->block;
    for (uint32_t i = 0; i < block_num; i++) {
        uint32_t block_addr = fs_get_file_block(parent, i + 1);
        if (block_addr == 0) break;
        if (fs_read_block(block_addr, directors) != 0) continue;
        for (uint32_t j = 0; j < directors_per_block; j++) {
            if (directors[j].inode_num != 0 && strcmp(directors[j].filename, name) == 0)
                return directors[j].inode_num;
        }
    }
    return 0;
}

/* ?????????????? dirname ???????????? Inode ???NULL ?????????????? */
static Inode* get_dir_inode_from_root(const char* dirname) {
    if (!dirname || strlen(dirname) == 0 || fscontext == NULL) return NULL;
    Inode* root_inode = &fscontext->inodetable[0];
    if (root_inode->type != inode_director) return NULL;
    uint32_t inode_num = get_inode_num_in_dir(root_inode, dirname);
    if (inode_num == 0) return NULL;
    Inode* dir = &fscontext->inodetable[inode_num];
    return (dir->type == inode_director) ? dir : NULL;
}

/* ?? parent ??????????? entry??parent ??????????? Inode* */
static int add_entry_to_dir(Inode* parent, const Director_entry* entry) {
    Director_entry directors[directors_per_block];
    uint32_t block_num = parent->block;
    for (uint32_t i = 0; i < block_num; i++) {
        uint32_t block_addr = fs_get_file_block(parent, i + 1);
        if (block_addr == 0) break;
        if (fs_read_block(block_addr, directors) != 0) continue;
        for (uint32_t j = 0; j < directors_per_block; j++) {
            if (directors[j].inode_num == 0) {
                directors[j] = *entry;
                if (fs_write_block(block_addr, directors) == 0) {
                    parent->size += sizeof(Director_entry);
                    parent->modify_time = (uint32_t)time(NULL);
                    write_back_inode(parent);
                    return 0;
                }
                return -1;
            }
        }
    }
    uint32_t new_block = fs_alloc_data_block();
    if (new_block == (uint32_t)-1) return -1;
    memset(directors, 0, sizeof(directors));
    directors[0] = *entry;
    if (fs_write_block(new_block, directors) != 0) {
        fs_free_data_block(new_block);
        return -1;
    }
    if (fs_set_file_block(parent, parent->block + 1, new_block) != 0) {
        fs_free_data_block(new_block);
        return -1;
    }
    parent->block++;
    parent->size += sizeof(Director_entry);
    parent->modify_time = (uint32_t)time(NULL);
    write_back_inode(parent);
    return 0;
}

void fs_create(const char* disk_path){
    FILE* test = fopen(disk_path,"rb");
    if(test != NULL){
        fclose(test);
        perror("磁盘文件已存在\n");
        return;
    }

    // ???????fscontext????
    if(fscontext != NULL){
        fs_unmount();
    }
    fscontext = (FScontext*)malloc(sizeof(FScontext));
    if(fscontext == NULL){
        perror("?????????\n");
        return;
    }
    memset(fscontext, 0, sizeof(FScontext));

    FILE* fp = fopen(disk_path,"wb+");
    if(fp == NULL){
        perror("??????????????\n");
        free(fscontext);
        fscontext = NULL;
        return;
    }
    fscontext->disk_fp = fp;
    fscontext->begin_block_of_inodemap = 1;
    fscontext->begin_block_of_bitmap = 2;
    fscontext->begin_block_of_inodetable = 3;
    fscontext->begin_block_of_data = fscontext->begin_block_of_inodetable + max_files / inodes_per_block;

    Super_block* super_block = &fscontext->super_block;
    super_block->magic = 0xDEADBEEFU;
    super_block->total_blocks = total_block;
    super_block->inodemap_blocks = 1;
    super_block->bitmap_blocks = 1;
    super_block->inodetable_blocks = fscontext->begin_block_of_data - fscontext->begin_block_of_inodetable;
    super_block->free_blocks = super_block->total_blocks - fscontext->begin_block_of_data;
    super_block->data_blocks = super_block->free_blocks;
    super_block->root_inode = 0;
    super_block->free_inodes = max_files - 1;

    fscontext->inodemap = (uint8_t*)calloc(block_size,1);
    fscontext->bitmap = (uint8_t*)calloc(block_size,1);
    fscontext->inodetable = (Inode*)calloc(block_size * super_block->inodetable_blocks,1);
    if(!fscontext->inodemap || !fscontext->bitmap || !fscontext->inodetable){
        perror("?????????\n");
        fclose(fp);
        free(fscontext->inodemap);
        free(fscontext->bitmap);
        free(fscontext->inodetable);
        free(fscontext);
        fscontext = NULL;
        return;
    }

    map_set(fscontext->inodemap,0);
    Inode* inode = &fscontext->inodetable[0];
    memset(inode,0,sizeof(Inode));
    inode->type = inode_director;
    inode->create_time = (uint32_t)time(NULL);
    inode->modify_time = inode->create_time;

    if(fs_write_block(0,super_block) != 0){
        perror("??????????????\n");
    }

    if(fs_write_block(1,fscontext->inodemap) != 0){
        perror("inode???????????\n");
    }

    if(fs_write_block(2,fscontext->bitmap) != 0){
        perror("?????????????\n");
    }

    for(uint32_t i = 0;i < super_block->inodetable_blocks;i++){
        if(fs_write_block(fscontext->begin_block_of_inodetable+i,&fscontext->inodetable[i*inodes_per_block]) != 0){
            perror("inode??????????\n");
        }
    }

    uint8_t* zero_block = (uint8_t*)calloc(block_size,1);
    if(!zero_block){
        perror("?????????\n");
        fclose(fp);
        free(fscontext->inodemap);
        free(fscontext->bitmap);
        free(fscontext->inodetable);
        free(fscontext);
        fscontext = NULL;
        return;
    }
    for(uint32_t i = 0;i < super_block->data_blocks;i++){
        if(fs_write_block(fscontext->begin_block_of_data+i,zero_block) != 0){
            perror("?????????\n");
        }
    }

    fclose(fscontext->disk_fp);
    fscontext->disk_fp = NULL;
    free(fscontext->inodemap);
    free(fscontext->bitmap);
    free(fscontext->inodetable);
    free(fscontext);
    fscontext = NULL;
    free(zero_block);

    printf("??????????????????\n");
}


int fs_mount(const char* disk_path){
    // ????
    pthread_mutex_lock(&fs_mutex);
    
    // ?????????????????
    if(fscontext != NULL){
        fs_unmount();
    }
    
    // ???????fscontext????
    fscontext = (FScontext*)malloc(sizeof(FScontext));
    if(fscontext == NULL){
        perror("?????????\n");
        pthread_mutex_unlock(&fs_mutex);
        return -1;
    }
    
    // ?????fscontext
    memset(fscontext, 0, sizeof(FScontext));
    
    fscontext->disk_fp = fopen(disk_path,"rb+");
    if(fscontext->disk_fp == NULL){
        perror("??????????\n");
        free(fscontext);
        fscontext = NULL;
        return -1;
    }
    Super_block super_block;
    if(fs_read_block(0,&super_block) != 0){
        perror("????????????\n");
        fclose(fscontext->disk_fp);
        free(fscontext);
        fscontext = NULL;
        return -1;
    }
    if(super_block.magic != magic_num){
        perror("????????\n");
        fclose(fscontext->disk_fp);
        free(fscontext);
        fscontext = NULL;
        return -1;
    }
    fscontext->super_block = super_block;

    fscontext->begin_block_of_inodemap = 1;
    fscontext->begin_block_of_bitmap = 2;
    fscontext->begin_block_of_inodetable = 3;
    fscontext->begin_block_of_data = 3 + super_block.inodetable_blocks;

    fscontext->inodemap = (uint8_t*)calloc(block_size,1);
    fscontext->bitmap = (uint8_t*)calloc(block_size,1);
    fscontext->inodetable = (Inode*)calloc(block_size * super_block.inodetable_blocks,1);
    if(!fscontext->inodemap || !fscontext->bitmap || !fscontext->inodetable){
        perror("?????????\n");
        fclose(fscontext->disk_fp);
        free(fscontext->inodemap);
        free(fscontext->bitmap);
        free(fscontext->inodetable);
        free(fscontext);
        fscontext = NULL;
        return -1;
    }

    if(fs_read_block(1,fscontext->inodemap) != 0){
        perror("????inode???????\n");
        goto mount_fail;
    }

    if(fs_read_block(2,fscontext->bitmap) != 0){
        perror("????????????????\n");
        goto mount_fail;
    }


    for(uint32_t i = 0;i < fscontext->super_block.inodetable_blocks;i++){
        if(fs_read_block(fscontext->begin_block_of_inodetable+i,&fscontext->inodetable[i*inodes_per_block]) != 0){
            perror("????inode??????\n");
            goto mount_fail;
        }
    }

    pthread_mutex_unlock(&fs_mutex);
    return 0;

    mount_fail:
    free(fscontext->inodemap);
    free(fscontext->bitmap);
    free(fscontext->inodetable);
    fclose(fscontext->disk_fp);
    free(fscontext);
    fscontext = NULL;
    pthread_mutex_unlock(&fs_mutex);
    return -1;
}

void fs_unmount(){    
    pthread_mutex_lock(&fs_mutex);    
    if(fscontext == NULL || fscontext->disk_fp == NULL){
        perror("???????????\n");
        pthread_mutex_unlock(&fs_mutex);
        return;
    }

    fs_write_block(0,&fscontext->super_block);
    fs_write_block(fscontext->begin_block_of_inodemap,fscontext->inodemap);
    fs_write_block(fscontext->begin_block_of_bitmap,fscontext->bitmap);
    for(uint32_t i = 0;i < fscontext->super_block.inodetable_blocks;i++){
        fs_write_block(fscontext->begin_block_of_inodetable+i,
                      &fscontext->inodetable[i*inodes_per_block]);
    }

    free(fscontext->inodemap);
    free(fscontext->bitmap);
    free(fscontext->inodetable);
    fclose(fscontext->disk_fp);
    free(fscontext);
    fscontext = NULL;
    pthread_mutex_unlock(&fs_mutex);
}

int fs_mkdir(const char* dirname, const char* current_dir){
    // ????
    pthread_mutex_lock(&fs_mutex);
    
    if(!dirname || strlen(dirname) > name_length || fscontext == NULL){
        perror("?????????\n");
        pthread_mutex_unlock(&fs_mutex);
        return -1;
    }
    Inode* parent = &fscontext->inodetable[0];
    if(current_dir && strlen(current_dir) > 0){
        parent = get_dir_inode_from_root(current_dir);
        if(!parent){
            perror("???????????\n");
            pthread_mutex_unlock(&fs_mutex);
            return -1;
        }
    }
    if(parent->type != inode_director){
        perror("?????????????\n");
        pthread_mutex_unlock(&fs_mutex);
        return -1;
    }
    if(get_inode_num_in_dir(parent, dirname) != 0){
        perror("??????????\n");
        pthread_mutex_unlock(&fs_mutex);
        return -1;
    }

    uint32_t new_inode = fs_alloc_inode_for_dir();
    if(new_inode == (uint32_t)-1){
        perror("inode????????\n");
        pthread_mutex_unlock(&fs_mutex);
        return -1;
    }
    Director_entry new_entry = {0};
    new_entry.inode_num = new_inode;
    strncpy(new_entry.filename, dirname, name_length - 1);
    new_entry.filename[name_length - 1] = '\0';

    if(add_entry_to_dir(parent, &new_entry) != 0){
        fs_free_inode(new_inode);
        pthread_mutex_unlock(&fs_mutex);
        return -1;
    }
    printf("????%s ?????????inode ? %u\n", dirname, new_inode);
    pthread_mutex_unlock(&fs_mutex);
    return 0;
}

int fs_create_file(const char* filename, const char* dirname){
    // ????
    pthread_mutex_lock(&fs_mutex);
    
    if(!filename || strlen(filename) > name_length || fscontext == NULL){
        perror("??????????\n");
        pthread_mutex_unlock(&fs_mutex);
        return -1;
    }
    Inode* dir_inode = get_dir_inode_from_root(dirname);
    if(!dir_inode){
        perror("????????\n");
        pthread_mutex_unlock(&fs_mutex);
        return -1;
    }

    Director_entry directors[directors_per_block];
    uint32_t block_num = dir_inode->block;
    for(uint32_t i = 0;i < block_num;i++){
        uint32_t block_addr = fs_get_file_block(dir_inode,i+1);
        if(block_addr == 0) break;
        if(fs_read_block(block_addr,directors) != 0) continue;
        for(uint32_t j = 0;j < directors_per_block;j++){
            if(directors[j].inode_num != 0 && strcmp(directors[j].filename,filename) == 0){
                perror("???????????\n");
                pthread_mutex_unlock(&fs_mutex);
                return -1;
            }
        }
    }

    Director_entry new_entry = {0};
    uint32_t new_inode = fs_alloc_inode();
    if(new_inode == (uint32_t)-1){
        perror("inode????????\n");
        pthread_mutex_unlock(&fs_mutex);
        return -1;
    }
    new_entry.inode_num = new_inode;
    strncpy(new_entry.filename,filename,name_length-1);
    new_entry.filename[name_length-1] = '\0';

    if(add_entry_to_dir(dir_inode, &new_entry) != 0){
        fs_free_inode(new_inode);
        pthread_mutex_unlock(&fs_mutex);
        return -1;
    }
    printf("?????%s ???? %s ??????????inode ? %u\n", filename, dirname, new_inode);
    pthread_mutex_unlock(&fs_mutex);
    return 0;
}

int fs_delete_file(const char* filename, const char* dirname){
    // ????
    pthread_mutex_lock(&fs_mutex);
    
    if(!filename || strlen(filename) > name_length || fscontext == NULL){
        perror("??????????\n");
        pthread_mutex_unlock(&fs_mutex);
        return -1;
    }
    Inode* dir_inode = get_dir_inode_from_root(dirname);
    if(!dir_inode){
        perror("????????\n");
        pthread_mutex_unlock(&fs_mutex);
        return -1;
    }

    Director_entry directors[directors_per_block];
    uint32_t block_num = dir_inode->block;
    for(uint32_t i = 0;i < block_num;i++){
        uint32_t block_addr = fs_get_file_block(dir_inode,i+1);
        if(block_addr == 0) break;
        if(fs_read_block(block_addr,directors) != 0) continue;
        for(uint32_t j = 0;j < directors_per_block;j++){
            if(directors[j].inode_num != 0 && strcmp(directors[j].filename,filename) == 0){
                fs_free_inode(directors[j].inode_num);
                memset(&directors[j],0,sizeof(Director_entry));
                dir_inode->size -= sizeof(Director_entry);
                dir_inode->modify_time = (uint32_t)time(NULL);
                fs_write_block(block_addr,directors);
                write_back_inode(dir_inode);
                printf("文件 %s 删除成功\n",filename);
                pthread_mutex_unlock(&fs_mutex);
                return 0;
            }
        }
    }
    perror("??????????\n");
    pthread_mutex_unlock(&fs_mutex);
    return -1;
}

int fs_read_file(const char* filename, const char* dirname, void* buffer, uint32_t size, uint32_t offset){
    // ????
    pthread_mutex_lock(&fs_mutex);
    
    if(!filename || strlen(filename) > name_length || size == 0 || !buffer || fscontext == NULL){
        pthread_mutex_unlock(&fs_mutex);
        return -1;
    }
    Inode* dir_inode = get_dir_inode_from_root(dirname);
    if(!dir_inode){
        perror("????????\n");
        pthread_mutex_unlock(&fs_mutex);
        return -1;
    }

    uint32_t target_inode_num = get_inode_num_in_dir(dir_inode, filename);
    if(target_inode_num == 0){
        perror("?????????!\n");
        pthread_mutex_unlock(&fs_mutex);
        return -1;
    }
    Inode* target_inode = &fscontext->inodetable[target_inode_num];
    if(target_inode->type != inode_file){
        perror("??????????\n");
        pthread_mutex_unlock(&fs_mutex);
        return -1;
    }
    if(target_inode->size < offset){
        pthread_mutex_unlock(&fs_mutex);
        return -1;
    }
    int realsize = target_inode->size - offset > (int)size ? (int)size : target_inode->size - offset;

    int beginblock = offset / block_size;
    int endblock = (offset + realsize - 1) / block_size;
    int curoffset = offset % block_size;
    int totalread = 0;

    for(int i = beginblock;i <= endblock;i++){
        uint8_t buf[block_size];
        memset(buf, 0, sizeof(buf));
        uint32_t block_num = fs_get_file_block(target_inode,i+1);
        if(block_num != 0){
            if(fs_read_block(block_num,buf) != 0){
                perror("????????\n");
                pthread_mutex_unlock(&fs_mutex);
                return -1;
            }
        }

        int cursize = i == endblock ? realsize - totalread : block_size - curoffset;
        if (totalread + cursize > (int)size) {
            pthread_mutex_unlock(&fs_mutex);
            return -1;
        }
        memcpy((uint8_t*)buffer + totalread,buf + curoffset,cursize);
        totalread += cursize;
        curoffset = 0;
    }
    pthread_mutex_unlock(&fs_mutex);
    return totalread;
}

int fs_write_file(const char* filename, const char* dirname, const void* buffer, uint32_t size, uint32_t offset){
    // ????
    pthread_mutex_lock(&fs_mutex);
    
    if(!filename || strlen(filename) > name_length || size == 0 || !buffer || fscontext == NULL){
        pthread_mutex_unlock(&fs_mutex);
        return -1;
    }
    Inode* dir_inode = get_dir_inode_from_root(dirname);
    if(!dir_inode){
        perror("????????\n");
        pthread_mutex_unlock(&fs_mutex);
        return -1;
    }

    uint32_t target_inode_num = get_inode_num_in_dir(dir_inode, filename);
    if(target_inode_num == 0){
        perror("?????????!\n");
        pthread_mutex_unlock(&fs_mutex);
        return -1;
    }
    Inode* target_inode = &fscontext->inodetable[target_inode_num];
    if(target_inode->type != inode_file){
        perror("??????????\n");
        pthread_mutex_unlock(&fs_mutex);
        return -1;
    }
    if(offset + size > 6*block_size + (block_size/sizeof(uint32_t))*block_size){
        perror("????????????\n");
        pthread_mutex_unlock(&fs_mutex);
        return -1;
    }
    int needblock = (offset + size + block_size - 1) / block_size;
    int curblock = target_inode->block;
    int orig_block = target_inode->block;
    int newblock = needblock - curblock;
    int ret = -1;

    if(newblock > 0) {
        int alloc_try = newblock;
        while(alloc_try > 0){
            uint32_t block_num = fs_alloc_data_block();
            if(block_num == (uint32_t)-1){
                while (target_inode->block > (uint32_t)orig_block) {
                    uint32_t bn = fs_get_file_block(target_inode, target_inode->block);
                    if (bn != 0) fs_free_data_block(bn);
                    target_inode->block--;
                }
                pthread_mutex_unlock(&fs_mutex);
                return -1;
            }
            fs_set_file_block(target_inode, ++curblock, block_num);
            target_inode->block++;
            alloc_try--;
        }
    } else if(newblock < 0) {
        int blocks_to_free = -newblock;
        while(blocks_to_free--){
            uint32_t block_num = fs_get_file_block(target_inode, curblock);
            if(block_num != 0){
                fs_free_data_block(block_num);
            }
            curblock--;
            target_inode->block--;
        }
    }

    int beginblock = offset / block_size;
    int endblock = (offset + size - 1) / block_size;
    int curoffset = offset % block_size;
    int totalwrite = 0;
    uint8_t buf[block_size];
    int write_ok = 1;
    for(int i = beginblock;i <= endblock;i++){
        uint32_t block_addr = fs_get_file_block(target_inode,i+1);
        if(block_addr == 0){
            write_ok = 0;
            break;
        }
        if(fs_read_block(block_addr,buf) != 0){
            perror("读取块失败\n");
            write_ok = 0;
            break;
        }
        int cursize = i == endblock ? (int)size - totalwrite : block_size - curoffset;
        memcpy(buf+curoffset,(const uint8_t*)buffer+totalwrite,cursize);
        totalwrite += cursize;
        curoffset = 0;

        fs_write_block(block_addr,buf);
    }
    if (write_ok) {
        target_inode->size = offset + size;
        target_inode->modify_time = (uint32_t)time(NULL);
        ret = totalwrite;
    }
    write_back_inode(target_inode);

    pthread_mutex_unlock(&fs_mutex);
    return ret;
}

void fs_cat_file(const char* filename, const char* dirname){
    pthread_mutex_lock(&fs_mutex);
    if(!filename || fscontext == NULL) {
        pthread_mutex_unlock(&fs_mutex);
        return;
    }
    uint8_t buf[1024];
    int read;
    uint32_t offset = 0;
    pthread_mutex_unlock(&fs_mutex);
    while((read = fs_read_file(filename, dirname, buf, sizeof(buf), offset)) > 0){
        fwrite(buf,1,read,stdout);
        offset += read;
    }
    printf("\n");
}

void fs_show_information(){
    pthread_mutex_lock(&fs_mutex);
    if(fscontext == NULL) {
        pthread_mutex_unlock(&fs_mutex);
        return;
    }
    Super_block* super_block = &fscontext->super_block;

    double usage = (1.0 - (double)super_block->free_blocks / super_block->data_blocks) * 100;
    printf("\n=== 文件系统信息 ===\n");
    printf(" 总块数:      %u\n", super_block->total_blocks);
    printf(" 数据块数:    %u\n", super_block->data_blocks);
    printf(" 空闲块数:    %u\n", super_block->free_blocks);
    printf(" 空闲inode数: %u\n", super_block->free_inodes);
    printf(" 空间使用率:  %.2f%%\n", usage);
    pthread_mutex_unlock(&fs_mutex);
}

void fs_list_director(const char* dirname){
    pthread_mutex_lock(&fs_mutex);
    if(fscontext == NULL) {
        pthread_mutex_unlock(&fs_mutex);
        return;
    }
    Inode* dir_inode = get_dir_inode_from_root(dirname);
    if(!dir_inode){
        perror("目录不存在\n");
        pthread_mutex_unlock(&fs_mutex);
        return;
    }
    printf("\n=== [%s] 目录列表 ===\n", dirname);
    printf(" %-20s %-10s %-8s %-6s %s\n", "文件名", "大小(字节)", "块数", "inode", "类型");
    Director_entry buf[directors_per_block];
    uint32_t block_num = dir_inode->block;
    int count = 0;
    for(uint32_t i = 0;i < block_num;i++){
        uint32_t block_addr = fs_get_file_block(dir_inode,i+1);
        if(block_addr == 0) break;
        if(fs_read_block(block_addr,buf) != 0){
            perror("读取目录块失败\n");
            continue;
        }
        for(int j = 0;j < directors_per_block;j++){
            if(buf[j].inode_num != 0){
                Inode* target_inode = &fscontext->inodetable[buf[j].inode_num];
                const char* type = target_inode->type == inode_file ? "文件" : "目录";
                printf(" %-20s %-10u %-8u %-6u %s\n",buf[j].filename,target_inode->size,target_inode->block,buf[j].inode_num,type);
                count++;
            }
        }
    }
    printf("共 %d 项\n", count);
    pthread_mutex_unlock(&fs_mutex);
}

int fs_list_director_to_buf(const char* dirname, char* buf, size_t buf_size) {
    if (!buf || buf_size == 0) return -1;
    pthread_mutex_lock(&fs_mutex);
    if (fscontext == NULL || !dirname) {
        pthread_mutex_unlock(&fs_mutex);
        return -1;
    }
    Inode* dir_inode = get_dir_inode_from_root(dirname);
    if (!dir_inode) {
        pthread_mutex_unlock(&fs_mutex);
        return -1;
    }
    size_t off = 0;
    int n;
    n = snprintf(buf + off, buf_size - off, "\n=== [%s] 目录列表 ===\n", dirname);
    if (n < 0 || (size_t)n >= buf_size - off) { pthread_mutex_unlock(&fs_mutex); return -1; }
    off += (size_t)n;
    n = snprintf(buf + off, buf_size - off, " %-20s %-10s %-8s %-6s %s\n",
                 "文件名", "大小(字节)", "块数", "inode", "类型");
    if (n < 0 || (size_t)n >= buf_size - off) { pthread_mutex_unlock(&fs_mutex); return -1; }
    off += (size_t)n;

    Director_entry entry_buf[directors_per_block];
    uint32_t block_num = dir_inode->block;
    int count = 0;
    for (uint32_t i = 0; i < block_num; i++) {
        uint32_t block_addr = fs_get_file_block(dir_inode, i + 1);
        if (block_addr == 0) break;
        if (fs_read_block(block_addr, entry_buf) != 0) continue;
        for (int j = 0; j < directors_per_block; j++) {
            if (entry_buf[j].inode_num != 0) {
                Inode* target_inode = &fscontext->inodetable[entry_buf[j].inode_num];
                const char* type = target_inode->type == inode_file ? "文件" : "目录";
                n = snprintf(buf + off, buf_size - off,
                             " %-20s %-10u %-8u %-6u %s\n",
                             entry_buf[j].filename, target_inode->size,
                             target_inode->block, entry_buf[j].inode_num, type);
                if (n < 0 || (size_t)n >= buf_size - off) {
                    pthread_mutex_unlock(&fs_mutex);
                    return -1;
                }
                off += (size_t)n;
                count++;
            }
        }
    }
    n = snprintf(buf + off, buf_size - off, "共 %d 项\n", count);
    if (n < 0 || (size_t)n >= buf_size - off) { pthread_mutex_unlock(&fs_mutex); return -1; }
    off += (size_t)n;

    pthread_mutex_unlock(&fs_mutex);
    return (int)off;
}

int fs_list_member_to_buf(char* buf, size_t buf_size) {
    if (!buf || buf_size == 0) return -1;
    pthread_mutex_lock(&fs_mutex);
    if (fscontext == NULL) {
        pthread_mutex_unlock(&fs_mutex);
        return -1;
    }
    size_t off = 0;
    int n;
    n = snprintf(buf + off, buf_size - off, "\n=== 根目录成员列表（所有用户/目录） ===\n");
    if (n < 0 || (size_t)n >= buf_size - off) { pthread_mutex_unlock(&fs_mutex); return -1; }
    off += (size_t)n;
    n = snprintf(buf + off, buf_size - off, " %-20s %-10s %-8s %-6s %s\n",
                 "名称", "大小(字节)", "块数", "inode", "类型");
    if (n < 0 || (size_t)n >= buf_size - off) { pthread_mutex_unlock(&fs_mutex); return -1; }
    off += (size_t)n;

    Director_entry entries[directors_per_block];
    Inode* root_inode = &fscontext->inodetable[0];
    uint32_t block_num = root_inode->block;
    int count = 0;
    for (uint32_t i = 0; i < block_num; i++) {
        uint32_t block_addr = fs_get_file_block(root_inode, i + 1);
        if (block_addr == 0) break;
        if (fs_read_block(block_addr, entries) != 0) continue;
        for (int j = 0; j < directors_per_block; j++) {
            if (entries[j].inode_num != 0) {
                Inode* target_inode = &fscontext->inodetable[entries[j].inode_num];
                const char* type = target_inode->type == inode_file ? "文件" : "目录";
                n = snprintf(buf + off, buf_size - off,
                             " %-20s %-10u %-8u %-6u %s\n",
                             entries[j].filename, target_inode->size,
                             target_inode->block, entries[j].inode_num, type);
                if (n < 0 || (size_t)n >= buf_size - off) {
                    pthread_mutex_unlock(&fs_mutex);
                    return -1;
                }
                off += (size_t)n;
                count++;
            }
        }
    }
    n = snprintf(buf + off, buf_size - off, "共 %d 个成员\n", count);
    if (n < 0 || (size_t)n >= buf_size - off) { pthread_mutex_unlock(&fs_mutex); return -1; }
    off += (size_t)n;

    pthread_mutex_unlock(&fs_mutex);
    return (int)off;
}

void fs_list_member(){
    pthread_mutex_lock(&fs_mutex);
    if(fscontext == NULL) {
        pthread_mutex_unlock(&fs_mutex);
        return;
    }
    printf("\n=== 根目录成员列表（所有用户/目录） ===\n");
    printf(" %-20s %-10s %-8s %-6s %s\n", "名称", "大小(字节)", "块数", "inode", "类型");
    Director_entry buf[directors_per_block];
    Inode* root_inode = &fscontext->inodetable[0];
    uint32_t block_num = root_inode->block;
    int count = 0;
    for(uint32_t i = 0;i < block_num;i++){
        uint32_t block_addr = fs_get_file_block(root_inode,i+1);
        if(block_addr == 0) break;
        if(fs_read_block(block_addr,buf) != 0){
            perror("读取目录块失败\n");
            continue;
        }
        for(int j = 0;j < directors_per_block;j++){
            if(buf[j].inode_num != 0){
                Inode* target_inode = &fscontext->inodetable[buf[j].inode_num];
                const char* type = target_inode->type == inode_file ? "文件" : "目录";
                printf(" %-20s %-10u %-8u %-6u %s\n",buf[j].filename,target_inode->size,target_inode->block,buf[j].inode_num,type);
                count++;
            }
        }
    }
    printf("共 %d 个成员\n", count);
    pthread_mutex_unlock(&fs_mutex);
}

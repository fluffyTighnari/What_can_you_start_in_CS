//Memory Allocator

#include<stddef.h>
#include<stdint.h>
#include<unistd.h>
#include<string.h>

typedef struct block_header{
    size_t size;
    int free;
    struct block_header* pre;
    struct block_header* next;
}block_header;

#define ALIGNMENT 16
#define ALIGN(size) (((size)+ALIGNMENT-1) & ~(ALIGNMENT - 1))
#define header_size ALIGN(sizeof(block_header))

block_header* freelist = NULL;

void* change_to_user(block_header* header){
return (void*)((char*)header + header_size);
}

block_header* change_to_allocator(void* block){
    return (block_header*)((char*)block - header_size);
}

void insert_block(block_header* header){
    header->next = freelist;
    header->pre = NULL;
    header->free = 1;
    if(header->next != NULL){
        header->next->pre = header;
        merge_block(header);
    }
    freelist = header;
}

void remove_block(block_header* header){
    if(header->pre != NULL){
        header->pre->next = header->next;
    }
    if(header->next != NULL){
        header->next->pre = header->pre;
    }
    header->free = 0;
    header->pre = header->next = NULL;
}

block_header* find_next_block(block_header* header){
    char* next = (char*)header + header->size + header_size;
    if(next - (char*)sbrk(0) >= 0){
    return NULL;
    }
    return (block_header*)next;
}

void merge_block(block_header* header){
    block_header* nextheader = find_next_block(header);
    if(nextheader == NULL || nextheader->free == 0){
        return;
    }
    remove_block(nextheader);
    header->size += nextheader->size + header_size;
}

void split_block(block_header* header,size_t size){
    if(header->size < size + header_size + ALIGNMENT){
    return;
    }
    block_header* newheader = (block_header*)((char*)header + header_size + size);
    newheader->size = header->size - size - header_size;
    newheader->free = 1;
    newheader->pre = newheader->next = NULL;
    header->size = size;
    insert_block(newheader);
}

block_header* span_heap(size_t needsize){
    size_t totalsize = needsize + header_size;
    block_header* newblock = sbrk(totalsize);
    if(newblock == (void*)-1){
        return NULL;
    }
    newblock->pre = NULL;
    newblock->next = NULL;
    newblock->free = 0;
    newblock->size = needsize;
    return newblock;
}



void* my_malloc(size_t size){
    if(size == 0){
        return NULL;
    }
    size_t needsize = ALIGN(size);
    block_header* cur = freelist;
    while(cur != NULL){
        if(cur->size >= needsize){
            remove_block(cur);
            split_block(cur,size);
            return change_to_user(cur);
        }
    cur = cur->next;
    }
    block_header* block = span_heap(needsize);
    if(block == NULL){
        return NULL;
    }
    return change_to_user(block);
}

void my_free(void* block){
    if(block == NULL){
        return;
    }
    block_header* header = change_to_allocator(block);
    insert_block(header);
    merge_block(header);
}

void* my_realloc(void* block,size_t size){
    if(block == NULL){
        return NULL;
    }
    if(size == 0){
        return NULL;
    }
    block_header* header = change_to_allocator(block);
    size_t needsize = ALIGN(size);
    if(needsize <= header->size){
        split_block(header,needsize);
        return change_to_user(header);
    }
    else{
        merge_block(header);
        if(header->size >= needsize){
            split_block(header,needsize);
            return change_to_user(header);
        }
        else{
            insert_block(header);
            void* newblock = my_malloc(size);
            if(newblock == NULL){
                return NULL;
            }
            memcpy(newblock,block,size);
            my_free(block);
            return newblock;
        }
    }
}


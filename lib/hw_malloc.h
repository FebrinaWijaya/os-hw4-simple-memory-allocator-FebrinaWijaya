#ifndef HW_MALLOC_H
#define HW_MALLOC_H

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdbool.h>

#define ALLOCATED -1
#define FREE 1

#define MMAP -1
#define HEAP 1

struct chunk_header_t;
typedef struct chunk_header_t chunk_header_t;
typedef chunk_header_t* chunk_ptr_t;

typedef struct chunk_info_t {
    //if >=0, then it is free, if <0, then it is allocated
    int alloc_flag_n_prev_chunk_size;
    //if >=0, then it is allocated by heap, if <0, then it is allocated by mmap
    int mmap_flag_n_cur_chunk_size;
} chunk_info_t;

typedef struct chunk_header_t {
    chunk_ptr_t prev;
    chunk_ptr_t next;
    chunk_info_t size_and_flag;
} chunk_header_t;

typedef struct mmap_head_t {
    chunk_ptr_t prev;
    chunk_ptr_t next;
} mmap_head_t;

void *start_sbrk;
chunk_header_t *mmap_head;
chunk_header_t *bin[11];

void *hw_malloc(size_t bytes);
int hw_free(void *mem);
void *get_start_sbrk(void);

void *add_to_list(void *addr,size_t size, int alloc_method); //alloc method = MMAP or HEAP
void split(void *addr, size_t chunk_size, size_t requested_size);
chunk_header_t* merge(chunk_header_t* head1, chunk_header_t* head2, int init_size);
int get_bin_index(int size);
int get_sign(int num);

#endif
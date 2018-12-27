#include "hw_malloc.h"
#define DEBUG 0
size_t mmap_treshold = 32768;

void *hw_malloc(size_t bytes)
{
    size_t size = bytes + sizeof(chunk_header_t);
    if(size > mmap_treshold) {
        //use mmap
        void* addr = mmap(NULL, size, PROT_READ|PROT_WRITE,
                          MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
        if (addr == MAP_FAILED)
            return NULL;
        return add_to_list(addr, size, MMAP);
    } else {
        //use heap
        if(start_sbrk == NULL) {
            start_sbrk = sbrk(64*1024);
            int i;
            for(i=0; i<11; i++) {
                bin[i] = (chunk_header_t *)malloc(sizeof(chunk_header_t));
                bin[i]->next = bin[i];
                bin[i]->prev = bin[i];
            }
            chunk_header_t *start_brk = (chunk_header_t *)start_sbrk;
            (start_brk->size_and_flag).alloc_flag_n_prev_chunk_size = 1;
            split(start_sbrk, 64*1024, size);
            return (void *)sizeof(chunk_header_t);
        } else {
            int i = get_bin_index(size); //min bin index for requested size
            chunk_header_t* selected_chunk;
            while(i<11) {
                if(bin[i]->next!=bin[i]) {
                    selected_chunk = bin[i]->next;

                    chunk_header_t *head = bin[i]->next;
                    while(head!=bin[i]) {
                        if(head<selected_chunk)
                            selected_chunk = head;
                        head = head->next;
                    }
                    selected_chunk->prev->next = selected_chunk->next;
                    selected_chunk->next->prev = selected_chunk->prev;

                    // bin[i]->next = selected_chunk->next;
                    // selected_chunk->next->prev = bin[i];
                    break;
                }
                ++i;
            }
            if(selected_chunk == NULL) return NULL;
            split(selected_chunk, (selected_chunk->size_and_flag).mmap_flag_n_cur_chunk_size, size);
            return (void *)((void *)selected_chunk + sizeof(chunk_header_t) - start_sbrk);
        }
    }
    return NULL;
}

int hw_free(void *mem)
{
    chunk_header_t *chunk_header = (chunk_header_t *)(mem - sizeof(chunk_header_t));
    if((long int)mem < 64*1024) chunk_header = (void *)chunk_header + (long int)start_sbrk;
    if(chunk_header == NULL) return -1;
    if((chunk_header->size_and_flag).mmap_flag_n_cur_chunk_size/MMAP >= 0) { //allocated by MMAP
        if(DEBUG) printf("free mmap\n");
        chunk_header_t *head = mmap_head->next;
        while(head!=chunk_header && head!=mmap_head)
            head = head->next;
        head->prev->next = head->next;
        head->next->prev = head->prev;

        int size = (chunk_header->size_and_flag).mmap_flag_n_cur_chunk_size/MMAP;
        return munmap(chunk_header, size); //0=success; -1=failed
    } else { //allocated by heap
        if(DEBUG) printf("free heap\n");
        if((chunk_header->size_and_flag).alloc_flag_n_prev_chunk_size / ALLOCATED < 0) //mem is free
            return 0;
        (chunk_header->size_and_flag).alloc_flag_n_prev_chunk_size /= ALLOCATED; //+size
        int cur_size = (chunk_header->size_and_flag).mmap_flag_n_cur_chunk_size;
        int prev_size = (chunk_header->size_and_flag).alloc_flag_n_prev_chunk_size;
        add_to_list(chunk_header, cur_size, HEAP);

        bool merged = true;
        while(merged) {
            if(cur_size == 32*1024) break;
            merged = false;
            chunk_header_t *prev_chunk = (chunk_header_t *)((void *)chunk_header-prev_size);
            if((void *)prev_chunk >= start_sbrk
                    && (prev_chunk->size_and_flag).alloc_flag_n_prev_chunk_size / ALLOCATED < 0 //free
                    && (prev_chunk->size_and_flag).mmap_flag_n_cur_chunk_size == cur_size) {
                chunk_header = merge(chunk_header, prev_chunk, cur_size);
                merged = true;
                cur_size = (chunk_header->size_and_flag).mmap_flag_n_cur_chunk_size;
                prev_size = (chunk_header->size_and_flag).alloc_flag_n_prev_chunk_size;
                continue;
            }
            chunk_header_t *next_chunk = (chunk_header_t *)((void *)chunk_header+cur_size);
            if((void *)next_chunk < start_sbrk + 64*1024
                    && (next_chunk->size_and_flag).alloc_flag_n_prev_chunk_size / ALLOCATED < 0 //free
                    && (next_chunk->size_and_flag).mmap_flag_n_cur_chunk_size == cur_size) {
                if(DEBUG) printf("merged\n");
                chunk_header = merge(chunk_header, next_chunk, cur_size);
                merged = true;
                cur_size = (chunk_header->size_and_flag).mmap_flag_n_cur_chunk_size;
                prev_size = (chunk_header->size_and_flag).alloc_flag_n_prev_chunk_size;
                continue;
            }
        }
    }
    return 0;
}

void *get_start_sbrk(void)
{
    return start_sbrk;
}

void *add_to_list(void *addr, size_t size, int alloc_method)
{
    // chunk_ptr_t chunk_ptr = (chunk_ptr_t)addr;
    // chunk_ptr->header = chunk_ptr;
    // chunk_ptr->data = chunk_ptr + sizeof(chunk_header_t);
    chunk_header_t *chunk_header = (chunk_header_t *)addr;
    if(alloc_method == MMAP) {
        (chunk_header->size_and_flag).mmap_flag_n_cur_chunk_size = size*MMAP; //-size
        (chunk_header->size_and_flag).alloc_flag_n_prev_chunk_size = ALLOCATED; //-1
        if(mmap_head == NULL) {
            //mmap_head = (mmap_head_t *)malloc(sizeof(mmap_head_t));
            mmap_head = (chunk_header_t *)malloc(sizeof(chunk_header_t));
            mmap_head->next = chunk_header;
            mmap_head->prev = chunk_header;
            chunk_header->next = mmap_head;
            chunk_header->prev = mmap_head;
            return chunk_header + 1; //address of (pointer + 1) = pointer + sizeof(pointer_type)
        }

        chunk_header_t *head = mmap_head->next;
        while(head!=mmap_head && ((head->size_and_flag).mmap_flag_n_cur_chunk_size * MMAP <= size))
            head = head->next;
        chunk_header->next = head;
        chunk_header->prev = head->prev;
        head->prev->next = chunk_header;
        head->prev = chunk_header;
        return chunk_header + 1; //address of (pointer + 1) = pointer + sizeof(pointer_type)
    } else if(alloc_method == HEAP) {
        (chunk_header->size_and_flag).mmap_flag_n_cur_chunk_size = size*HEAP; //size
        int i = get_bin_index(size);

        chunk_header->next = bin[i];
        chunk_header->prev = bin[i]->prev;
        bin[i]->prev->next = chunk_header;
        bin[i]->prev = chunk_header;

        // chunk_header_t *head = bin[i]->next;
        // while(head!=bin[i] && head<chunk_header)
        //     head = head->next;
        // chunk_header->next = head;
        // chunk_header->prev = head->prev;
        // head->prev->next = chunk_header;
        // head->prev = chunk_header;
    }
    return NULL;
}

void split(void *addr, size_t chunk_size, size_t requested_size)
{
    void *init_upper = addr + chunk_size;
    while(requested_size <= chunk_size/2) {
        chunk_header_t *chunk_header = (chunk_header_t *)(addr+chunk_size);
        if((void *)chunk_header < (start_sbrk + 64*1024)) {
            int sign = 1;
            if((void *)chunk_header == init_upper)
                sign = get_sign((chunk_header->size_and_flag).alloc_flag_n_prev_chunk_size);
            (chunk_header->size_and_flag).alloc_flag_n_prev_chunk_size = sign*chunk_size/2;
        }
        //printf("adding to list %ld %ld\n", requested_size, chunk_size/2);
        add_to_list(addr+chunk_size/2, chunk_size/2, HEAP);
        //printf("added to list\n");
        chunk_size = chunk_size/2;
    }

    chunk_header_t *chunk_header = (chunk_header_t *)(addr+chunk_size);
    if((void *)chunk_header < (start_sbrk + 64*1024)) {
        int sign = 1;
        if((void *)chunk_header == init_upper)
            sign = get_sign((chunk_header->size_and_flag).alloc_flag_n_prev_chunk_size);
        (chunk_header->size_and_flag).alloc_flag_n_prev_chunk_size = sign*chunk_size;
    }

    chunk_header = (chunk_header_t *)(addr);
    (chunk_header->size_and_flag).alloc_flag_n_prev_chunk_size *= ALLOCATED;
    (chunk_header->size_and_flag).mmap_flag_n_cur_chunk_size = chunk_size*HEAP;
}

chunk_header_t* merge(chunk_header_t* head1, chunk_header_t* head2, int init_size)
{
    int i = get_bin_index(init_size);
    chunk_header_t *head = bin[i]->next;
    while(head!=head1)
        head = head->next;
    head->prev->next = head->next;
    head->next->prev = head->prev;
    head = bin[i]->next;

    while(head!=head2)
        head = head->next;
    head->prev->next = head->next;
    head->next->prev = head->prev;

    chunk_header_t *head_upper;
    if(head1 < head2) {
        head = head1;
        head_upper = head2;
    } else {
        head = head2;
        head_upper = head1;
    }

    (head->size_and_flag).mmap_flag_n_cur_chunk_size = 2*init_size*HEAP;
    add_to_list(head, 2*init_size, HEAP);

    chunk_header_t *chunk_header = (chunk_header_t *)((void *)head_upper + init_size);
    if((void *)chunk_header < (start_sbrk + 64*1024)) {
        int sign = get_sign((chunk_header->size_and_flag).alloc_flag_n_prev_chunk_size);
        (chunk_header->size_and_flag).alloc_flag_n_prev_chunk_size = sign*2*init_size;
    }

    return head;
}

int get_bin_index(int size)
{
    int i=0;
    while(size>32) { //2^5 = bin[0]
        size/=2;
        ++i;
    }
    return i;
}

int get_sign(int num)
{
    if(num > 0)
        return 1;
    else
        return -1;
}
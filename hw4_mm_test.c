#include "lib/hw_malloc.h"
#include "hw4_mm_test.h"
#define DEBUG 0

int main(int argc, char *argv[])
{
    // printf("%ld\n",sizeof(chunk_info_t)); //8
    // printf("%ld\n",sizeof(chunk_header)); //24

    char buffer[BUF_SIZE];
    while(fgets(buffer, BUF_SIZE, stdin)) {
        //printf("%s", buffer);
        char *token;
        token = strtok(buffer, " ");
        if(strcmp(token, "print") == 0) {
            token = strtok(NULL, " ");
            if(strcmp(token, "mmap_alloc_list") == 0 || strcmp(token, "mmap_alloc_list\n") == 0)
                print_mmap_alloc_list();
            else {
                token[3]='\0';
                if(strcmp(token, "bin") == 0) {
                    int i = atoi(strtok(token+4, "]"));
                    print_bin(i);
                }
            }
        } else if(strcmp(token, "alloc") == 0) {
            token = strtok(NULL, " ");
            void *ptr = hw_malloc(atoi(token));
            if(ptr == NULL)
                perror("hw_malloc returned NULL\n");
            else
                printf("%.12p\n",ptr);
        } else if(strcmp(token, "free") == 0) {
            token = strtok(NULL, " ");
            long int addr_i = strtol(token, NULL, 0);
            //printf("%ld\n", addr_i);
            void *ptr = (void *)addr_i;
            //printf("%p\n", ptr);
            if(hw_free(ptr)==0)
                printf("success\n");
            else
                printf("failed\n");
        }
    }
    return 0;
}
void print_mmap_alloc_list(void)
{
    if(mmap_head == NULL)
        return;
    chunk_header_t *head = mmap_head->next;
    while(head!=mmap_head) {
        printf("%.12p--------%d\n", head, (head->size_and_flag).mmap_flag_n_cur_chunk_size * MMAP);
        head = head->next;
    }
}
void print_bin(int i)
{
    if(bin[i] == NULL)
        return;
    chunk_header_t *head = bin[i]->next;
    while(head!=bin[i]) {
        //print chunk address
        if((void *)((void *)head - start_sbrk) == 0)
            printf("0x000000000000--------%d\n",(head->size_and_flag).mmap_flag_n_cur_chunk_size);
        else
            printf("%.12p--------%d\n", (void *)((void *)head - start_sbrk), (head->size_and_flag).mmap_flag_n_cur_chunk_size);
        if(DEBUG) printf("alloc_flag_n_prev_chunk_size : %d\n",(head->size_and_flag).alloc_flag_n_prev_chunk_size);
        head = head->next;
    }
}

#define _GNU_SOURCE
#include <stdint.h>
#include <sys/mman.h>
#include <string.h>
#define PAGESIZE 4096   

// 8 different buckets 8 16 32 64 128 256 512 1024
#define NUM_BUCKETS 8 

void __attribute__ ((constructor)) init(void);
void __attribute__ ((destructor)) cleanup(void);

// keep track of free block
static uintptr_t *segregated_free_list[NUM_BUCKETS];
static unsigned int map[NUM_BUCKETS];

void init(void){
    int bucket = 0;
    map[bucket] = sizeof(void*);
    for(bucket = 1;bucket<NUM_BUCKETS;bucket++){
        map[bucket] = map[bucket-1]<<1;
    }
}


uintptr_t align(size_t size) {
    return (size + sizeof(void*) - 1) & ~(sizeof(void*) - 1);
}
uintptr_t *getHeader(void* ptr){
    return (uintptr_t *) ((uintptr_t) ptr & ~(uintptr_t) 0xfff);
}
int getBucket(size_t size){
    int bucket = 0;
    while(size > map[bucket]){
        bucket++;
    }
    return bucket;
}

void *malloc (size_t size)
{
    if(size <= 0){
        return NULL;
    }
    if(size > 1024){
        //directly create large chunck of size
        size = align(size);
        void *chunk = mmap(NULL,  size + sizeof(uintptr_t),
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        uintptr_t *ptr = (uintptr_t *)chunk;
        *ptr++ = size;
        return (void *)ptr;
    }else{
        int bucket = getBucket(size);
        //get from free list or create new page
        if(segregated_free_list[bucket] == NULL){
            // create page and add it to seg list
            void *page = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            uintptr_t *ptr = (uintptr_t *)page;
            size = map[bucket];
            *ptr++ = size;
            for(int i = 0; i < (PAGESIZE-sizeof(uintptr_t))/(size + sizeof(uintptr_t *));i++){
                uintptr_t *temp = (uintptr_t *)((uintptr_t) ptr + i * (size + sizeof(uintptr_t *)));
                *temp = (uintptr_t) segregated_free_list[bucket];
                segregated_free_list[bucket] = temp;
            }
        }
        void *ptr = (void *) ((uintptr_t)segregated_free_list[bucket] + sizeof(uintptr_t *));
        segregated_free_list[bucket] = (uintptr_t*)(*segregated_free_list[bucket]);
        return ptr;
    }
}

void free(void *ptr)
{
    if(ptr == NULL) return;
    // put the block back to the free list or ummap it if large chunk
    uintptr_t *page_head = getHeader(ptr);
    uintptr_t size = *page_head;
    if(size > 1024){
        munmap(page_head, size + sizeof(uintptr_t));
    }else{
        int bucket = getBucket(size);
        uintptr_t *temp = (uintptr_t *) ((uintptr_t) ptr - sizeof(uintptr_t *));
        *temp = (uintptr_t)segregated_free_list[bucket];
        segregated_free_list[bucket] = temp;
    }
}

void *calloc(size_t nmemb, size_t size)
{   
    uintptr_t chunkSize = nmemb * size;
    void *ptr = malloc(chunkSize);
    memset(ptr,0,chunkSize);
    return ptr;
}

void *realloc(void *ptr, size_t size)
{
    if(ptr == NULL){
        void *ptr = malloc(size);
        return ptr;
    }else if(size == 0){
        free(ptr);
        return NULL;
    }else{
        // if need change bucket call malloc and memcpy,else do nothing
        uintptr_t *page_head = getHeader(ptr);
        uintptr_t old_size = *page_head;
        uintptr_t new_size = align(size);
        if(old_size < new_size){
            void *old_ptr = ptr;
            ptr = malloc(new_size);
            memcpy(ptr,old_ptr,old_size);
            free(old_ptr);
        }      
    }
    return ptr;
}

void cleanup(void);
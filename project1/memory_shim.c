/*
 * Name: Chong Meng
 * CPSC3220 Intro to Operating System
 * memory_shim.c for memory_shim.so
 * overwrite malloc and free
 */

#define _GNU_SOURCE

void __attribute__ ((constructor)) init(void);
void __attribute__ ((destructor)) cleanup(void);

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

// Original malloc and free 
void *(*original_malloc)(size_t size) = NULL;
void (*original_free)(void *pointer) = NULL;

// Initialize the shim
void init(void)
{
    if (original_malloc == NULL)
    {
        original_malloc = dlsym(RTLD_NEXT, "malloc");
    }
    
    if (original_free == NULL)
    {
        original_free = dlsym(RTLD_NEXT, "free");
    }
}

// Use a linkedList to manage the pointer and the bytes allocated to it.
// Linked list node
struct Node
{
    // Store the pointer
    void *pointer;
    // Store the # of bytes
    int size;

    struct Node *next;
};

// a  dummyNode for convenience;
struct Node *dummyNode = NULL;
// the linkedlist : head ->...->tail->dummyNode
struct Node *head = NULL;
struct Node *tail = NULL;

void *malloc (size_t size)
{
    // call original_malloc for request
    void *pointer = original_malloc(size);
    // allocate memory for the new node
    struct Node *cur = original_malloc(sizeof(struct Node));
    cur->pointer = pointer;
    cur->size = size;
    // link the node to the tail
    if(tail == NULL)
    {
        tail = cur;
        head = tail;
        tail->next = dummyNode;
    }
    else
    {
        tail->next = cur;
        tail = cur;
        tail->next = dummyNode;
    }

    return pointer;
}

void free (void *pointer)
{
    // free the memory as requested
    original_free(pointer);
    
    // delete the node of linkedlist and free its' memory
    struct Node *prev = NULL, *cur = NULL;
    for (cur = head;cur != dummyNode; prev = cur, cur = cur->next)
    {
        if (cur->pointer == pointer)
        {
            if(cur == head)
            {
                head = cur->next;
            }
            else
            {
                prev->next = cur->next;
            }
            original_free(cur);
            return;
        }
    }
}

void cleanup(void)
{
    int leak_count = 0, leak_size = 0;
    struct Node *cur = NULL;

    // Print out the information 
    for(cur = head;cur != dummyNode; cur = cur->next)
    {
        leak_count++;
        leak_size += cur->size;
        fprintf(stderr, "LEAK\t%d\n", cur->size);
    }

    fprintf(stderr, "TOTAL\t%d\t%d\n",leak_count, leak_size);

    // free the remaining nodes of the linkedlist
    // free(NULL) is a nop
    while(head != dummyNode)
    {
        cur = head;
        head = head->next;
        original_free(cur);
    }

}

#define NUM_THREADS 1024 // assuming the largest number of thread actived at a same time

#include "mythreads.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// use a linkedlist to store the information of all threads
typedef struct Thread_t {
    int threadId;
    int status;  // 0:running 1:ready 2:wait 3:finished
    int lockId; // thread may hold the lock
    ucontext_t *context;   // context of the thread
    void *result; // store the results
    struct Thread_t *prev;
    struct Thread_t *next;
}Thread;

typedef struct node_t {
    Thread *thread;
    struct node_t *next;
}node;

typedef struct Lock_t {
    int locked; // 1 : hold by thread, 0 : available
    Thread *owner; 
    node *queue;// thread wait for the lock.
    node *tail; // waiting queue tail
    node *conds[CONDITIONS_PER_LOCK];  // waiting queue for the condition variable
    node *condtails[CONDITIONS_PER_LOCK]; // waiting queue tail for the condition variable
}Lock;

// functions used in this library
Thread* getNextThread(); // get next runnable thread
void freeThread(Thread *thread); // free the memory relevent to thread
void move_to_tail(Thread *thread); // move thread to tail of the queue
void add_to_head(Thread *thread); // add thread to head of the queue
void func_wrapper(void *(*thFuncPtr)(void *), void *args); // wrapper for calling function
int getId(); // get next available ID, just for ID recycle
static void interruptDisable();
static void interruptEnable();
void lockEnqueue(int lockNum, Thread *thread); // let thread wait for lock lockNum
void lockDequeue(int lockNum); // dequeue thread since it can acquire lockNum
void condEnqueue(int lockNum, int conditionNum, Thread *thread); // thread wait for conditionNum under lockNum
void condDequeue(int lockNum, int conditionNum); // dequeue thread at conditionNum's waiting queue

// global variables
int interruptsAreDisabled = 0;
int id_status[NUM_THREADS];
Lock *lock_status[NUM_LOCKS];// Lock register
Thread *thread_dict[NUM_THREADS];// for quick lookup: id ---> thread
Thread *join_list[NUM_THREADS]; // if threadJoin is called, we need do sth after thread is finished.  
Thread *head; // head of the whole thread queue
Thread *tail; // tail of the whole thread queue
Thread *current_thread; // current running thread

void threadInit()
{
    interruptDisable();
    // initialize lock_states[]
    for(int i = 0; i<NUM_LOCKS;i++){
        lock_status[i] = malloc(sizeof(Lock));
        for(int j = 0; j < CONDITIONS_PER_LOCK; j++){
            lock_status[i]->conds[j] = lock_status[i]->condtails[j] = NULL;
        }
        lock_status[i]->locked = 0;
        lock_status[i]->owner = NULL;
        lock_status[i]->queue = lock_status[i]->tail = NULL;
        // no place to free it, but I don't want it be global.
    }
    // initialize main_thread and save its context
    Thread *main_thread = (Thread*)malloc(sizeof(Thread));
    main_thread->context =(ucontext_t*)malloc(sizeof(ucontext_t));
    getcontext(main_thread->context);
   
    main_thread->threadId = NUM_THREADS; // give main_thread a special id : NUM_THREADS
    main_thread->lockId = -1;
    main_thread->status = 0; // it should be current thread, so it's running
    
    main_thread->prev = NULL;
    main_thread->next = NULL;
    main_thread->result = NULL;
    
    // dont need to allocate stack ,just save it;
    getcontext(main_thread->context);
    add_to_head(main_thread);
    current_thread = main_thread;
    interruptEnable();
}

int threadCreate(thFuncPtr funcPtr, void *argPtr)
{
    interruptDisable();
    Thread *new_thread = (Thread*)malloc(sizeof(Thread));
    // initialize the thread
    // store the context on heap
    new_thread->context = (ucontext_t*)malloc(sizeof(ucontext_t));
    getcontext(new_thread->context);
    // allocate and initialize a new call stack
    new_thread->context->uc_stack.ss_sp =  malloc(STACK_SIZE);
    new_thread->context->uc_stack.ss_size = STACK_SIZE;
    new_thread->context->uc_stack.ss_flags = 0;
    
    new_thread->lockId = -1; // should be unlocked at beginning

    // get next available ID, print error if too much threads are running
    int id;
    if((id = getId())!= -1){
        new_thread->threadId = id;
    }else{
        fprintf(stderr, "too much thread");
        exit(EXIT_FAILURE);
    }
    thread_dict[id] = new_thread;
    new_thread->status = 1; // new created thread should be runnable
    new_thread->prev = NULL;
    new_thread->next = NULL;
    new_thread->result = NULL;
    // make the context and Add the Thread to the Queue (Thread List)
    makecontext(new_thread->context, (void (*) (void))func_wrapper,2, funcPtr, argPtr);
    add_to_head(new_thread);
    // for better scheduling, we should better move current_thread to the tail
    move_to_tail(current_thread);
    current_thread->status = 1; //make it ready to run
    new_thread->status = 0; // make new_thread to run
    // switch the context to run new_thread
    Thread *old_thread = current_thread;
    current_thread = new_thread;
    swapcontext(old_thread->context, current_thread->context);
    interruptEnable();
    return new_thread->threadId;
    }

void threadYield()
    {
    interruptDisable();
    current_thread->status = 1; // mark current_thread ready to run
    move_to_tail(current_thread);
    // run next runnable thread;
    Thread *old_thread = current_thread; 
    current_thread = getNextThread();
    // mark it as running
    current_thread->status = 0;
    swapcontext(old_thread->context, current_thread->context);
    interruptEnable();
}

void threadJoin(int thread_id, void **result)
{
    interruptDisable();
    // if the thread is not existed
    if(thread_id >= NUM_THREADS || id_status[thread_id] == 0)
        return;
    // if already finished
    if(thread_dict[thread_id]->status == 3)
    {   
        if(result != NULL)
            *result = thread_dict[thread_id]->result;
        // after the thread->result is called, we can safely free it
        freeThread(thread_dict[thread_id]);
    }
    else
    {
        // thread_id is not finished. change current thread's status to wait until thread_id finished
        // move it to tail
        current_thread->status = 2;
        move_to_tail(current_thread);
        // get next runnable thread;
        Thread *old_thread = current_thread; 
        current_thread = getNextThread();
        
        current_thread->status = 0; // mark it as running

        // current_thread is waitting for thread_id
        join_list[thread_id] = old_thread;

        // run next runnable thread
        swapcontext(old_thread->context, current_thread->context);

        // if swap back, means thread_id is finished
        if(result != NULL)
            *result = thread_dict[thread_id]->result;
        // if the thread->result is called, we can safely free it
        freeThread(thread_dict[thread_id]);
    }
    interruptEnable();
}

void threadExit(void *result)
{
    interruptDisable();
    if(current_thread->threadId == NUM_THREADS)
    {
        exit(EXIT_SUCCESS);
    }
    else
    {
        current_thread->status = 3; // current_thread's status is finished
        current_thread->result = result;
        if(join_list[current_thread->threadId] != NULL){
            join_list[current_thread->threadId]->status = 1;// make join thread ready to run
            join_list[current_thread->threadId] = NULL; // free the join list
        }
        move_to_tail(current_thread);
        Thread *old_thread = current_thread;
        current_thread = getNextThread();
        current_thread->status = 0;
        swapcontext(old_thread->context, current_thread->context);
    }
}

void threadLock(int lockNum)
{   
    if(interruptsAreDisabled == 0){
        interruptDisable();
    }
    if(lock_status[lockNum]->locked == 0 && lock_status[lockNum]->queue == NULL){
        lock_status[lockNum]->locked = 1;
        lock_status[lockNum]->owner = current_thread;
        current_thread->lockId = lockNum;
    }
    else //if(lock_status[lockNum]->locked == 1), we need wait for the lock to be released
    {   
        lockEnqueue(lockNum, current_thread);
        current_thread->status = 2;  // mark current thread as wait until thread_id finished
        move_to_tail(current_thread); // move it to tail
        // get next runnable thread;
        Thread *old_thread = current_thread; 
        current_thread = getNextThread();
        current_thread->status = 0; // mark it as running
        // run next runnable thread
        swapcontext(old_thread->context, current_thread->context);
        
        // swap back mean lock is ready for current_thread
        lock_status[lockNum]->locked = 1;
        lock_status[lockNum]->owner = current_thread;
        current_thread->lockId = lockNum;
        lockDequeue(lockNum);
    }
    interruptEnable();
}

void threadUnlock(int lockNum)
{
    interruptDisable();
    if(lock_status[lockNum]->locked == 1 ){
        lock_status[lockNum]->owner->lockId = -1;
        lock_status[lockNum]->locked = 0;
        lock_status[lockNum]->owner = NULL;
        if(lock_status[lockNum]->queue != NULL)
            lock_status[lockNum]->queue->thread->status = 1; // change the status of first thread in the queue to runnable
    }
    interruptEnable();
}

void threadWait(int lockNum, int conditionNum) {
    interruptDisable();
    if(lock_status[lockNum]->locked == 0 || lock_status[lockNum]->owner != current_thread){
        fprintf(stderr, "error: call threadWait for unused lock");
        exit(EXIT_FAILURE);
    }
    else
    {
        lock_status[lockNum]->locked = 0;
        lock_status[lockNum]->owner = NULL;
        if(lock_status[lockNum]->queue != NULL)
            lock_status[lockNum]->queue->thread->status = 1;

        condEnqueue(lockNum, conditionNum, current_thread); // put current_thread in to conditionNum's waitting queue
        current_thread->lockId = -1;
        current_thread->status = 2;
        move_to_tail(current_thread);
        // get next runnable thread;
        Thread *old_thread = current_thread; 
        current_thread = getNextThread();
        current_thread->status = 0; // mark it as running
        swapcontext(old_thread->context, current_thread->context);

        // swap back : got the signal
        // get the lock for current_thread
        threadLock(lockNum);
    }
    if(interruptsAreDisabled == 1){
        interruptEnable();
    }
    
}

void threadSignal(int lockNum, int conditionNum) {
    interruptDisable();
    if(lock_status[lockNum]->conds[conditionNum] != NULL){
        lock_status[lockNum]->conds[conditionNum]->thread->status = 1; // tell it ready to run;
        condDequeue(lockNum, conditionNum); // dequeue specific thread
    }
    interruptEnable();
}

// helper functions
Thread* getNextThread()
{
    Thread *temp_thread = head;
    while(temp_thread->status != 1)
    {
        temp_thread = temp_thread->next;
        if(temp_thread == NULL)
            return NULL;
    }
    return temp_thread;
}

void freeThread(Thread *thread)
{
    if(thread == head){
        head = head->next;
        head->prev = NULL;
    }
    else if (thread == tail)
    {
        tail = tail->prev;
        tail->next = NULL;
         
    }
    else
    {
        thread->prev->next = thread->next;
        thread->next->prev = thread->prev;
    }
    free(thread->context->uc_stack.ss_sp);
    free(thread->context);
    thread_dict[thread->threadId] = NULL;
    id_status[thread->threadId] = 0;
    free(thread);
}

void move_to_tail(Thread *thread)
{
    if(thread->next == NULL) // already at tail
        return;
    if(thread == head)
    {
        head = thread->next;
        head->prev = NULL;  
    }
    else
    {
        thread->prev->next = thread->next;
        thread->next->prev = thread->prev;
    }
    tail->next = thread;
    thread->prev = tail;
    tail = thread;
    tail->next = NULL;
}

void add_to_head(Thread *thread)
{       
    if(head==NULL)
    {
        head = thread;
        tail = head;
    }
    else
    {
        thread->next = head;
        head->prev = thread;
        head = thread;
    }       
}

// wrapper for calling function
void func_wrapper(thFuncPtr funcPtr, void *args){
    interruptEnable();
    void *result = funcPtr(args);
    threadExit(result);
}

int getId()
{
    for(int i = 0; i<NUM_THREADS;i++)
    {
        if(id_status[i]==0)
        {
            id_status[i]=1;
            return i;
        }
           
    }
    return -1;
}

// method for enabling/disabling of interrupts
static void interruptDisable()
{
    assert(!interruptsAreDisabled);
    interruptsAreDisabled = 1;
}

static void interruptEnable()
{
    assert(interruptsAreDisabled);
    interruptsAreDisabled = 0;
}

void lockEnqueue(int lockNum, Thread *thread)
{
    node *temp = malloc(sizeof(node));
    temp->thread = thread;
    temp->next = NULL;

    if(lock_status[lockNum]->queue == NULL){
        lock_status[lockNum]->queue = lock_status[lockNum]->tail = temp;
    }
    else
    {
        lock_status[lockNum]->tail->next = temp;
        lock_status[lockNum]->tail = temp;
    }
}

void lockDequeue(int lockNum)
{
    if(lock_status[lockNum]->queue->next == NULL){
        free(lock_status[lockNum]->queue);
        lock_status[lockNum]->queue = lock_status[lockNum]->tail = NULL;
    }
    else
    {
        node *temp = lock_status[lockNum]->queue;
        lock_status[lockNum]->queue = lock_status[lockNum]->queue ->next;
        free(temp);
    }
}
void condEnqueue(int lockNum, int conditionNum, Thread *thread)
{
    node *temp = malloc(sizeof(node));
    temp->thread = thread;
    temp->next = NULL;
    if(lock_status[lockNum]->conds[conditionNum] == NULL){
        lock_status[lockNum]->conds[conditionNum] = lock_status[lockNum]->condtails[conditionNum] = temp;
    }
    else
    {
        lock_status[lockNum]->condtails[conditionNum]->next = temp;
        lock_status[lockNum]->condtails[conditionNum] = temp;
    }
    
}
void condDequeue(int lockNum, int conditionNum)
{
    if(lock_status[lockNum]->conds[conditionNum]->next == NULL){
        free(lock_status[lockNum]->conds[conditionNum]);
        lock_status[lockNum]->conds[conditionNum] = lock_status[lockNum]->condtails[conditionNum] = NULL;
    }
    else
    {
        node *temp = lock_status[lockNum]->conds[conditionNum];
        lock_status[lockNum]->conds[conditionNum] = temp->next;
        free(temp);
    }
}

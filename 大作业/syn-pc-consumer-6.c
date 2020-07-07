/*  compiling with -lpthread

    this version works properly
    file list:  syn-pc-con-6.h
                syn-pc-con-6.c
                syn-pc-producer-6.c
                syn-pc-consumer-6.c
    with process shared memory and semaphores 
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "syn-pc-con-6.h"

#define gettid() syscall(__NR_gettid)

void *consumer(void *arg)
{
    void *shm = (void *)arg; // shm delivered from main()
    struct ctln_pc_st *ctln = NULL;
    struct data_pc_st *data = NULL;
    ctln = (struct ctln_pc_st *)shm;
    data = (struct data_pc_st *)shm;

    while ((ctln->consume_num < ctln->item_num) || (ctln->END_FLAG == 0))  { 
        sem_wait(&ctln->stock);  /* if stock is empty and all producers stop working at this point, one or more consumers may wait forever */
        sem_wait(&ctln->sem_mutex);
        if (ctln->consume_num < ctln->item_num) { 
            ctln->dequeue = (ctln->dequeue + 1) % ctln->BUFFER_SIZE;
            printf("                                consumer tid %ld taken item no %d by pro %ld, now dequeue = %d\n", gettid(), (data + ctln->dequeue + BASE_ADDR)->item_no, (data + ctln->dequeue + BASE_ADDR)->pro_tid, ctln->dequeue);
            ctln->consume_num++;
            sem_post(&ctln->emptyslot);
        }
        else {
            sem_post(&ctln->stock);
        }
        sem_post(&ctln->sem_mutex);
    }
    pthread_exit(0);
}

int main(int argc, char *argv[])
{
    struct ctln_pc_st *ctln = NULL;  // point to shared control para, within BASE_ADDR
    struct data_pc_st *data = NULL;  // point to shared data, beyond BASE_ADDR

    int shmid;
    void *shm = NULL;
    shmid = strtol(argv[1], NULL, 10); // shmnid delivered from syn-pc-con
    shm = shmat(shmid, 0, 0);
    if (shm == (void *)-1) {
        perror("\nsyn-pc-consumer shmat failed");
        exit(EXIT_FAILURE);
    }

    ctln = (struct ctln_pc_st *)shm;
    data = (struct data_pc_st *)shm;

    pthread_t tidc[ctln->THREAD_CONS];
    int i, ret;
    for (i = 0; i < ctln->THREAD_CONS; ++i) {
        ret = pthread_create(&tidc[i], NULL, &consumer, shm); 
        if (ret != 0) {
            perror("\nsyn-pc-consumer thread create error");
            break;
        }
    } /* we can also define shm and shared as global variables and used by all threads */

    for (i = 0; i < ctln->THREAD_CONS; ++i)
        pthread_join(tidc[i], NULL);

    if (shmdt(shm) == -1) {
        perror("\nshm-pc-consumer shmdt() failed");
        exit(EXIT_FAILURE);
    }  
    return 0;
}


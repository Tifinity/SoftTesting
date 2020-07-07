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

void *producer(void *arg)
{
    void *shm = (void *)arg; // shm delivered from main()
    struct ctln_pc_st *ctln = NULL;
    struct data_pc_st *data = NULL;
    ctln = (struct ctln_pc_st *)shm;
    data = (struct data_pc_st *)shm;

    while (ctln->item_num < ctln->MAX_ITEM_NUM) {
        sem_wait(&ctln->emptyslot);
        sem_wait(&ctln->sem_mutex);

        if (ctln->item_num < ctln->MAX_ITEM_NUM) { /* upperbound of item_num must be controled in critical section */
            ctln->item_num++;		
            ctln->enqueue = (ctln->enqueue + 1) % ctln->BUFFER_SIZE;
            (data + ctln->enqueue + BASE_ADDR)->item_no = ctln->item_num;
            (data + ctln->enqueue + BASE_ADDR)->pro_tid = gettid();
            printf("producer tid %ld prepared item no %d, now enqueue = %d\n", (data + ctln->enqueue + BASE_ADDR)->pro_tid, (data + ctln->enqueue + BASE_ADDR)->item_no, ctln->enqueue);
            if (ctln->item_num == ctln->MAX_ITEM_NUM)
                ctln->END_FLAG = 1;
            sem_post(&ctln->stock);
        } 
        else {
            sem_post(&ctln->emptyslot);
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
    shmid = strtol(argv[1], NULL, 10); // shmnid delivered from syn-pc-con.o
    shm = shmat(shmid, 0, 0);
    if (shm == (void *)-1) {
        perror("\nsyn-pc-producer shmat failed");
        exit(EXIT_FAILURE);
    }

    ctln = (struct ctln_pc_st *)shm;
    data = (struct data_pc_st *)shm;

    pthread_t tidp[ctln->THREAD_PRO];
    int i, ret;
    for (i = 0; i < ctln->THREAD_PRO; ++i) {
        ret = pthread_create(&tidp[i], NULL, &producer, shm);
        if (ret != 0) {
            perror("\nsyn-pc-producer thread create error");
            break;
        }
    }    

    for (i = 0; i < ctln->THREAD_PRO; ++i) {
        pthread_join(tidp[i], NULL);
    }

    for (i = 0; i < ctln->THREAD_CONS - 1; ++i) /* all producers stop working, in case some consumer takes the last stock and no more than THREAD_CON-1 consumers stick in the sem_wait(&stock) */
        sem_post(&ctln->stock);

    if (shmdt(shm) == -1) {
        perror("\nshm-pc-producer shmdt() failed");
        exit(EXIT_FAILURE);
    }
    return 0;
}


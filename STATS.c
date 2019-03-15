#include <sys/types.h>

#include <sys/wait.h>
#include <sys/sem.h>
#include <stdbool.h>

#include <sys/shm.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shm.h"

#define NUMSEMS 5

static int set_semvalue(int semid, int semnum)
{
    union semun sem_union;
    sem_union.val = 1;
    if (semctl(semid, semnum, SETVAL, sem_union) == -1)
        return 0;
    return 1;
}

static void del_semvalue(int semid, int semnum)
{
    union semun sem_union;
    if (semctl(semid, semnum, IPC_RMID, sem_union) == -1)
        fprintf(stderr, "Failed to delete semaphore\n");
}

static int p(int semid, int child)
{
    struct sembuf sem_b;
    sem_b.sem_num = child;
    sem_b.sem_op = -1; /* P() sem_b.sem_flg = SEM_UNDO; if (semop(sem_id, &sem_b,*/
    if (semop(semid, &sem_b, 1) == -1)
    {
        fprintf(stderr, "semaphore_p failed\n");
        return 0;
    }
    return 1;
}

static int v(int semid, int child)
{
    struct sembuf sem_b;
    sem_b.sem_num = child;
    sem_b.sem_op = 1; /* V() */
    sem_b.sem_flg = SEM_UNDO;
    if (semop(semid, &sem_b, 1) == -1)
    {
        fprintf(stderr, "semaphore_v failed\n");
        return 0;
    }
    return 1;
}

void collectData(void)
{
    printf("Please enter 5 integers:\n");
    for (int i = 0; i < SIZE; i++)
    {
        scanf("%d", &data[i]);
    }
}

void printData(void)
{
    for (int i = 0; i < SIZE; i++)
    {
        printf("%d ", data[i]);
    }
    printf("\n");
}

bool sorted(void)
{
    for (int i = 1; i < SIZE; i++)
    {
        if (data[i - 1] < data[i])
        {
            return false;
        }
    }
    return true;
}

void sort(int semid, int pi)
{
    int tmp;
    while(!sorted())
    {
        if (data[pi + 1] > data[pi])
        {
            
            tmp = data[pi];
            p(semid, pi);
            data[pi] = data[pi + 1];
            v(semid, pi);
            p(semid, pi+1);
            data[pi + 1] = tmp;
            v(semid, pi+1);
        }
    }
}

int getMedian(void)
{
    if (sorted())
    {
        return data[2];
    }
    return 0;
}

int getMax(void)
{
    if (sorted())
    {
        return data[0];
    }
    return 0;
}

int getMin(void)
{
    if (sorted())
    {
        return data[4];
    }
    return 0;
}

int main(void)
{
    key_t semkey, shmkey;
    int semid, shmid;
    void *shared_mem;

    semkey = ftok("./", 1);
    if (semkey == (key_t)-1)
    {
        printf("main: ftok() for sem failed\n");
        return -1;
    }

    shmkey = ftok("./", 1);
    if (shmkey == (key_t)-1)
    {
        printf("main: ftok() for shm failed\n");
        return -1;
    }

    semid = semget(203948, NUMSEMS, 0666 | IPC_CREAT);
    if (semid == -1)
    {
        printf("main: semget() failed\n");
        return -1;
    }

    for (int i = 0; i < NUMSEMS; i++)
    {
        if (!set_semvalue(semid, i))
        {
            fprintf(stderr, "Failed to initialize semaphore\n");
            exit(EXIT_FAILURE);
        }
    }

    shmid = shmget(shmkey, SIZE * sizeof(int), 0666 | IPC_CREAT); // shared memory identity
    if (shmid == -1)
    {
        fprintf(stderr, "shmget failed\n");
        exit(EXIT_FAILURE);
    }

    shared_mem = shmat(shmid, (void *)0, 0); // attaching shared memory to address space of process
    if (shared_mem == (void *)-1)
    {
        fprintf(stderr, "shmat failed\n");
        exit(EXIT_FAILURE);
    }

    data = (int *)shared_mem;

    collectData();
    printData();
    printf("\n");

    printf("%d\n", getMax());
    printf("%d\n", getMin());
    printf("%d\n", getMedian());

    int a = 4; // number of child processes
    pid_t pids[a];

    /* Start children. */
    for (int i = 0; i < a; ++i)
    {
        if ((pids[i] = fork()) < 0) // if fork() fails
        {
            perror("fork");
            abort();
        }
        else if (pids[i] == 0) // if process is a child
        {
            sort(semid, i);
            exit(0);
        }
    }

    /* Wait for children to exit. */
    int status;
    pid_t pid;
    while (a > 0)
    {
        pid = wait(&status);
        //printf("Child with PID of %ld of parent %ld exited with status 0x%x.\n", (long)pid, (long)getppid(), status);
        --a; // Remove pid from the pids array.
    }

    printf("\n");
    printData();

    for (int i; i < NUMSEMS; ++i)
    {
        del_semvalue(semid, i);
    }
    
    if (shmdt(shared_mem) == -1)
    {
        fprintf(stderr, "shmdt failed\n");
        exit(EXIT_FAILURE);
    }
    if (shmctl(shmid, IPC_RMID, 0) == -1) 
    {
        fprintf(stderr, "shmctl failed\n");
        exit(EXIT_FAILURE);
    }
    
}
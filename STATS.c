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

#define SEMKEY 203948
#define SHMKEY 102985
#define NUMSEMS 5 // # of semaphores in semaphore set (one for each process)

/**
 * Initialize values of all the semaphores in the semaphore set
 */
static int set_semvalue(int semid, int semnum)
{
    union semun sem_union;
    sem_union.val = 1;
    if (semctl(semid, semnum, SETVAL, sem_union) == -1)
        return 0;
    return 1;
}

/**
 * Deletes all semaphores in semaphore set
 */
static void del_semvalue(int semid)
{
    union semun sem_union;
    if (semctl(semid, 0, IPC_RMID, sem_union) == -1)
        fprintf(stderr, "Failed to delete semaphore\n");
}

/**
 * Wait function
 */
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

/**
 * Signal function
 */
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

/**
 * Creates and array from user inputted integers
 */
void collectData(void)
{
    printf("Please enter 5 integers:\n");
    for (int i = 0; i < SIZE; i++)
    {
        scanf("%d", &data[i]);
    }
}

/**
 * Prints the array
 */
void printData(void)
{
    for (int i = 0; i < SIZE; i++)
    {
        printf("%d ", data[i]);
    }
    printf("\n");
}

/**
 * Checks if array is sorted in decreasing order
 */
bool sorted(void)
{
    for (int i = 0; i < SIZE; i++)
    {
        if (data[i] < data[i + 1])
        {
            return false;
        }
    }
    return true;
}

/**
 * Sorts the array
 */
void sort(int semid, int pi, char debug)
{
    int tmp;
    while (!sorted())
    {
        if (data[pi + 1] > data[pi])
        {
            /**
             * Request keys for two adjacent array elements
             */
            p(semid, pi);
            p(semid, pi + 1);
            /**
             * Swapping here
             */
            tmp = data[pi];
            data[pi] = data[pi + 1];
            data[pi + 1] = tmp;
            /**
             * Release keys for two adjacent array elements
             */
            v(semid, pi);
            v(semid, pi + 1);
            if (debug == 'y')
                printf("Process %d: Performed swapping\n", pi + 1);
        }
        else
        {
            if (debug == 'y')
                printf("Process %d: No swapping\n", pi + 1);
        }
    }
}

/**
 * Gets median if sorted
 */
int getMedian(void)
{
    if (sorted())
    {
        return data[2];
    }
    return 0;
}

/**
 * Gets maximum if sorted
 */
int getMax(void)
{
    if (sorted())
    {
        return data[0];
    }
    return 0;
}

/**
 * Gets minimum if sorted
 */
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
    int semid, shmid; // semaphore set and shared memory identities, respectively
    void *shared_mem;
    char debug; // for debug mode

    semid = semget(SEMKEY, NUMSEMS, 0666 | IPC_CREAT); // creating semaphore set of 5
    if (semid == -1)
    {
        printf("main: semget() failed\n");
        return -1;
    }

    for (int i = 0; i < NUMSEMS; i++) // initializes semaphores
    {
        if (!set_semvalue(semid, i))
        {
            fprintf(stderr, "Failed to initialize semaphore\n");
            exit(EXIT_FAILURE);
        }
    }

    shmid = shmget(SHMKEY, SIZE * sizeof(int), 0666 | IPC_CREAT); // shared memory identity
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

    data = (int *)shared_mem; // assigning shared memory to the struct in the header file "shm.h"

    collectData();
    printf("Unsorted array: ");
    printData();
    printf("Debug Mode? [y/n]\n");
    scanf(" %c", &debug); // input for debug mode

    int a = SIZE - 1; // number of child processes
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
            sort(semid, i, debug);
            exit(0);
        }
    }

    while (!sorted()) // check if array is sorted
    {
        sorted();
    }

    /* wait for children to exit. */
    int status;
    pid_t pid;
    while (a > 0)
    {
        pid = wait(&status);
        --a; // remove pid from the pids array.
    }

    printf("Sorted array: ");
    printData();
    printf("Maximum: %d\n", getMax());
    printf("Minimum: %d\n", getMin());
    printf("Median: %d\n", getMedian());

    del_semvalue(semid); // delete all semaphores

    if (shmdt(shared_mem) == -1) // detach shared memory
    {
        fprintf(stderr, "shmdt failed\n");
        exit(EXIT_FAILURE);
    }

    if (shmctl(shmid, IPC_RMID, 0) == -1) // delete shared memory segment
    {
        fprintf(stderr, "shmctl failed\n");
        exit(EXIT_FAILURE);
    }
}
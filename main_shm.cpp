#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <semaphore.h>
//
// POSIX semaphore
//
// sem_t *sem_open(const char *name, int oflag, ...);
// : initialize and open a named semaphore
// : establish a connection between a named semaphore and a process
// : O_CREATE - create a semaphore if it does not exist. If O_CREATE is set and the semaphore
//              already exist, then O_CREATE has no effect. It requires a third and a fourth 
//              arguments: mode and (initial) value (unsigned).
// : O_EXCL - if O_EXCL and O_CREATE are set, sem_open() fails if the semaphore name exists.
//
// int sem_post(sem_t *sem);
// : it increments the value of the semaphore and waks up a blocked process waiting on the semaphore, if any
//
// int sem_wait(sem_t * sem);
// : It decrements (locks) the semaphore pointed by sem. If the semaphore's value is greater
//   than zero, then the decrement proceeds, and the function returns, immediately. If the 
//   semaphore currently has the value zero, then the call blocks until either it becomes
//   possible to perform the decrement, or a signal handler interrupts the call.


#include <fcntl.h>
#include <unistd.h>

#include <string.h>

// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
#include <iostream>
#include <string>

#include <mpi.h>

#define MAX_BUFFERS 10

#define LOGFILE "example.log"

#define SHARED_MEM_NAME "/myshared-mem"
#define SEM_MUTEX_NAME "/sem-mutex"
#define SEM_COUNT_NAME "/sem-count"
#define SEM_SIGNAL_NAME "/sem-signal"
#define SEM_WRITER_NAME "/sem-writer"

typedef struct _shared_memory {
    char buf[MAX_BUFFERS][256];
    int  index;
    int  pindex;
} shared_memory;

void error(std::string msg);

int shm_reader()
{
    shared_memory *shm_ptr;
    //char *shm_ptr;  // pointer to shared memory object
    sem_t *sem_mutex, *sem_count, *sem_signal; 
    //sem_t*sem_writer;
    int   shm_fd;   // shared memory file descriptor
    //int   log_fd;   // log file descriptor
    char mybuf[256];

    // Open log file
    // if ((log_fd = open(LOGFILE, O_CREAT | O_WRONLY | O_APPEND | O_SYNC, 0666)) == -1)
    //     error("fopen");

    // mutual exclusion semaphore, sem_mutex with an initial value 0.
    if ( (sem_mutex = sem_open(SEM_MUTEX_NAME, O_CREAT, 0660, 0)) == SEM_FAILED )
        error("sem_mutex");

    // create the shared memory object
    if ( (shm_fd = shm_open(SHARED_MEM_NAME, O_RDWR | O_CREAT, 0660)) == -1 )
        error("shm_open");

    // configure the size of the shared memory object
    if (ftruncate(shm_fd, sizeof(shared_memory)) == -1)
        error("ftruncate");

    // memory map the shared memory object
    if ((shm_ptr = (shared_memory*)mmap(NULL, sizeof(shared_memory), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)) == MAP_FAILED)
        error("mmap");

    // Initialize the shared memory
    shm_ptr->index = shm_ptr->pindex = 0;

    // counting semaphore, indicating the number of available buffers
    if ((sem_count = sem_open(SEM_COUNT_NAME, O_CREAT, 0660, MAX_BUFFERS)) == SEM_FAILED)
        error("sem_count");

    // counting semaphore, indicating the number of strings to be printed. Initial value = 0
    if ((sem_signal = sem_open(SEM_SIGNAL_NAME, O_CREAT, 0660, 0)) == SEM_FAILED)
        error("sem_signal");

    // counting semaphore, indicating the number of writers, initial value = 0
    // if ((sem_writer = sem_open(SEM_WRITER_NAME, O_CREAT, 0660, 0)) == SEM_FAILED)
    //     error("sem_writer");

    // Initialization complete!
    // now we can set mutex semaphore as 1 to indicate shared memory segment is available
    if (sem_post(sem_mutex) == -1)
        error("sem_post: sem_mutex");

    printf("[SHARED] Start reader ...\n");
    while (true) { // forever ???
        // Is there a string to print?
        if (sem_wait(sem_signal) == -1)
            error("sem_wait: sem_signal");

        strcpy(mybuf, shm_ptr->buf[shm_ptr->pindex]);

        // since there is only one process using the pindex, mutex semaphore is not necessary
        shm_ptr->pindex++;
        if (shm_ptr->pindex == MAX_BUFFERS)
            shm_ptr->pindex = 0;

        // contents of one buffer has been printed. 
        // One more buffer is available for use by writers
        if (sem_post(sem_count) == -1)
            error("sem_post: sem_count");

        // write the string to file
        // if (write(log_fd, mybuf, strlen(mybuf)) != strlen(mybuf))
        //     error("write: logfile");
        printf("%s", mybuf);

        if (strcmp(mybuf, "end\n") == 0)
        {
            break;
        }
    }
    printf("[SHARED] End readers ...\n");

    sem_close(sem_mutex);
    sem_close(sem_count);
    sem_close(sem_signal);

    if (munmap(shm_ptr, sizeof(shared_memory)) == -1)
        error("munmap");

    // unlink
    // - it removes a shared memory object name, and, once all processes have unmapped the object, de-allocates and destroys
    //   the contents of the associated memory region.
    shm_unlink(SHARED_MEM_NAME);
    sem_unlink(SEM_MUTEX_NAME);
    sem_unlink(SEM_COUNT_NAME);
    sem_unlink(SEM_SIGNAL_NAME);
    return 0;
}


int shm_writer()
{
    shared_memory * shm_ptr;
    sem_t *sem_mutex, *sem_count, *sem_signal;
    int shm_fd;
    //char mybuf[256];

    // mutual exculsion semaphore, sem_mutex
    if ((sem_mutex = sem_open(SEM_MUTEX_NAME, 0, 0, 0)) == SEM_FAILED)
        error("sem_open");

    // Get shared memory
    if ((shm_fd = shm_open(SHARED_MEM_NAME, O_RDWR, 0)) == -1)
        error("shm_open");

    if ((shm_ptr = (shared_memory*)mmap(NULL, sizeof(shared_memory), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)) == MAP_FAILED)
        error("mmap");

    // counting semaphore, indicating the number of available buffers
    if ((sem_count = sem_open(SEM_COUNT_NAME, 0, 0, 0)) == SEM_FAILED)
        error("sem_open: sem_count");

    // counting semaphore, indicating the number of strings to be printed. initial value = 0
    if ((sem_signal = sem_open(SEM_SIGNAL_NAME, 0, 0, 0)) == SEM_FAILED)
        error("sem_open: sem_signal");


    char buf[200], *cp;
    //int index = 0;
    bool bEnd = false;

    printf("[SHARED] Start writer ... \n");
    printf("Please type a message: ");
    fflush(stdout);
    while (fgets(buf, 198, stdin)) {
        if (strcmp(buf, "end\n") == 0)
            bEnd = true;

        // remove newline from string
        int length = strlen(buf);
        if (buf[length-1] == '\n')
            buf[length-1] = '\0';

        // get a buffer
        if (sem_wait(sem_count) == -1)
            error("sem_wait: sem_count");

        // there might be multiple producers. we must ensure that only one producer uses buffer_index at a time
        if (sem_wait(sem_mutex) == -1)
            error("sem_wait: sem_mutex");

        {
            // critical section
            time_t now = time(NULL);
            cp = ctime(&now);
            int len = strlen(cp);
            if (*(cp + len - 1) == '\n')
                *(cp + len - 1) = '\0';

            if (bEnd)
                sprintf(shm_ptr->buf[shm_ptr->index], "%s\n", buf);
            else
                sprintf(shm_ptr->buf[shm_ptr->index], "%d: %s %s\n", getpid(), cp, buf);

            (shm_ptr->index)++;
            if (shm_ptr->index == MAX_BUFFERS)
                shm_ptr->index = 0;
        }

        // release mutex
        if (sem_post(sem_mutex) == -1)
            error("sem_post: sem_mutex");

        // tell that there is a string to print
        if (sem_post(sem_signal) == -1)
            error("sem_post: sem_signal");

        if (bEnd) break;

        printf("Please type a message: ");
        fflush(stdout);
    }
    printf("[SHARED] End writer ...\n");

    sem_close(sem_mutex);
    sem_close(sem_signal);
    sem_close(sem_count);

    // finalize
    if (munmap(shm_ptr, sizeof(shared_memory)) == -1)
        error("munmap");

    return 0;
}

int main (int argc, char ** argv)
{
    MPI_Init(&argc, &argv);

    int wrank, wsize;
    
    MPI_Comm_size(MPI_COMM_WORLD, &wsize);
    MPI_Comm_rank(MPI_COMM_WORLD, &wrank);


    int role = atoi(argv[1]); 
    // const char* path = argv[2];
    // int msz_size = atoi(argv[3]);
    // int msz_count = atoi(argv[4]);
    // int max_msz_num = atoi(argv[5]);
    // int buffer_size = msz_size * max_msz_num;
    // int check = atoi(argv[6]);


    if (role == 0)
        shm_reader();
    else
        shm_writer();

    MPI_Finalize();
    return 0;    
}

void error (std::string msg)
{
    perror(msg.c_str());
    exit(1);
}


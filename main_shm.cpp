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
#include <chrono>
#include <iostream>

#include <mpi.h>

using namespace std;
using namespace std::chrono;

#define MAX_BUFFERS 10
#define BUFFER_SIZE 4096

#define LOGFILE "example.log"

#define SHARED_MEM_NAME "/myshared-mem"
#define SEM_MUTEX_NAME "/sem-mutex"
#define SEM_COUNT_NAME "/sem-count"
#define SEM_SIGNAL_NAME "/sem-signal"
#define SEM_WRITER_NAME "/sem-writer"

typedef struct _shared_memory {
    char buf[MAX_BUFFERS][BUFFER_SIZE];
    int  index;
    int  pindex;
} shared_memory;

void error(std::string msg);

int shm_reader(int msz_size, int msz_num, bool bCheck)
{
    shared_memory *shm_ptr;
    sem_t *sem_mutex, *sem_count, *sem_signal; 
    int   shm_fd;   // shared memory file descriptor
    char mybuf[BUFFER_SIZE];

    high_resolution_clock::time_point t1, t2;
    double duration, total_size;

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

    // Initialization complete!
    // now we can set mutex semaphore as 1 to indicate shared memory segment is available
    if (sem_post(sem_mutex) == -1)
        error("sem_post: sem_mutex");

    int i = 0, sum = 0;
    std::cout << "[SHARED] Start reading ..." << std::endl;
    while (i < msz_num) { // forever ???
        // Is there a string to print?
        if (sem_wait(sem_signal) == -1)
            error("sem_wait: sem_signal");

        //strcpy(mybuf, shm_ptr->buf[shm_ptr->pindex]);
        memcpy(
            mybuf,
            shm_ptr->buf[shm_ptr->pindex],
            msz_size
        );
        if (i == 0)
            t1 = high_resolution_clock::now();

        // since there is only one process using the pindex, mutex semaphore is not necessary
        shm_ptr->pindex++;
        if (shm_ptr->pindex == MAX_BUFFERS)
            shm_ptr->pindex = 0;

        // contents of one buffer has been printed. 
        // One more buffer is available for use by writers
        if (sem_post(sem_count) == -1)
            error("sem_post: sem_count");

        // write the string to file
        // printf("%s", mybuf);
        // if (strcmp(mybuf, "end\n") == 0)
        // {
        //     break;
        // }
        if (bCheck)
        {
            for (int j = 0; j < msz_size; j++)
                if (mybuf[j] != char(j%255))
                {
                    std::cout << "Incorrect data: " << mybuf[j] << " vs. " << char(j%255) << std::endl;
                    break;
                }
        }
        i++;
        sum += msz_size;
    }
    t2 = high_resolution_clock::now();
    std::cout << "[SHARED] End reading: " << i << std::endl;

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

    if (sum != msz_num * msz_size)
        std::cout << "Couldn't read all messages!" << std::endl;
    {
        total_size = double(i) * double(msz_size) / 1024.0 / 1024.0; // MBytes
        duration = (double)duration_cast<milliseconds>(t2 - t1).count() / 1000.0; //sec
        std::cout << "[SHARED READER]\n"
                  << "Total # messages : " << i << "\n"
                  << "Message size     : " << msz_size << " Bytes\n"
                  << "Total size       : " << total_size << " MBytes\n"
                  << "Total time       : " << duration << " seconds\n"
                  << "Throughput       : " << total_size / duration << " MBytes/sec\n"
                  << std::endl;
    }
    return 0;
}


int shm_writer(int msz_size, int msz_num)
{
    shared_memory * shm_ptr;
    sem_t *sem_mutex, *sem_count, *sem_signal;
    int shm_fd;
    high_resolution_clock::time_point t1, t2;
    double duration, total_size;

    char * buf;
    int i, sum;

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


    buf = new char[msz_size * msz_num];
    for (i = 0; i < msz_size*msz_num; i++)
    {
        int num = i%msz_size;
        buf[i] = (char)(num%255);
    }
    sum = 0;
    i = 0;
    std::cout << "[SHARED] Start writing: " << msz_size << ", " << msz_num << std::endl;
    t1 = high_resolution_clock::now();
    while (i < msz_num) {
        // get a buffer
        if (sem_wait(sem_count) == -1)
            error("sem_wait: sem_count");

        // there might be multiple producers. we must ensure that only one producer uses buffer_index at a time
        if (sem_wait(sem_mutex) == -1)
            error("sem_wait: sem_mutex");

        {
            // critical section
            memcpy(
                shm_ptr->buf[shm_ptr->index],
                buf + i*msz_size,
                msz_size
            );

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

        i++;
        sum += msz_size;
    }
    t2 = high_resolution_clock::now();
    std::cout << "[SHARED] End writing: " << i << std::endl;

    sem_close(sem_mutex);
    sem_close(sem_signal);
    sem_close(sem_count);

    // finalize
    if (munmap(shm_ptr, sizeof(shared_memory)) == -1)
        error("munmap");

    delete[] buf;
    if (sum != msz_num*msz_size)
        std::cout << "Couldn't write all messages!" << std::endl;
    {
        total_size = double(i) * double(msz_size) / 1024.0 / 1024.0; // MBytes
        duration = (double)duration_cast<milliseconds>(t2 - t1).count() / 1000.0; // sec
        std::cout << "[SHARED WRITER]\n" 
                  << "Total # messages : " << i << "\n"
                  << "Message size     : " << msz_size << " Bytes\n"
                  << "Total size       : " << total_size << " MBytes\n"
                  << "Total time       : " << duration << " seconds\n"
                  << "Throughput       : " << total_size / duration << " MBytes/sec\n"
                  << std::endl;         
    }

    return 0;
}

int main (int argc, char ** argv)
{
    MPI_Init(&argc, &argv);

    int wrank, wsize;
    
    MPI_Comm_size(MPI_COMM_WORLD, &wsize);
    MPI_Comm_rank(MPI_COMM_WORLD, &wrank);


    int role = atoi(argv[1]); 
    int msz_size = atoi(argv[2]);
    int msz_count = atoi(argv[3]);
    int check = atoi(argv[4]);

    if (role == 0)
        shm_reader(msz_size, msz_count, check==1);
    else
        shm_writer(msz_size, msz_count);

    MPI_Finalize();
    return 0;    
}

void error (std::string msg)
{
    perror(msg.c_str());
    exit(1);
}


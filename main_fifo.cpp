#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include <mpi.h>

#include <chrono>
#include <iostream>

using namespace std;
using namespace std::chrono;

int fifo_reader(const char* path, int msz_size, int msz_num, int buffer_size, bool bCheck)
{
    int              fd;
    char           * buf;
    int              i, n, sum;
    high_resolution_clock::time_point t1, t2;
    double duration, total_size;

    if (mkfifo(path, 0666) == -1)
    {
        std::cerr << "[READER] Failed to create fifo: " << path << std::endl;
        return -1;
    }

    fd = open(path, O_RDONLY);
    if (fd == -1)
    {
        std::cerr << "[READER] Failed to open fifo: " << path << std::endl;
        unlink(path);
        return -1;
    }

    if (buffer_size > fcntl(fd, F_GETPIPE_SZ))
    {
        if (fcntl(fd, F_SETPIPE_SZ, buffer_size) == -1)
        {
            std::cerr << "[READER] Failed to resize pipe buffer!\n" 
                      << "- Current: " << fcntl(fd, F_GETPIPE_SZ) << " Bytes\n" 
                      << "- Requested: " << buffer_size << "Bytes\n" 
                      << std::endl;
            unlink(path);
            return -1;
        }
        std::cout << "[READER] Buffer size: " << fcntl(fd, F_GETPIPE_SZ) << " Bytes" << std::endl;
    }

    buf = new char[msz_size];
    sum = 0;

    std::cout << "[FIFO] Start reading ..." << std::endl;
    t1 = high_resolution_clock::now();
    for (i = 0; i < msz_num; i++)
    {
        n = read(fd, buf, msz_size);
        if (n == -1) {
            std::cerr << "Error in reading data!" << std::endl;
            delete [] buf;
            unlink(path);
            return -1;
        }
        if (bCheck) 
        {
            for (int j = 0; j < n; j++)
                if (buf[j] != (char)(j%255))
                {
                    std::cout << "Incorrect data: " << buf[j] << " vs. " << char(j%255) << std::endl;
                    break;
                }
        }
        sum += n;
    }
    t2 = high_resolution_clock::now();
    std::cout << "[FIFO] End reading: " << i << std::endl;

    delete [] buf;

    if (sum != msz_num * msz_size)
    {
        std::cout << "Couldn't read all messages!" << std::endl;
    }
    unlink(path);

    {
        total_size = double(i) * double(msz_size) / 1024.0 / 1024.0; // MBytes
        duration = (double)duration_cast<milliseconds>(t2 - t1).count() / 1000.0; // sec
        std::cout << "[FIFO READER]\n" 
                  << "Total # messages : " << i << "\n"
                  << "Message size     : " << msz_size << " Bytes\n"
                  << "Total size       : " << total_size << " MBytes\n"
                  << "Total time       : " << duration << " seconds\n"
                  << "Throughput       : " << total_size / duration << " MBytes/sec\n"
                  << std::endl; 
    }

    return 0;
}

int fifo_writer(const char* path, int msz_size, int msz_num)
{
    int              fd;
    char           * buf;
    int              i, n, sum;
    high_resolution_clock::time_point t1, t2;
    double duration, total_size;

    fd = open(path, O_WRONLY);
    if (fd == -1)
    {
        std::cerr << "[WRITER] Failed to open fifo: " << path << std::endl;
        return -1;
    }

    buf = new char[msz_size*msz_num];
    for (i = 0; i < msz_size*msz_num; i++)
    {
        int num = i%msz_size;
        buf[i] = (char)(num%255);
    }
    sum = 0;

    std::cout << "[FIFO] Start writing ..." << std::endl;
    t1 = high_resolution_clock::now();
    for (i = 0; i < msz_num; i++)
    {
        n = write(fd, buf + i*msz_size, msz_size);
        if (n != msz_size) {
            std::cerr << "Error in writing data!" << std::endl;
            delete [] buf;
            return -1;
        }
        sum += n;
    }
    t2 = high_resolution_clock::now();
    std::cout << "[FIFO] End writing: " << i << std::endl;

    delete [] buf;

    if (sum != msz_num * msz_size)
    {
        std::cout << "Couldn't write all messages!" << std::endl;
    }

    {
        total_size = double(i) * double(msz_size) / 1024.0 / 1024.0; // MBytes
        duration = (double)duration_cast<milliseconds>(t2 - t1).count() / 1000.0; // sec
        std::cout << "[FIFO WRITER]\n" 
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
    const char* path = argv[2];
    int msz_size = atoi(argv[3]);
    int msz_count = atoi(argv[4]);
    int max_msz_num = atoi(argv[5]);
    int buffer_size = msz_size * max_msz_num;
    int check = atoi(argv[6]);


    if (role == 0)
        fifo_reader(path, msz_size, msz_count, buffer_size, check==1);
    else
        fifo_writer(path, msz_size, msz_count);    

    MPI_Finalize();
    return 0;
}
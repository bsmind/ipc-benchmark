MSZ_SIZE=4096
MSZ_COUNT=500000
READER=0
WRITER=1
CHECK=0

echo "====== BEGIN FIFO ======"
rm -f myfifo
mpirun --allow-run-as-root -n 1 ./fifo $READER myfifo $MSZ_SIZE $MSZ_COUNT 0 $CHECK &
pid_r=$!
sleep 1
mpirun --allow-run-as-root -n 1 ./fifo $WRITER myfifo $MSZ_SIZE $MSZ_COUNT 0 $CHECK
wait $pid_r
echo "====== END FIFO ======"

# echo "Shared"
# mpirun -n 1 ./shm 0 $SIZE 100000 5 &
# sleep 5
# mpirun -n 1 ./shm 1 $SIZE 100000 5

echo "====== BEGIN ADIOS ======"
rm -rf *.bp*
mpirun --allow-run-as-root -n 1 ./adios $READER $MSZ_SIZE $MSZ_COUNT $CHECK &
pid_r=$!
mpirun --allow-run-as-root -n 1 ./adios $WRITER $MSZ_SIZE $MSZ_COUNT $CHECK
wait $pid_r
echo "====== END ADIOS ======"

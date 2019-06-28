MSZ_SIZE=4096
MSZ_COUNT=500000
#MSZ_COUNT=500
READER=0
WRITER=1
CHECK=0

#echo "====== BEGIN FIFO ======"
#rm -f myfifo
#mpirun --allow-run-as-root -n 1 ./fifo $READER /dev/shm/myfifo $MSZ_SIZE $MSZ_COUNT 0 $CHECK &
#pid_r=$!
#sleep 1
#mpirun --allow-run-as-root -n 1 ./fifo $WRITER /dev/shm/myfifo $MSZ_SIZE $MSZ_COUNT 0 $CHECK
#wait $pid_r
#echo "====== END FIFO ======"

#echo "====== BEGIN SHARED ======"
#mpirun --allow-run-as-root -n 1 ./shm $READER $MSZ_SIZE $MSZ_COUNT $CHECK &
#pid_r=$!
#sleep 1
#mpirun --allow-run-as-root -n 1 ./shm $WRITER $MSZ_SIZE $MSZ_COUNT $CHECK
#wait $pid_r
#echo "====== END SHARED ======"

echo "====== BEGIN ADIOS ======"
rm -rf *.bp*
mpirun --allow-run-as-root -n 1 ./adios $READER $MSZ_SIZE $MSZ_COUNT $CHECK &
pid_r=$!
mpirun --allow-run-as-root -n 1 ./adios $WRITER $MSZ_SIZE $MSZ_COUNT $CHECK
wait $pid_r
echo "====== END ADIOS ======"

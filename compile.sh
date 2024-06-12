#!/bin/bash
srun -p compute /usr/bin/bash -c '
source ~/.bashrc &&
module load openmpi/2.1.2 &&
g++ -std=c++11 -O3 demo.cpp -o demo -I /opt/openmpi-2.1.2/include -I/apps/anaconda3/include -L /opt/openmpi-2.1.2/lib -L/apps/anaconda3/lib/ -lssl -lcrypto -l mpi
'

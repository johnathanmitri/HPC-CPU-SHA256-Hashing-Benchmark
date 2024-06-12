#!/bin/bash
#SBATCH --job-name=cs3700_teamProject # Job name
#SBATCH --mem=100000 # Job memory request
#SBATCH --partition=memxl # Use GPU cluster
#SBATCH --nodes=3 # Number of computing nodes
#SBATCH --time=00:02:00 # Time limit HH:MM:SS
#SBATCH --ntasks-per-node=32 # Number of processes on each node

. /etc/profile.d/modules.sh

module load openmpi/2.1.2

/opt/openmpi-2.1.2/bin/mpirun ./demo $1 $2

I used the OpenSSL library for the SHA256 algorithm. However, I unfortunately don't have permissions to install things for myself on the HPC, so I used the command "find / -name sha.h" to search the file system for the library, and found it in /apps/anaconda3/, so this is what we're using. 

In order to compile it, doing "./compile.sh" should work. 

To run it, I first needed to do "export LD_LIBRARY_PATH=/apps/anaconda3/lib/:$LD_LIBRARY_PATH" so that the linker can find the OpenSSL library.

Then to run it, do sbatch run.sh, and give it the file containing the block header, and the minimum number of zero bits you want the hash to have.

For example: "sbatch run.sh blockHeaderApr29.bin 28"

Also modify run.sh to choose a partition and node count that is currently available.


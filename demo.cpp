#include <iostream>
#include "mpi.h"
#include <random>
#include <string>
#include <chrono>
#include <openssl/sha.h>
#include <fstream>
#include <sstream>
#include <iomanip>

SHA256_CTX sha256;

// function to write the nonce to the data block, then calculate the hash and write it to 'hashOut'
void calculateHash(unsigned char* blockPtr, size_t fileSize, unsigned int nonce, unsigned char* hashOut) {
    *(unsigned int*)(&blockPtr[fileSize]) = nonce; // 1 byte past the end of the file, which is start of nonce

    // hash it
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, blockPtr, fileSize + sizeof(nonce));
    SHA256_Final(hashOut, &sha256);
}

// function to just load a binary file from disk, and allocate 4 additional bytes at the end for the nonce
unsigned char* loadBlock(const char* filename, size_t* fileSize) {
    // open the binary file in binary mode
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return nullptr;
    }
    
    *fileSize = file.tellg();
    file.seekg(0, std::ios::beg); // set the position back to the beginning
    
    unsigned char* buffer = new unsigned char[*fileSize + 4]; // add 4 bytes (32bits) for the nonce
    
    file.read(reinterpret_cast<char*>(buffer), *fileSize);
    
    if (!file) {
        std::cerr << "Error reading file: " << filename << std::endl;
        delete[] buffer; 
        return nullptr;
    }
    
    file.close();
    
    return buffer;
}

// given some bytes, return the hex representation as a string
std::string bytesToHexString(unsigned char* bytes, size_t length) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < length; ++i) {
        ss << std::setw(2) << static_cast<int>(bytes[i]);
    }
    return ss.str();
}

// calculates the number of zero bits in a 256 bit hash
int countLeadingZeroBits(unsigned char* hash) {
    int res = 0;

    int i = 0;
    for (; i < SHA256_DIGEST_LENGTH; i++) {
        if (hash[i] == 0) 
            res += 8;  // if the whole byte is zero, we can just add 8 and move on
        else {
            // __builtin_clz uses an x86 instruction that counts the number of leading zeroes, so very good performance
            // it assumes the value is a 32 bit integer, but here it is 8 bit, so subtract 28
            res += (__builtin_clz(hash[i]) - 24); 
            break;
        }
    }
    return res;
}

int main (int argc,  char *argv[]) {
    MPI_Init(&argc, &argv);
    
    int myid;
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);

    int commSize;
    MPI_Comm_size(MPI_COMM_WORLD, &commSize);

    std::random_device rd;
    std::mt19937 gen(rd()); 
    std::uniform_int_distribution<unsigned int> dist(0, std::numeric_limits<unsigned int>::max()); // distribution for the Nonce. Basically just generating 32 completely random bits.

    const char* blockFileName = argv[1];
    int target = std::stoi(argv[2]);

    size_t fileSize;
    unsigned char* blockPtr = loadBlock(blockFileName, &fileSize);

    auto start = std::chrono::steady_clock::now();

    unsigned char hash[SHA256_DIGEST_LENGTH];

    long long i = 0; // keep track of hash count
    int stopFlag = 0;
    while (stopFlag == 0) {
        // Non blocking MPI call to check for any message. We don't send any message except when its time to stop, so we don't care about the contents. Just checking to see if a message is there.
        // If there is a message waiting for us, set stopFlag. This is non blocking, so we'll check it later...
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &stopFlag, MPI_STATUS_IGNORE);

        // Run 100,000 hashes between each MPI call for performance reasons. We don't wanna probe too often so that the hash rate isn't affected.
        long long blockEnd = i + 100000;  
        for (; i < blockEnd; i++) {
            unsigned int nonce = dist(gen); // Select a random Nonce

            calculateHash(blockPtr, fileSize, (unsigned int)nonce, hash); 
            int leadingZeroes = countLeadingZeroBits(hash);
            
            if (leadingZeroes >= target) { // check if this hash satisfies the constraints
                printf("Node #%d found a hash with %d leading zero bits using Nonce = 0x%s after %d tries: 0x%s\n", 
                       myid, leadingZeroes, bytesToHexString((unsigned char*)&nonce, 4).c_str(), i+1, bytesToHexString(hash, SHA256_DIGEST_LENGTH).c_str());
                
                // Notify every other node that we found a solution
                for (int dst = 0; dst < commSize; dst++) {
                    if (dst != myid) 
                    {
                        MPI_Request req;
                        // Send an int value. We don't actually care about this value, we just want Iprobe to find a message.
                        MPI_Isend(&stopFlag, 1, MPI_INT, dst, 0, MPI_COMM_WORLD, &req);
                    }
                }
                // set the stopFlag so we don't loop again
                stopFlag = 1;
                break;
            }
        }
    }
    
    // Record the run time of each node to make sure hash rate is accurate, as some nodes may have started/stopped working at different times than others.
    auto end = std::chrono::steady_clock::now();
    long long milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Get the sum of all the run times
    long long totalMilliseconds;
    MPI_Reduce(&milliseconds, &totalMilliseconds, 1, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);  

    // Get the total number of hashes computed across all nodes
    long long totalHashes;
    MPI_Reduce(&i, &totalHashes, 1, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    if (myid == 0) {
        // get the mean of the run times
        double avgMilliseconds = totalMilliseconds/(double)commSize;
        double avgSeconds = avgMilliseconds / 1000.0;
        
        printf("\nProcess Count: %d\n", commSize);
        printf("Hash input size: %d bytes\n", fileSize+4);
        printf("Runtime in seconds: %f\n", avgSeconds);
        printf("Total hashes: %lld\n", totalHashes);
        
        double hashRate = totalHashes/avgSeconds;
        printf("Total Hash Rate: %fh/s  =  %fMH/s\n", hashRate, hashRate/1000000.0);
    }

    MPI_Finalize();
    return 0;
}

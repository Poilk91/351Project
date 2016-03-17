#include <sys/shm.h>
#include <sys/msg.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "msg.h" /* FOr the message struct */

/* The size of the memory chunk */
#define SHARED_MEMORY_CHUNK_SIZE 1000

int shmid; /* The shared memory ID */
pid_t recvPid; /* The receiver process ID */
void* sharedMemPtr; /* The pointer to the shared memory */
const char* fileName;
FILE* fp;
int sleeper = 1;

void init(int&, void*&);
void mainLoop();
void cleanUp(const int&, void*);
void sendHandler(int);

int main(int argc, char** argv)
{
	/* Check the command line arguments */
	if(argc < 2)
	{
		fprintf(stderr, "USAGE: %s <FILE NAME>\n", argv[0]);
		exit(-1);
	}
	fileName = argv[1];
	/* Connect to shared memory send process ID */
	init(shmid, sharedMemPtr);
	/* Retrieve the reciever's PID and wait for SIGUSR1 */
	/* Also begins sending the data into shared memory */
	mainLoop();
	/* Cleanup */
	cleanUp (shmid, sharedMemPtr);
	return 0;
}

void init(int& shmid, void*& sharedMemPtr)
{
	key_t key;
	/* Use ftok with keyfile.txt to create a unique key */
	if((key = ftok("keyfile.txt", 'a')) == -1)
	{
		perror("ftok");
		exit(-1);
	}
	/* Allocate a shared memory chunk */
	if((shmid = shmget(key, SHARED_MEMORY_CHUNK_SIZE, 0666|IPC_CREAT)) == -1)
	{
		perror("shmget");
		exit(-1);
	}
	/* Retrieve shared memory pointer and attach to shared memory */
	if((sharedMemPtr = shmat(shmid, NULL, 0)) == (char*)-1)
	{
		perror("shmat");
		exit(-1);
	}
}

void mainLoop()
{
	/* Open the file for reading */
	fp = fopen(fileName, "r");
	/* Was the file open? */
	if(!fp)
	{
		perror("fopen");
		exit(-1);
	}
	/* Retrieve the receiver PID from the shared memory*/
	recvPid = *((int*)sharedMemPtr);
	printf("%d\n", recvPid);
	/* Write the sender PID into shared memory */
	*((int*)sharedMemPtr) = getpid();
	printf("%d\n",*((int*)sharedMemPtr));
	/* Send the SIGUSR1 signal to the receiver telling
	him to retrieve our PID */
	kill(recvPid, SIGUSR1);
	/* Wait for SIGUSR1 to tell us to begin reading the file */
	signal(SIGUSR1,sendHandler);
	while(true);
}

void cleanUp(const int& shmid, void* sharedMemPtr)
{
	/* Detach from shared memory */
	shmdt(sharedMemPtr);
	exit(1);
}

void sendHandler(int signum)
{
	int size;
	/* Read the entire file */
	if(!feof(fp))
	{
		if((size = fread((char*)sharedMemPtr+4, sizeof(char), SHARED_MEMORY_CHUNK_SIZE-4, fp)) < 0)
		{
			perror("fread");
			exit(-1);
		}
		/* Write the size into the first four bytes of the shared memory */
		*((int*)sharedMemPtr) = size;
		/* Let the receiver know that it can start reading */
		kill(recvPid, SIGUSR1);
	}
	/* Close the file */
	else
	{
		fclose(fp);
		cleanup(shmid, sharedMemPtr);
	}
}

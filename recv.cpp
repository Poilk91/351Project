#include <sys/shm.h>
#include <sys/msg.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "msg.h"

/* The size of the shared memory block */
#define SHARED_MEMORY_CHUNK_SIZE 1000

/* The shared memory pointer id */
int shmid;
int sleeper = 1;
/* The PID of the sender */
pid_t sendPID;
/* The pointer to the shared memory */
void* sharedMemPtr;
/* The name of the output file */
const char recvFileName[] = "recvfile";

/* Function Prototypes */
void init(int&, void*&);
void retrievePID();
void retrieveData();
void mainLoop();
void cleanUp(const int&, void*);
void ctrlCSignal(int);
void retrieveHandler(int);
void retrievePIDHandler(int);

int main(int argc, char** argv)
{
	/* Set up the ctrl-C signal listener */
	signal(SIGINT, ctrlCSignal);
	/* Initialize */
	init(shmid, sharedMemPtr);
	/* Go to main loop */
	mainLoop();
	/* Detach from shared memory segment and deallocate shared memory */
	cleanUp(shmid, sharedMemPtr);
	return 0;
}

void init(int& shmid, void*& sharedMemPtr)
{
	/* Create a key with ftok */
	key_t key;
	if((key = ftok("keyfile.txt",'a')) == -1)
	{
		perror("ftok");
		exit(-1);
	}
	/* Generate a shared memory ID and memeroy block */
	if((shmid = shmget(key, SHARED_MEMORY_CHUNK_SIZE, 0666|IPC_CREAT)) == -1)
	{
		perror("shmget");
		exit(-1);
	}
	/* Generate the shared memory pointer */
	if((sharedMemPtr = shmat(shmid, NULL, 0)) == (char*)-1)
	{
		perror("shmat");
		exit(-1);
	}
}

void retrievePID()
{
	/* Retrieve sender's PID */
	sendPID = *((int*)sharedMemPtr);
	printf("%d\n",sendPID);
	/* Send SIGUSR1 signal to sender telling him
	to start sending data*/
	kill(sendPID, SIGUSR1);
	/* Wait for SIGUSR1 to upload data
	 then retrieve data */
	signal(SIGUSR1, retrieveHandler);
}

void retrieveHandler(int signum)
{
	retrieveData();
}

void raiseHandler(int signum)
{
	printf("WAKE UP!\n");	
	sleeper = 0;
}

void retrieveData()
{
	printf("enter data\n");
	/* Open file for writing */
	FILE* fp = fopen(recvFileName, "w");
	/* Error checks */
	if(!fp)
	{
		perror("fopen");
		exit(-1);
	}
	do
	{
		/* Retrieve the message size from shared memory location */
		int msgSize = *((int*)sharedMemPtr);
		printf("%d\n", msgSize);
		/* Write message to file if the size is greater than 0 */
		if(msgSize >0)
		{
			if(fwrite((char*)sharedMemPtr+4, sizeof(char), msgSize, fp) < 0)
			{
				perror("fwrite");
				exit(-1);
			}
			/* Send signal to show that data has been read */
			kill(sendPID, SIGUSR1);
			/* Wait for wakeup signal to keep reading from memory */
			signal(SIGUSR1, raiseHandler);
			/* Pause the writing */
			while(sleeper);
			sleeper = 1;
			printf("continued\n");
		}
		else
			fclose(fp);
	}while(true);
}

void retrievePIDHandler(int signum)
{
	retrievePID();
}

void mainLoop()
{
	/* Place Receiver PID into shared memory location */
	*((int*)sharedMemPtr) = getpid();
	printf("%d\n", *((int*)sharedMemPtr));
	/*Wait for SIGUSR1 telling us that sender has
	out PID then retrieve the sender PID */
	signal(SIGUSR1, retrievePIDHandler);
	while(true);
}

void cleanUp(const int& shmid, void* sharedMemPtr)
{
	/* Detach from shared memory */
	shmdt(sharedMemPtr);
	/* Deallocate the shared memory chunk */
	shmctl(shmid, IPC_RMID, NULL);
	exit(1);
}

void ctrlCSignal(int signal)
{
	cleanUp(shmid, sharedMemPtr);
}

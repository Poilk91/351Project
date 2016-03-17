#include <sys/shm.h>
#include <sys/msg.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "msg.h"    /* For the message struct */


/* The size of the shared memory chunk */
#define SHARED_MEMORY_CHUNK_SIZE 1000

/* The ids for the shared memory segment */
int shmid, sendPID;

/* The pointer to the shared memory */
void *sharedMemPtr;

/* The name of the received file */
const char recvFileName[] = "recvfile";


/**
 * Sets up the shared memory segment
 * @param shmid - the id of the allocated shared memory
 * @param msqid - the id of the shared memory
 * @param sharedMemPtr - the pointer to the shared memory
 */

void init(int& shmid, void*& sharedMemPtr)
{
	key_t key;
	if((key = ftok("keyfile.txt",'a')) == -1)//Use ftok("keyfile.txt", 'a') in order to generate the key.
	{
		perror("ftok");
		exit(1);
	}

	if((shmid = shmget(key, SHARED_MEMORY_CHUNK_SIZE, 0666)) < 0)//allocate shared memory location
	{
		perror("shmget");
		exit(1);
	}

	if((sharedMemPtr = shmat(shmid, NULL, 0)) == (char *)-1)//attach shared mem ptr to shared memory location
	{
		perror("shmat");
		exit(1);
	}
}

void retrieveData()
{
	/* Open file for writing */
	FILE* fp = fopen(recvFileName, "w");
	/* Error checks */
	if(!fp)
	{
		perror("fopen");
		exit(-1);
	}
	/*Begin listning for a wakeup signal*/
	signal(SIGUSR1,SIGCONT);
	
	do
	{
		/*Retrieve the message size from shared memory location*/
		int msgSize = *((int*)sharedMemPtr);
		/*write message to file if the size is greater than 0*/
		if(msgSize > 0)
		{
			if(fwrite(sharedMemPtr+4, sizeof(char), msgSize, fp) < 0){
					perror("fwrite");
					exit(-1);
			}
		}
		/*If message size is less than 0 close file and cleanup*/
		else
		{
			fclose(fp);
			cleanUp();
		}
		/*Send signal to show i have read data*/
		kill (sendPID, SIGUSR1);
		/*Wait for wakeup signal telling me there is more data to read*/
		pause();
	}while(true);
}

void retrievePID()
{
	/*Retrieve sender's PID*/
	sendPID = *((int*)sharedMemPtr);
	/*Send SIGUSR1 signal to sender*/
	kill (sendPID, SIGUSR1);
	/*Wait for SIGUSR1 then retreive data */
	signal (SIGUSR1, retrieveData());
}

/**
 * The main loop
 */
void mainLoop()
{
	/*Place Receiver PID into shared memory location*/
	*((int*)(sharedMemPtr)) = getpid();
	/*Wait for SIGUSR1 then retreive PID */
	signal(SIGUSR1, retrievePID());
	
	while(true);
}
/**
 * Perfoms the cleanup functions
 * @param sharedMemPtr - the pointer to the shared memory
 * @param shmid - the id of the shared memory segment
 */

void cleanUp(const int& shmid, void* sharedMemPtr)
{
	/* Detach from shared memory */
	shmdt(sharedMemPtr);
	/* Deallocate the shared memory chunk */
	shmctl(shmid, IPC_RMID, NULL);
	exit();
}

/**
 * Handles the exit signal
 * @param signal - the signal type
 */

void ctrlCSignal(int signal)
{
	/* Free system V resources */
	cleanUp(shmid, sharedMemPtr);
}

int main(int argc, char** argv)
{
	/* set up ctrlC signal listener */
	signal(SIGINT,ctrlCSignal);
	/* Initialize */
	init(shmid, sharedMemPtr);
	/* Go to the main loop */
	mainLoop();
	/* Detach from shared memory segment, and deallocate shared memory(i.e. call cleanup) */
	cleanUp(shmid, sharedMemPtr);
	return 0;
}

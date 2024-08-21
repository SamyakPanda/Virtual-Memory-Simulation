#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <string.h>
#include <time.h>



int len = 0;
int pages[100];

// structure for message queue 3
typedef struct MQ3_sendbuffer{		
	long mtype;         	
	int id;
	int pageno;
}MQ3_sendbuffer;

// structure for message queue 3 recvbuffer
typedef struct MQ3_recvbuffer{			
	long mtype;          
	int frameno;
}MQ3_recvbuffer;

struct MQ1buffer{
	long mtype;         
	int id;
};




int main(int argc, char *argv[]){
	if (argc<4){
		perror("Invalid Number of Arguments\n");
		exit(1);
	}

	int pid,MQ1, MQ3;
	int i;
	pid = atoi(argv[1]);			
	MQ1 = atoi(argv[2]);
	MQ3 = atoi(argv[3]);

	// Convert reference string to pages
	char *token;
	token=strtok(argv[4],"  ");
	while(token!=NULL){
		pages[len++] = atoi(token); 
		token=strtok(NULL,"  ");
	}
	
	printf("Process id = %d\n",pid);


	// Send message to scheduler
	struct MQ1buffer msg_to_scheduler;
	msg_to_scheduler.mtype= 10;
	msg_to_scheduler.id=pid;
	int length=sizeof(struct MQ1buffer)-sizeof(long);
	if (msgsnd(MQ1,&msg_to_scheduler,length,0)==-1){
		perror("Error in sending message to scheduler");
		exit(1);
	}
	
	// Receive message from scheduler
	struct MQ1buffer msg_from_scheduler;						
	length=sizeof(struct MQ1buffer)-sizeof(long);

	if (msgrcv(MQ1,&msg_from_scheduler,length,20+pid,0)==-1){
		perror("Error in receiving message");
		exit(1);
	}

	// Send request to MMU
	MQ3_sendbuffer msg_to_mmu;
	MQ3_recvbuffer msg_from_mmu;

	for(i=0;i<len;i++){
		printf("Sending request for Page %d.\n",pages[i]);
		msg_to_mmu.mtype=10;
		msg_to_mmu.id=pid;
		msg_to_mmu.pageno=pages[i];
		length=sizeof(struct MQ3_sendbuffer)-sizeof(long);
		// Send message to MMU
		if(msgsnd(MQ3,&msg_to_mmu,length,0)==-1){
				perror("Error in sending message");
				exit(1);
			}

		length=sizeof(struct MQ3_recvbuffer)-sizeof(long);
		// Receive message from MMU
		if(msgrcv(MQ3,&msg_from_mmu,length,20+pid,0)==-1){
			perror("Error in receiving message");
			exit(1);
		}

		// Check if the frame number is received
		if(msg_from_mmu.frameno>=0){
			printf("MMU responded with frame number for process %d: %d\n",pid,msg_from_mmu.frameno);
			i++;
		}
		// Check if page fault is detected
		else if(msg_from_mmu.frameno== -1){
			printf("Page Fault detected for process %d",pid);
			length=sizeof(struct MQ1buffer)-sizeof(long);

			if (msgrcv(MQ1,&msg_from_scheduler,length,20+pid,0)==-1)
			{
				perror("Error in receiving message");
				exit(1);
			}
		}
		// Check if invalid page reference is detected
		else if (msg_from_mmu.frameno== -2){
			printf("Invalid Page Reference for Process %d. Terminating the Process...\n",pid);
			exit(1);
		}
	}
	
	printf("Process %d completed succcesfully\n",pid);
	msg_to_mmu.pageno=-9;
	msg_to_mmu.id=pid;
	msg_to_mmu.mtype=10;
	length=sizeof(struct MQ3_sendbuffer)-sizeof(long);
	if (msgsnd(MQ3,&msg_to_mmu,length,0)==-1)	
		{
			perror("Error in sending message");
			exit(1);
		}
	return 0;
}


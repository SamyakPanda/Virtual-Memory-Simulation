#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>


struct MQ2buffer{
	long mtype;
	char mbuffer[1];
};

struct MQ1buffer{
	long mtype;
	int id;
};

int k; 

int main(int argc , char * argv[])
{
	if (argc<5)
	{
		printf("Invalid Number of Arguments\n");
		printf("Usage: %s <MQ1> <MQ2> <k> <master_pid>\n", argv[0]);
		exit(1);
	}

	int MQ1,MQ2,master_pid,length,curr_pid;
	MQ1=atoi(argv[1]);	//get the id of the message queues MQ1 and MQ2
	MQ2=atoi(argv[2]);
	k=atoi(argv[3]);
	master_pid=atoi(argv[4]);

	struct MQ1buffer msg_to_process,msg_from_process;
	struct MQ2buffer msg_from_mmu;

	int n_terminated=0; 

	while (1)
	{
		length=sizeof(struct MQ1buffer)-sizeof(long);
		if(msgrcv(MQ1,&msg_from_process,length,10,0)==-1)	//select the 1st process from the ready queue
		{
			perror("Error in receiving message");
			exit(1);
		}

		curr_pid=msg_from_process.id;

		msg_to_process.mtype=20+curr_pid;
		msg_to_process.id=curr_pid;

		length=sizeof(struct MQ1buffer)-sizeof(long);
		if (msgsnd(MQ1,&msg_to_process,length,0)==-1)	//send message to the selected process to start execution
		{
			perror("Error in sending message");
			exit(1);
		}

		length=sizeof(struct MQ2buffer)-sizeof(long);
		if(msgrcv(MQ2,&msg_from_mmu,length,0,0)==-1)	//receive message from MMU
		{
			perror("Error in receiving message");
			exit(1);
		}
		if (msg_from_mmu.mtype==1)
		{
			//if message type if 1, then add the current process to the end of ready queue
			msg_from_process.mtype=10;
			msg_from_process.id=curr_pid;
			length=sizeof(struct MQ1buffer)-sizeof(long);
			if (msgsnd(MQ1,&msg_from_process,length,0)==-1)
			{
				perror("Error in sending message");
				exit(1);
			}
		}
		else if(msg_from_mmu.mtype==2)
		{	
			n_terminated++;	//increment the number of processes TERMINATE
		}
		else
		{
			printf("Incorrect Message Received\n");
			exit(1);
		}
		if (n_terminated==k) break; //if all processes have TERMINATE, then break
	}
	//send signal to Master to terminate all the modules
	kill(master_pid,SIGUSR1);	
	pause();	
	printf("Terminating Scheduler\n");
	exit(1);
}
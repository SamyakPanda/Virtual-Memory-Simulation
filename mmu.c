#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <limits.h>
#include <sys/shm.h>
#include <stdbool.h>
#include <fcntl.h>
#include <string.h>

				
				

int timestamp=0;					
int fault_freq[10000];			
int fault_freq_index=0;			
int outfile;

typedef struct{						
	int frame;
	bool valid;
	int time;
}PTentry;

typedef struct{						
	int pid;
	int m;
	int allocount;
	int usecount;
}process;

typedef struct{						
	int size;
	int ffl[];
}FFL;

//Message Queue Structures

typedef struct {					
	long mtype;
	int id;
	int pageno;
}MQ3_recvbuf;

typedef struct {					
	long mtype;
	int frameno;
}MQ3_sendbuf;

typedef struct {	
	long mtype;
	char mbuf[1];
}MQ2buf;

int SM1, SM2;		
int MQ2, MQ3;
int PCBid;

process *PCB;		
PTentry *PT;
FFL *freeFL;

int m,k;






void sendFrameNo(int id, int frame){
	MQ3_sendbuf msg_to_process;
	int length;

	msg_to_process.mtype=20+id;		//msg to process
	msg_to_process.frameno=frame;
	length=sizeof(MQ3_sendbuf)-sizeof(long);

	if(msgsnd(MQ3,&msg_to_process,length,0)==-1)	//send frame number
	{
		perror("Error in sending message");
		exit(1);
	}
}


int handlePageFault(int id,int pageno){
	int i,frameno;
	if(freeFL->size==0||PCB[id].usecount>PCB[id].allocount){
		int min=INT_MAX,mini=-1;			
		for(i=0;i<PCB[id].m;i++){
			if(PT[id*m+i].valid==true){
				if(PT[id*m+i].time<min){
					min=PT[id*m+i].time;	//minimum timestamp is found
					mini = i;
				}
			}
		}
		PT[id*m+mini].valid=false;			//that page table entry is made invalid
		frameno=PT[id*m+mini].frame;		//corresponding frame is returned 
	}

	else{
		frameno=freeFL->ffl[freeFL->size-1];		//otherwise get a free frame and allot it to the corresponding process
		freeFL->size-=1;
		PCB[id].usecount++;
	}
	return frameno;
}


void Msg_scheduler(int type){
	MQ2buf msg_to_scheduler;
	int length;

	msg_to_scheduler.mtype=type;
	length=sizeof(MQ2buf)-sizeof(long);

	if(msgsnd(MQ2,&msg_to_scheduler,length,0)==-1)	//send the msg
	{
		perror("Error in sending message");
		exit(1);
	}
}



void serviceMessageRequest(){
	int id,pageno,length,frameno,i,found;
	int mintime,mini;
	MQ3_recvbuf msg_from_process;
	MQ3_sendbuf msg_to_process;
	length=sizeof(MQ3_recvbuf)-sizeof(long);
	if(msgrcv(MQ3,&msg_from_process,length,10,0)==-1){
		perror("Error in receiving message");
		exit(1);
	}
	id=msg_from_process.id;
	pageno=msg_from_process.pageno;				//Retrieve the process id and page number requested

	if (pageno== -9){
		//When a process is over/terminated, free all the frames allotted to it
		int i= 0;
		for(i= 0;i<PCB[i].m;i++){
			if(PT[id*m+i].valid==true){
				freeFL->ffl[freeFL->size]=PT[id*m+i].frame;		//add the frame to FFL
				freeFL->size += 1;								//increase the size 
			}
		}
		Msg_scheduler(2);
		return;
	}

	timestamp++;								//Increase the timestamp

	printf("Page reference: (%d, %d, %d)\n",timestamp,id,pageno);
	char temp[100];
	memset(temp,0,sizeof(temp));
	sprintf(temp,"Page reference: (%d, %d, %d)\n",timestamp,id,pageno);
	write(outfile,temp,strlen(temp));

	if (pageno>PCB[id].m||pageno<0){
		printf("Invalid Page Reference: (%d, %d)\n",id,pageno);
		char buffer[100];
		memset(buffer,0,sizeof(buffer));
		sprintf(buffer,"Invalid Page Reference: (%d, %d)\n",id,pageno);
		write(outfile,buffer,strlen(buffer));
		
		sendFrameNo(id, -2);	
		

		int i= 0;
		for(i= 0;i<PCB[i].m;i++){
			if(PT[id*m+i].valid==true){
				freeFL->ffl[freeFL->size]=PT[id*m+i].frame;		
				freeFL->size += 1;								
			}
		}
		Msg_scheduler(2);
	}

	else{
		if(PT[id*m+pageno].valid==true){
			frameno=PT[id*m+pageno].frame;
			sendFrameNo(id,frameno);
			PT[id*m+pageno].time=timestamp;
		}
		else{
			printf("Page Fault: (%d, %d)\n",id,pageno);
			char buffer[100];
			memset(buffer,0,sizeof(buffer));
			sprintf(buffer,"Page Fault: (%d, %d)\n",id,pageno);
			write(outfile,buffer,strlen(buffer));
			fault_freq[id]+=1;
			sendFrameNo(id,-1);			
			frameno=handlePageFault(id,pageno);
			PT[id*m+pageno].valid=true;
			PT[id*m+pageno].time=timestamp;
			PT[id*m+pageno].frame=frameno;
			Msg_scheduler(1);		
		}
	}	
}

void complete(int signo){
	int i;
	if(signo==SIGUSR2){

		printf("Frequency of Page Faults for Each Process:\n");	
		char buf[100];
		memset(buf,0,sizeof(buf));
		sprintf(buf,"Frequency of Page Faults for Each Process:\n");
		write(outfile,buf,strlen(buf));
		printf("PID\tFrequency\n");
		memset(buf,0,sizeof(buf));
		sprintf(buf,"PID\tFrequency\n");
		write(outfile,buf,strlen(buf));
		for(i=0;i<k;i++){
			printf("%d\t%d\n",i,fault_freq[i]);
			memset(buf,0,sizeof(buf));
			sprintf(buf,"%d\t%d\n",i,fault_freq[i]);
			write(outfile,buf,strlen(buf));
		}

		shmdt(PCB);					
		shmdt(PT);
		shmdt(freeFL);
		close(outfile);			//close the file
		exit(0); 
	}
}

int main(int argc, char const *argv[]){
	signal(SIGUSR2,complete);					
	sleep(1);									
	if (argc<8){
		perror("Invalid Number of Arguments\n");
		exit(1);
	}

	MQ2=atoi(argv[1]);						//Get various ids and other parameters
	MQ3=atoi(argv[2]);
	SM1=atoi(argv[3]);
	SM2=atoi(argv[4]);
	PCBid=atoi(argv[5]);
	m=atoi(argv[6]);
	k=atoi(argv[7]);

	int i;

	for(i=0;i<k;i++) fault_freq[i]=0;	
	PCB=(process *)(shmat(PCBid,NULL,0));		
	PT=(PTentry *)(shmat(SM1,NULL,0));
	freeFL=(FFL *)(shmat(SM2,NULL,0));

	outfile = open("result.txt",O_CREAT|O_WRONLY|O_TRUNC,0666);		//Open the output file
	while(1){
		serviceMessageRequest();				
	}
	return 0;
}
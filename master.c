#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>


// structure for page table entry
typedef struct{
	bool valid;
	int frame;
	int time; // time of last access of the page no
}PTentry;

// structure for free frame lis
typedef struct{
	int size;  
	int ffl[]; 
}FFL;

// structure for process control block
typedef struct{
	int pid;
	int m;		   
	int allocount;
	int usecount;
}process;


// k = number of processes, m = maximum number of pages required per process, f = total number of frames
int k, m, f;	
int key;

int SM1, SM2;		// shared memory segments
int MQ1, MQ2, MQ3;	
int PCB;			// id for PCB block
int pid_master, pid_scheduler, pid_mmu;

// Signal Handlers
void ctl_shared(int i){
	
	// remove shared memory and message queues

	if (shmctl(SM1, IPC_RMID, NULL) == -1){
		perror("PageTable Shared Memory Error");
	}
	if (shmctl(SM2, IPC_RMID, NULL) == -1){
		perror("FreeFrameList Shared Memory Error");
	}
	if (msgctl(MQ1, IPC_RMID, NULL) == -1){
		perror("MQ1 Error");
	}
	if (msgctl(MQ2, IPC_RMID, NULL) == -1){
		perror("MQ2 Error");
	}
	if (msgctl(MQ3, IPC_RMID, NULL) == -1){
		perror("MQ3 Error");
	}
	exit(i);
}

// kill scheduler and mmu
void kill_process(int signo){
	sleep(1);
	if (signo == SIGUSR1){
		kill(pid_scheduler, SIGTERM); 
		kill(pid_mmu, SIGUSR2);		  
		sleep(1);
		ctl_shared(0);
	}
}


// create processes
void create_processes(){
	// create processes and call the process
	process *ptr = (process *)(shmat(PCB, NULL, 0));

	int *a = (int *)ptr;
	if (*a == -1){
		perror("Shared Memory Attach Error: PCB");
		ctl_shared(1);
	}

	int i, j, rand_seed;
	int rlen;
	int generated;
	int n, r;
	char ref[100000];
	memset(ref, 0, sizeof(ref));
	int index = 0;
	for (i = 0; i < k; i++){
		rand_seed = time(NULL);
		rlen = rand() % (8 * ptr[i].m + 1) + 2 * ptr[i].m;
		generated = 0;

		while (generated != rlen){
			srand(generated);
			n = rand() % (rlen / 3 + 1) + 1;

			if (generated + n <= rlen){
				if (rand() % 2 == 0)
					srand(rand_seed);
				for (j = 0; j < n; j++){
					r = rand() % ptr[i].m + rand() % n;
					char buffer[10];
					sprintf(buffer, "%d", r);
					strcat(ref, buffer);
					strcat(ref, "  ");
				}
				generated += n;
			}

			else{
				srand(rand_seed);
				for (j = 0; j < rlen - generated; j++){
					r = rand() % ptr[i].m + rand() % (n * 2);
					char buffer[10];
					sprintf(buffer, "%d", r);
					strcat(ref, buffer);
					strcat(ref, "  ");
				}
				generated = rlen;
			}
		}

		printf("Reference String: %s\n", ref);
		// fork and create process using execlp
		if (fork() == 0){
			char PNo[20], M1[20], M3[20];
			sprintf(PNo, "%d", i);
			sprintf(M1, "%d", MQ1);
			sprintf(M3, "%d", MQ3);
			execlp("./process", "./process", PNo, M1, M3, (char *)&ref[0], (char *)(NULL)); // call the process
			exit(0);
		}

		usleep(250 * 1000);
	}
}



void ProcessBlocks(){
	// create process control blocks to store the number of pages, number of frames allocated and used for each process
	int i;
	key = rand();
	PCB = shmget(key, sizeof(process) * k, 0666 | IPC_CREAT | IPC_EXCL);
	if (PCB == -1){
		perror("Process Block Create Error");
		ctl_shared(1);
	}

	process *ptr = (process *)(shmat(PCB, NULL, 0));
	int *a = (int *)ptr;
	if (*a == -1){
		perror("Shared Memory Attach Error: PCB");
		ctl_shared(1);
	}

	int totalpages = 0;
	for (i = 0; i < k; i++){
		ptr[i].pid = i;
		ptr[i].m = rand() % m + 1;
		totalpages += ptr[i].m;
		ptr[i].usecount = 0;
	}

	int total_allo = 0; 
	int max = 0, maxi = 0;
	int allo;
	for (i = 0; i < k; i++){
		
		allo = 1 + (int)(ptr[i].m * (f - k) / (float)totalpages);
		ptr[i].allocount = allo;
		total_allo = total_allo + allo;
	}

	int remain_allo = f - total_allo;
	
	while (remain_allo > 0){
		ptr[rand() % k].allocount += 1;
		remain_allo--;
	}

	for (i = 0; i < k; i++){
		printf("Allocated Frames: %d,Size of Page Table: %d, Allocated No. of frames: %d\n", ptr[i].allocount, ptr[i].m, ptr[i].allocount);
	}

	if (shmdt(ptr) == -1){
		perror("Shared Memory Detach Error: PCB");
		ctl_shared(1);
	}
}






int main(int argc, char const *argv[]){

	srand(time(NULL));
	signal(SIGUSR1, kill_process);
	signal(SIGINT, ctl_shared);
	if (argc < 4){
		printf("Error: usage %s <k> <m> <f>\n", argv[0]);
		ctl_shared(1);
	}

	k = atoi(argv[1]); 
	m = atoi(argv[2]); //Maximum number of pages required per process (m)
	f = atoi(argv[3]); //Total number of frames (f)

	pid_master = getpid();

	if (k <= 0 || m <= 0 || f <= 0 || f < k){
		printf("Inut is invalid\n");
		ctl_shared(1);
	}

	// createPT(); 
	// create page tables
	int i;
	key = rand();
	SM1 = shmget(key, m * k * sizeof(PTentry), 0666 | IPC_CREAT | IPC_EXCL);
	if (SM1 == -1){
		perror("PT Shared Memory Error");
		ctl_shared(1);
	}

	PTentry *pt = (PTentry *)(shmat(SM1, NULL, 0));
	int *a = (int *)pt;
	if (*a == -1){
		perror("PT Shared Memory Attach Error");
		ctl_shared(1);
	}
	// initialize the frame no to -1 and valid to false
	for (i = 0; i < k * m; i++){
		pt[i].frame = -1;
		pt[i].valid = false;
	}

	if (shmdt(pt) == -1){
		perror("PT Shared Memory Detach Error");
		ctl_shared(1);
	}

	
	int *b;
	FFL *ptr;
	key = rand();
	SM2 = shmget(key, sizeof(FFL) + f * sizeof(int), 0666 | IPC_CREAT | IPC_EXCL); // create FFL
	if (SM2 == -1){
		perror("FFL Shared Memory Error");
		ctl_shared(1);
	}

	ptr = (FFL *)(shmat(SM2, NULL, 0));
	b = (int *)ptr;
	if (*b == -1){
		perror("FFL Shared Memory Attach Error");
		ctl_shared(1);
	}

	for (i = 0; i < f; i++){
		ptr->ffl[i] = i; 
	}

	ptr->size = f; 

	if (shmdt(ptr) == -1){
		perror("FFL Shared Memory Detach Error");
		ctl_shared(1);
	}


	ProcessBlocks();


	// create message queues using MQ1,MQ2,MQ3
	key = rand();
	MQ1 = msgget(key, 0666 | IPC_CREAT | IPC_EXCL); 
	if (MQ1 == -1){
		perror("MQ1 Create Error");
		ctl_shared(1);
	}

	key = rand();
	MQ2 = msgget(key, 0666 | IPC_CREAT | IPC_EXCL);
	if (MQ2 == -1){
		perror("MQ2 Create Error");
		ctl_shared(1);
	}

	key = rand();
	MQ3 = msgget(key, 0666 | IPC_CREAT | IPC_EXCL);
	if (MQ3 == -1){
		perror("MQ3 Create Error");
		ctl_shared(1);
	}


	// create scheduler and MMU
	pid_scheduler = fork();
	if (pid_scheduler == 0){
		char M1[20], M2[20], N[20], PID[20];
		sprintf(M1, "%d", MQ1);
		sprintf(M2, "%d", MQ2);
		sprintf(N, "%d", k);
		sprintf(PID, "%d", pid_master);
		execlp("./scheduler", "./scheduler", M1, M2, N, PID, (char *)(NULL)); // create scheduler
		exit(0);
	}

	pid_mmu = fork();
	if (pid_mmu == 0){
		char buffer1[20], buffer2[20], buffer3[20], buffer4[20], buffer5[20], buffer6[20], buffer7[20];
		sprintf(buffer1, "%d", MQ2);
		sprintf(buffer2, "%d", MQ3);
		sprintf(buffer3, "%d", SM1);
		sprintf(buffer4, "%d", SM2);
		sprintf(buffer5, "%d", PCB);
		sprintf(buffer6, "%d", m);
		sprintf(buffer7, "%d", k);


		execlp("xterm", "xterm", "-T", "Memory Management Unit", "-e", "./mmu", buffer1, buffer2, buffer3, buffer4, buffer5, buffer6, buffer7, (char *)(NULL)); // create MMU
		exit(0);
	}

	create_processes();

	pause(); // wait till scheduler notifies completion of all processes

	ctl_shared(0);
	return 0;
}

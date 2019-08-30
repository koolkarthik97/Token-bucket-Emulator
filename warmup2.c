#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<string.h>
#include<unistd.h>
#include<sys/time.h>
#include<math.h>
#include<signal.h>

#include "cs402.h"
#include "my402list.h"

pthread_t packetThread,tokenThread,s1Thread,s2Thread,controlThread;
pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
sigset_t set;
int terminate=0,removed_pkts=0,t_dropped=0,t_count=1,pkts_read=0,removed_pkts_Q1=0,removed_pkts_Q2=0;

long double emulation_end=0;
struct timeval startTime;
int noOfPkts=0;
char *filename;
long double token_drop_probability=0,packet_drop_probability=0;
int mode=0; //0 - deterministic and 1- trace file
typedef struct shared_data_structures{
	My402List q1;
	My402List q2;
	int token_count;
	double lambda,mu,r,b;
	int p,num;
	FILE* fp;
	long double total_interarrival_time,total_service_time,time_Q1,time_Q2,time_S1,time_S2,time_S,time_SS;
	int droppedPkts;
}sharedDS;

typedef struct packet_structure_from_file{
	int id;
	int inter_arrival_time;
	int tokens_needed;
	int service_time;
	long double arrival_time;
	long double Q2_entry_time;
	long double service_start;
}Packet;

//function declarations
void createThreads(sharedDS* ds);
void syncThreadsBeforeExit();	
void initializeVals(sharedDS* ds);
/***************************************************/

Packet* split_string(char *line) {
    const char delimiter[] = " ";
    char *tmp;
	Packet *packet=(Packet*)malloc(sizeof(Packet));
    tmp = strtok(line, delimiter);
    if (tmp == NULL)
    	return NULL;
    packet->inter_arrival_time=atoi(tmp);
	int count=0;
    for (;;) {
        tmp = strtok(NULL, delimiter);
        if (tmp == NULL)
            break;
        else if(count==0)
			packet->tokens_needed=atoi(tmp);
		else if(count==1)
			packet->service_time=atoi(tmp);
		count++;
    }
	return packet;
}
//call in trace mode ONLY !!!!
int getNumberOfPacketsFromFile(sharedDS* ds){
	char *line = NULL;
    size_t size;

	//get num
    if (getline(&line, &size, ds->fp) != -1) {
    const char delimiter[] = " ";
    char *tmp;
    tmp = strtok(line, delimiter);
    ds->num=atoi(tmp);
	if(atoi(tmp)==0){
		fprintf(stderr, "Line 1 is not a number. Invalid file\n");
	 	exit(1);	
	}
	noOfPkts=ds->num;
    }else{
		return -1;
	}
    free(line);
	return 1;
}
//
long double getTimestamp(){
	struct timeval currTime;
	gettimeofday(&currTime,NULL);
	struct timeval diff;
	timersub(&currTime, &startTime, &diff);
	return (diff.tv_sec*1000)+(diff.tv_usec*0.001);
}


//move from Q1 to Q2 if possible
void moveFromQ1toQ2(sharedDS* ds){
	if(My402ListLength(&(ds->q1))==0)
		return;
	My402ListElem *elem=My402ListFirst(&(ds->q1));
	Packet* packet=(Packet*)elem->obj;
	if(packet->tokens_needed<=ds->token_count){
		ds->token_count-=packet->tokens_needed;
		long double d=getTimestamp();
		ds->time_Q1+=d-packet->arrival_time;
		printf("%012.3Lfms: ",d);
		printf("p%d leaves Q1, time in Q1 = %.3Lfms, token bucket now has %d token\n",packet->id,d-packet->arrival_time,ds->token_count);
		My402ListUnlink(&(ds->q1),elem);
		My402ListAppend(&(ds->q2),packet);
		//print packet enters q2
		d=getTimestamp();
		printf("%012.3Lfms: ",d);
		printf("p%d enters Q2\n",packet->id);
		packet->Q2_entry_time=getTimestamp();
		if(My402ListLength(&(ds->q2))==1)
			pthread_cond_broadcast(&cv);
	}
}


//call for packet thread
void *packetThreadProcedureCall(void *arg){
	//casting
	sharedDS* ds=(sharedDS*)arg;
	pkts_read=0;
	struct timeval prevPktArrival;
	prevPktArrival.tv_sec=startTime.tv_sec;
	prevPktArrival.tv_usec=startTime.tv_usec;
	while(ds->num>0){
	//read one packet
		Packet *packet=NULL;
		if(mode==1)
		{
			char *line = NULL;
			size_t size;
			if(getline(&line, &size, ds->fp) != -1) 
			{
				if(strlen(line)>1024){
					fprintf(stderr, "File not in specified format.\n");
					exit(1);	
				}
				pkts_read++;
				packet=split_string(line);
				packet->id=pkts_read;
				packet->arrival_time=getTimestamp();
			}
		//sleep for time inter-arr of that pkt
		free(line);
		}
		else{
			//pthread_cleanup_push(free, packet);
		//WRITE CODE FOR IF DETERMINISTIC HERE !!!!
			packet=(Packet*)malloc(sizeof(Packet));
			pkts_read++;
			packet->id=pkts_read;
			packet->inter_arrival_time=(double)(1/ds->lambda)*1000;
			if(packet->inter_arrival_time > 10000)
				packet->inter_arrival_time=10000;
			packet->tokens_needed=ds->p;
			
			packet->service_time=(double)(1/ds->mu)*1000;
			if(packet->service_time > 10000)
				packet->service_time=10000;
			packet->arrival_time=getTimestamp();
			//pthread_cleanup_pop(0);
			
		}
		
		usleep(1000*packet->inter_arrival_time);
		
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
		//print ms timestamp
		struct timeval currTime;
		gettimeofday(&currTime,NULL);
		struct timeval diff;
		timersub(&currTime, &startTime, &diff);
		pthread_mutex_lock(&mutex);
		long double d=(diff.tv_sec*1000)+(diff.tv_usec*0.001);
		printf("%012.3Lfms: ",d);
		//calculate inter-arrival time
		timersub(&currTime,&prevPktArrival,&diff);
		d=(diff.tv_sec*1000)+(diff.tv_usec*0.001);
		prevPktArrival.tv_sec=currTime.tv_sec;
		prevPktArrival.tv_usec=currTime.tv_usec;
		ds->num--;
		ds->total_interarrival_time+=d;
		if(packet->tokens_needed>ds->b){
			printf("p%d arrives, needs %d tokens, inter-arrival time = %.3Lfms, dropped\n",pkts_read,packet->tokens_needed,d);
			ds->droppedPkts+=1;
		}
		else{
			printf("p%d arrives, needs %d tokens, inter-arrival time = %.3Lfms\n",pkts_read,packet->tokens_needed,d);
			//critical section code
			(void)My402ListAppend(&(ds->q1),packet);
			d=getTimestamp();
			printf("%012.3Lfms: ",d);
			printf("p%d enters Q1\n",pkts_read);
			moveFromQ1toQ2(ds);
		}
	if(mode==0)
		noOfPkts=pkts_read;	
	pthread_mutex_unlock(&mutex);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
	}
	
	return 0;
}

//call for token thread
void *tokenThreadProcedureCall(void *arg){
	sharedDS* ds=(sharedDS*)arg;
	
	t_count=1;
	t_dropped=0;
	double timeInterval=(double)(1/ds->r); //in seconds
	if(timeInterval>10)
		timeInterval=10;
	while(1){
		
		usleep(timeInterval*1000000);
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
		//critical section
		pthread_mutex_lock(&mutex);
		long double d=getTimestamp();
		printf("%012.3Lfms: ",d);
		if(ds->token_count<ds->b)
		{
			ds->token_count+=1;
			//print timestamp
			printf("token t%d arrives, token bucket now has %d token\n",t_count,ds->token_count);
			
		}else{
			t_dropped++;
			printf("token t%d arrives, dropped\n",t_count);
		}
		t_count++;
		//check if obj can be moved from Q1 to Q2
		moveFromQ1toQ2(ds);
		pthread_mutex_unlock(&mutex);
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
		if(ds->num<1 && My402ListEmpty(&(ds->q1))==1)
			break;
	}
	return 0;
}


//server thread first procedure call
void *serverThreadProcedureCall(void *arg){
	sharedDS* ds=(sharedDS*)arg;
	while((My402ListEmpty(&(ds->q1))!=1 || My402ListEmpty(&(ds->q2))!=1 || ds->num>0)){
		if(terminate==1)
			break;
		long double d;
		pthread_mutex_lock(&mutex);
		//guard condition
		while((My402ListEmpty(&(ds->q2))==1 && My402ListEmpty(&(ds->q2))!=1 && ds->num>0) && terminate==0)
			pthread_cond_wait(&cv,&mutex);

			
		//critical section code
		//dequeue the packet
		if(My402ListEmpty(&(ds->q2))==1){
			pthread_mutex_unlock(&mutex);
			continue;
		}
		My402ListElem *elem=My402ListFirst(&(ds->q2)); 
		Packet* packet=(Packet*)elem->obj;
		My402ListUnlink(&(ds->q2),elem);
		pthread_mutex_unlock(&mutex);
		d=getTimestamp();
		printf("%012.3Lfms: ",d);
		
		//print time in Q2
		printf("p%d leaves Q2, time in Q2 = %.3Lfms\n",packet->id, d-packet->Q2_entry_time);	
		ds->time_Q2+=d-packet->Q2_entry_time;
		//sleep for service time
		d=getTimestamp();
		printf("%012.3Lfms: ",d);
		packet->service_start=d;
		if(pthread_self()==s1Thread){
			printf("p%d begins service at S1, requesting %dms of service\n",packet->id,packet->service_time);
		}
		else if(pthread_self()==s2Thread){
			printf("p%d begins service at S2, requesting %dms of service\n",packet->id,packet->service_time);
		}	 
		usleep(packet->service_time*1000);	
		d=getTimestamp();
		printf("%012.3Lfms: ",d);
		//print final eject data
		ds->total_service_time+=d-packet->service_start;
		ds->time_S+=d-packet->arrival_time;
		ds->time_SS+=(pow((d-packet->arrival_time)/1000,2)); //store in seconds
		if(pthread_self()==s1Thread){
			ds->time_S1+=d-packet->service_start;
			printf("p%d departs from S1, service time = %.3Lfms, time in system = %.3Lfms\n",packet->id,d-packet->service_start,
			d-packet->arrival_time);
		}
		else if(pthread_self()==s2Thread){
			ds->time_S2+=d-packet->service_start;
			printf("p%d departs from S2, service time = %.3Lfms, time in system = %.3Lfms\n",packet->id,d-packet->service_start,
			d-packet->arrival_time);
		}
		
		//printf("q1 : %d and q2 : %d and ds num : %d\n",My402ListLength(&(ds->q1)),My402ListLength(&(ds->q2)),ds->num);
	}

	return 0;
}


//ctrl c handling thread
void *controlThreadProcedureCall(void* arg) {
	 sharedDS* ds=(sharedDS*)arg;
	 int sig=2;
	 while (1) {
		sigwait(&set, &sig);
		pthread_mutex_lock(&mutex);
		terminate=1;
		pthread_cancel(tokenThread);
		pthread_cancel(packetThread);
		long double d=getTimestamp();
		printf("%012.3Lfms: ",d);
		printf("SIGINT caught, no new packets or tokens will be allowed\n");
		pthread_mutex_unlock(&mutex);
		removed_pkts_Q1=My402ListLength(&(ds->q1));
		int i=0;
		while(i<removed_pkts_Q1){
			My402ListElem *elem=My402ListFirst(&(ds->q1)); 
			Packet* packet=(Packet*)elem->obj;
			My402ListUnlink(&(ds->q1),elem);
			d=getTimestamp();
			printf("%012.3Lfms: ",d);	
			//print time
			printf("p%d removed from Q1\n",packet->id);	
			i++;
			}
		i=0;
		while(i<removed_pkts_Q2){
			My402ListElem *elem=My402ListFirst(&(ds->q2)); 
			Packet* packet=(Packet*)elem->obj;
			My402ListUnlink(&(ds->q2),elem);
			d=getTimestamp();
			printf("%012.3Lfms: ",d);	
			//print time
			printf("p%d removed from Q2\n",packet->id);	
			i++;
			}
		}
	 return(0);
}
//create all threads
void createThreads(sharedDS* ds){
	gettimeofday(&startTime,NULL);
	pthread_create(&controlThread,NULL,controlThreadProcedureCall,ds);
	printf("00000000.000ms: emulation begins\n");
	pthread_create(&packetThread,NULL,packetThreadProcedureCall,ds);
    pthread_create(&tokenThread,NULL,tokenThreadProcedureCall,ds);
    pthread_create(&s1Thread,NULL,serverThreadProcedureCall,ds);
    pthread_create(&s2Thread,NULL,serverThreadProcedureCall,ds);
	return;
}

//join all threads together 
void syncThreadsBeforeExit(sharedDS* ds){
	//while((My402ListEmpty(&(ds->q1))!=1 || My402ListEmpty(&(ds->q2))!=1 || ds->num>0)||(terminate==0));
	pthread_mutex_lock(&mutex);
	pthread_cond_broadcast(&cv);
	pthread_mutex_unlock(&mutex);
	pthread_join(packetThread,NULL);
	pthread_join(tokenThread,NULL);
	pthread_join(s1Thread,NULL);
	pthread_join(s2Thread,NULL);
	
}

//set default vals for structure
void initializeVals(sharedDS* ds){
	ds->lambda=1,ds->mu=0.35,ds->r=1.5,ds->b=10;
	ds->p=3,ds->num=20;
	(void)My402ListInit(&(ds->q1));
	(void)My402ListInit(&(ds->q2));
	ds->token_count=0;
	ds->fp=NULL;
	ds->total_interarrival_time=0;
	ds->total_service_time=0;
	ds->droppedPkts=0;
	ds->time_Q1=0;
	ds->time_Q2=0;
	ds->time_S1=0;
	ds->time_S2=0;
	ds->time_S=0;
	ds->time_SS=0;
}

//read from cmdLine
void parseCMDLine(int argc, char *argv[],sharedDS* ds){
    for(int i=1;i<argc;i++)
    {
		if((i%2)==1 && !(strcmp(argv[i],"-t")==0 ||strcmp(argv[i],"-n")==0 ||strcmp(argv[i],"-P")==0 ||strcmp(argv[i],"-B")==0 ||
		strcmp(argv[i],"-r")==0 ||strcmp(argv[i],"-lambda")==0 ||strcmp(argv[i],"-mu")==0)){
					fprintf(stderr, "Malformed cmd line args. Refer README\n");
	   				exit(1);	
		}
		if(argc>1 && argc%2==0){
			fprintf(stderr, "Malformed cmd line args. Refer README\n");
	   		exit(1);
		}
        //check for file..
        if(strcmp(argv[i],"-t")==0)
        {
            if(i+1<argc && argv[i+1][0]!='-')
            {
				if(strstr(argv[i+1],"/etc")!=NULL){
					fprintf(stderr, "%s is not a file (or) file is not  in the correct format.\n",argv[i+1]);
	   				exit(1);	
				}
				if(strstr(argv[i+1],"bashrc")!=NULL || strstr(argv[i+1],"/var/log")!=NULL){
					fprintf(stderr, "Access denied. Cannot open %s.\n",argv[i+1]);
	   				exit(1);	
				}
                ds->fp=fopen(argv[i+1],"r");
				if(ds->fp!=NULL)
				{
					filename=argv[i+1];
					getNumberOfPacketsFromFile(ds);
					mode=1;
					break;
				}					
				else{
					fprintf(stderr, "File does not exist (or) file cannot be opened.\n");
	   				exit(1);	
				}
        	}else{
				fprintf(stderr, "Incorrect file argument.\n");
	   			exit(1);				
			}      
        }
    }
	//get other arguments
	if(mode==0){
	//read lambda mu P n
		for(int i=1;i<argc;i++){
			//lambda
			if(strcmp(argv[i],"-lambda")==0 && argv[i+1][0]!='-'){
				ds->lambda=atof(argv[i+1]);
			}
			else if(strcmp(argv[i],"-mu")==0 && argv[i+1][0]!='-'){
				ds->mu=atof(argv[i+1]);
			}
			else if(strcmp(argv[i],"-P")==0 && argv[i+1][0]!='-'){
				ds->p=atoi(argv[i+1]);
			}
			else if(strcmp(argv[i],"-n")==0 && argv[i+1][0]!='-'){
				ds->num=atoi(argv[i+1]);
			}
		}
	}
	for(int i=1;i<argc;i++)
	{
		if(strcmp(argv[i],"-r")==0 && argv[i+1][0]!='-'){
			ds->r=atof(argv[i+1]);
		}
		else if(strcmp(argv[i],"-B")==0 && argv[i+1][0]!='-'){
			ds->b=atof(argv[i+1]);
		}
	}	
}

void printEvalParam(sharedDS* ds){
	printf("Emulation Parameters:\n");
	printf("\tnumber to arrive = %d\n",ds->num);
    if(mode==0)
	{
		printf("\tlambda = %.3lf\n",ds->lambda);
		printf("\tmu = %.3lf\n",ds->mu);
	}   
    printf("\tr = %.3lf\n",ds->r);
	printf("\tB = %.3lf\n",ds->b);
	if(mode==0)
		printf("\tP = %d\n",ds->p);
	if(mode==1)
		printf("\ttsfile = %s\n",filename);
       

}

void printStatistics(sharedDS* ds){
	printf("\n");

	printf("Statistics:\n");
	//calc avg pkt inter arr. time
	

	printf("\taverage packet inter-arrival time = %.8Lf seconds\n",(ds->total_interarrival_time/noOfPkts)/1000);
	if((noOfPkts-ds->droppedPkts)==0 || ds->total_service_time==0 )
		printf("\taverage packet service time = N/A (No packets serviced)\n");
	else
    	printf("\taverage packet service time = %.8Lf seconds\n",(ds->total_service_time/(noOfPkts-ds->droppedPkts-removed_pkts_Q1-removed_pkts_Q2))/1000);
    printf("\taverage number of packets in Q1 = %.8Lf\n",(ds->time_Q1/emulation_end));
	printf("\taverage number of packets in Q2 = %.8Lf\n",(ds->time_Q2/emulation_end));
	printf("\taverage number of packets in S1 = %.8Lf\n",(ds->time_S1/emulation_end));
	printf("\taverage number of packets in S2 = %.8Lf\n",(ds->time_S2/emulation_end));
    printf("\n");
	
    if(ds->time_S==0 || (noOfPkts-ds->droppedPkts)==0){
		printf("\taverage time a packet spent in system = N/A\n (No packets entered the system successfully");
		printf("\tstandard deviation for time spent in system = N/A\n");
	}
	else{
    	printf("\taverage time a packet spent in system = %.8Lf seconds\n",(ds->time_S/(noOfPkts-ds->droppedPkts-removed_pkts_Q1-removed_pkts_Q2))/1000);
		long double var=sqrt(((ds->time_SS)/(noOfPkts-ds->droppedPkts)) - pow(((ds->time_S/1000)/(noOfPkts-ds->droppedPkts-removed_pkts_Q1-removed_pkts_Q2)),2));
		printf("\tstandard deviation for time spent in system = %.8Lf seconds\n",var);
	}
	
	printf("\n");
	token_drop_probability=(long double)t_dropped/t_count;
	printf("\ttoken drop probability = %.8Lf\n",token_drop_probability);
	packet_drop_probability=(long double)ds->droppedPkts/pkts_read;
    printf("\tpacket drop probability = %.8Lf\n",packet_drop_probability);
}



int main(int argc, char *argv[]){
	if(argc>15){
		fprintf(stderr, "Too many arguments.\n");
	   	exit(1);	
	}
	sigemptyset(&set);
	sigaddset(&set,SIGINT);
	sigprocmask(SIG_BLOCK,&set, 0);
	sharedDS* ds=(sharedDS*)malloc(sizeof(sharedDS));
	initializeVals(ds);
    parseCMDLine(argc,argv,ds);
	printEvalParam(ds);
	createThreads(ds);    
	syncThreadsBeforeExit(ds);
	pthread_cond_broadcast(&cv);
	emulation_end=getTimestamp();
	printf("%012.3Lfms: emulation ends\n",emulation_end);
	printStatistics(ds);
	pthread_cancel(controlThread);
	pthread_join(controlThread,NULL);
    return 0;
}

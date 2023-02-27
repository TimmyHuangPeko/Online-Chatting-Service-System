#include "chatsystem.h"
#include <semaphore.h>		/*sem_t, sem_init()*/
#include <pthread.h>

#define DEBUG

#define DFL_PORT "12000"
#define DFL_ADDR "1.173.222.247"
#define QLEN 5
#define MXUSR 67


/*off-line message*/
typedef struct offline_message{
	message_packet* packet;
	struct offline_message* next;
}offline_message;

/*on-line state*/
typedef struct online_state{
	char name[NAMESIZE];
	sockaddr_in addr;
	int fd;
	sem_t mutex;
	struct online_state* next;
}online_state;

/*user information*/
typedef struct user_information{
	char name[NAMESIZE];
	online_state* state;
	offline_message* head;
}user_information;


/*server address*/
sockaddr_in srv_addr;

/*master fd*/
static master_fd = 0;

/*error number*/
extern int errno;

/*whole user list*/
user_information* user_list[MXUSR] = {NULL};

/*head of on-line list*/
online_state* head = NULL;

/*shared memory mutex of on-line list and user list*/
pthread_mutex_t list_mutex;


/*main function*/
void erroutput(const char* format, ...);
void passive_open(char* port, char* addr);
int user_find(int* index, char* name);
void user_scan();
void state_errctrl(online_state* state);
void packet_print(message_packet* packet, int size);
void state_print(online_state* state);

/*on-line list operation*/
void online_push(online_state* state, char* name);
void online_pop(online_state* state);
void online_scan();
void online_print(online_state* state);

/*off-line message list operation*/
void message_push(offline_message** msg_head, message_packet* packet);
void message_pop(offline_message** msg_head);
void message_scan(offline_message* msg_head);

/*client connection request*/
void* serve_client(void* arg);

/*unused function*/
//void sigchld_handler(int signum);



int main(int argc, char** argv)
{
	/*create passive socket*/
	char port[6];
	char addr[16];
	
	switch(argc){
		
		case 1:
			
			strcpy(addr, "0");
			strcpy(port, "0");
			break;
		case 2:
			
			strcpy(addr, argv[1]);
			strcpy(port, "0");
			break;
		case 3:
			
			strcpy(addr, argv[1]);
			strcpy(port, argv[2]);
			break;
		default:
			
			printf("[MASTER PROCESS MESSAGE]unable to open the server: invalid open\n");
			return 0;
	}
	
	passive_open(port, addr);
	
	
	/*initialize thread, mutex attribute*/
	pthread_t thread;
	pthread_attr_t attr;
	
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_mutex_init(&list_mutex, 0);
	
	/*handle connection request*/
	online_state* state;
	socklen_t addr_len;
	
	while(1){
		
		state = NULL;	//test if is needed
		state = (online_state*)malloc(sizeof(online_state));
		memset(state, 0, sizeof(online_state));
		addr_len = sizeof(state->addr);
		
		if((state->fd = accept(master_fd, (sockaddr*)&state->addr, &addr_len)) < 0){
			
			if(errno == EINTR) continue;
			erroutput("[MASTER THREAD ERROR]failed to accept new connection: %s\n", strerror(errno));
			continue;
		}
		sem_init(&state->mutex, 0, 1);
		
		if(pthread_create(&thread, &attr, &serve_client, (void*)state) < 0){
			
			erroutput("[MASTER THREAD ERROR]failed to create new slave thread: %s\n", strerror(errno));
			state_errctrl(state);
		}
	}
}




void erroutput(const char* format, ...)
{
	/*--------------------------------------------------------------
	 * output error message to screen
	 *  
	 *--------------------------------------------------------------
	 */
	
	va_list args;
	
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}

void passive_open(char* port, char* addr)
{
	/*--------------------------------------------------------------
	 * set IP address and port number of server
	 * create a passive socket for waiting connection request
	 *  
	 *--------------------------------------------------------------
	 */
	
	if(port[0] == '0') strcpy(port, DFL_PORT);
	if(addr[0] == '0' && strlen(addr) == 1) strcpy(addr, DFL_ADDR);
	
	
	memset(&srv_addr, 0, sizeof(sockaddr_in));
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_port = htons((short)strtol(port, NULL, 10));
	srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	#ifdef DEBUG
		printf("[DEBUG]port number = %hd\n", ntohs(srv_addr.sin_port));
	#endif
	
	
	if((master_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
		
		erroutput("[MASTER PROCESS ERROR]failed to create new socket: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	#ifdef DEBUG
		printf("[DEBUG]socket descriptor = %d\n", master_fd);
	#endif
	
	
	if(bind(master_fd, (sockaddr*)&srv_addr, sizeof(sockaddr_in)) < 0){
		
		erroutput("[MASTER PROCESS ERROR]failed to bind socket %d with port %hd: %s\n", master_fd, ntohs(srv_addr.sin_port), strerror(errno));
		exit(EXIT_FAILURE);
	}
	#ifdef DEBUG
		printf("[DEBUG]bind socket success\n");
	#endif
	
	
	if(listen(master_fd, QLEN) < 0){
		
		erroutput("[MASTER PROCESS ERROR]failed to turn socket to passive mode: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	#ifdef DEBUG
		printf("[DEBUG]turn socket to passive mode success\n");
	#endif
	
	
	printf("[MASTER PROCESS MESSAGE]successfully create passive socket at master process\n");
}

void* serve_client(void* arg)
{
	/*--------------------------------------------------------------
	 * interact with client, including sending message, receiving message, and notification
	 *  
	 *--------------------------------------------------------------
	 */
	
	online_state* state = (online_state*)arg;
	
	message_packet* packet = (message_packet*)malloc(sizeof(message_packet));
	int rcv_size = 0;
	int snd_size = 0;
	int user_index = 0;
	
	do{
		
		memset(packet, 0, sizeof(message_packet));
		
		rcv_size = read(state->fd, packet, sizeof(message_packet));
		#ifdef DEBUG
			packet_print(packet, rcv_size);
			//state_print(state);
		#endif
		
		switch(rcv_size){
			
			case 0:
				
				pthread_mutex_lock(&list_mutex);
					user_list[user_index]->state = NULL;
					sem_destroy(&state->mutex);
				pthread_mutex_unlock(&list_mutex);
				
				packet->type = 0;
				strcpy(packet->name, strcat(state->name, "-"));
				packet->time = time(NULL);
				
				for(online_state* temp = head; temp; temp = temp->next){
					
					sem_wait(&temp->mutex);
					if(temp != state && (snd_size = write(temp->fd, packet, sizeof(message_packet))) < 0){
						
						erroutput("[SLAVE THREAD ERROR]failed to send packet to %s: %s\n", temp->name, strerror(errno));
					}
					sem_post(&temp->mutex);
				}
				
				//sem_wait(&state->mutex);
				pthread_mutex_lock(&list_mutex);
					close(state->fd);
					online_pop(state);
				pthread_mutex_unlock(&list_mutex);
				
				
				#ifdef DEBUG
					online_scan();
				#endif
				
				pthread_exit(NULL);
				
			default:
				
				switch(packet->type){
					
					case 0:
						
						/*update on-line list and user list*/
						pthread_mutex_lock(&list_mutex);
							online_push(state, packet->name);
							#ifdef DEBUG
								online_scan();
							#endif
							
							if(!user_find(&user_index, packet->name)){
								
								user_information* nw_user = (user_information*)malloc(sizeof(user_information));
								
								user_list[user_index] = nw_user;
								strcpy(nw_user->name, packet->name);
								nw_user->head = NULL;
								#ifdef DEBUG
									user_scan();
								#endif
							}
							user_list[user_index]->state = state;
						pthread_mutex_unlock(&list_mutex);
						#ifdef DEBUG
							message_scan(user_list[user_index]->head);
						#endif
						
						/*send on-line message*/
						sem_wait(&state->mutex);
						pthread_mutex_lock(&list_mutex);
							while(user_list[user_index]->head){
								
								if((snd_size = write(state->fd, user_list[user_index]->head->packet, sizeof(message_packet))) < 0){
									
									erroutput("[SLAVE THREAD ERROR]failed to send packet to %s: %s\n", state->name, strerror(errno));
								}
								message_pop(&user_list[user_index]->head);
							}
						pthread_mutex_unlock(&list_mutex);
						sem_post(&state->mutex);	
						#ifdef DEBUG
							message_scan(user_list[user_index]->head);
						#endif
						
						/*notify other users*/
						strcpy(packet->content, inet_ntoa(state->addr.sin_addr));
						
						pthread_mutex_lock(&list_mutex);
							for(online_state* temp = head; temp; temp = temp->next){
								
								sem_wait(&temp->mutex);
									if(temp != state && (snd_size = write(temp->fd, packet, sizeof(message_packet))) < 0){
										
										erroutput("[SLAVE THREAD ERROR]failed to send packet to %s: %s\n", temp->name, strerror(errno));
									}
								sem_post(&temp->mutex);
							}
						pthread_mutex_unlock(&list_mutex);
						break;
						
					case 2:;
						
						/*receive packet from user*/
						int rcvr_index = 0;
						
						pthread_mutex_lock(&list_mutex);
							if(!user_find(&rcvr_index, packet->name)){
							
								/*unknown user*/
								pthread_mutex_unlock(&list_mutex);
								
								strcat(packet->name, " ");
								sem_wait(&state->mutex);
									if((snd_size = write(state->fd, packet, sizeof(message_packet))) < 0){
											
										erroutput("[SLAVE THREAD ERROR]failed to send packet: %s\n", strerror(errno));
									}
								sem_post(&state->mutex);
							}
							else if(user_list[rcvr_index]->state){
							
								/*on-line user*/
								online_state* rcv_state = user_list[rcvr_index]->state;
								pthread_mutex_unlock(&list_mutex);
								
								sem_wait(&state->mutex);
									if((snd_size = write(state->fd, packet, sizeof(message_packet))) < 0){
											
										erroutput("[SLAVE THREAD ERROR]failed to send packet: %s\n", strerror(errno));
									}
								sem_post(&state->mutex);
								
								packet->type = 1;
								strcpy(packet->name, state->name);
								sem_wait(&rcv_state->mutex);
									if((snd_size = write(rcv_state->fd, packet, sizeof(message_packet))) < 0){
											
										erroutput("[SLAVE THREAD ERROR]failed to send packet: %s\n", strerror(errno));
									}
								sem_post(&rcv_state->mutex);
							}
							else{
								
								/*off-line user*/
								pthread_mutex_unlock(&list_mutex);
								
								strcat(packet->name, "-");
								sem_wait(&state->mutex);
									if((snd_size = write(state->fd, packet, sizeof(message_packet))) < 0){
											
										erroutput("[SLAVE THREAD ERROR]failed to send packet: %s\n", strerror(errno));
									}
								sem_post(&state->mutex);
								
								pthread_mutex_lock(&list_mutex);
									strcpy(packet->name, state->name);
									strcat(packet->name, "-");
									packet->type = 1;
									
									message_push(&user_list[rcvr_index]->head, packet);
								pthread_mutex_unlock(&list_mutex);
								#ifdef DEBUG
									message_scan(user_list[rcvr_index]->head);
								#endif
								
								packet = NULL;
								packet = (message_packet*)malloc(sizeof(message_packet));
							}
						break;
						
				}
				break;
				
			case -1:
				
				erroutput("[SLAVE PROCESS ERROR]socket %d failed to receive packet: %s\n", state->fd, strerror(errno));
				
		}
	}while(rcv_size);
}

int user_find(int* index, char* name)
{
	/*--------------------------------------------------------------
	 * using hash function to find user's index
	 * open addressing: double hashing
	 * formula: h(k,i) = ( (k%m) + i*((k%(m-1))+1) )%m
	 *--------------------------------------------------------------
	 */
	
	int key = 0;
	int i = 0;
	for(int j = 0; j < strlen(name); j++) key += name[j];
	
	do{
		
		++i;
		*index = ( (key % MXUSR) + i*(1 + (key % (MXUSR - 1))) ) % MXUSR;
	}while(user_list[*index] && strcmp(user_list[*index]->name, name));
	
	if(user_list[*index]) return 1;
	return 0;
}

void user_scan()
{
	printf("[DEBUG]user list:\n");
	for(int i = 0; i < MXUSR; i++) if(user_list[i]) printf("\t%2d %s\n", i, user_list[i]->name);
}

void online_push(online_state* state, char* name)
{
	/*--------------------------------------------------------------
	 * push new on-line state to list(online_state)
	 *--------------------------------------------------------------
	 */
	online_state* temp = head;
	
	if(!temp) head = state;
	else{
		
		while(temp->next) temp = temp->next;
		temp->next = state;
	}
	
	strcpy(state->name, name);
	state->next = NULL;
}

void online_pop(online_state* state)
{
	/*--------------------------------------------------------------
	 * pop given on-line state from list(online_state)
	 *--------------------------------------------------------------
	 */
	online_state* temp = head;
	
	if(!temp) return;
	else if(temp == state) head = head->next;
	else{
		
		while(temp->next && temp->next != state) temp = temp->next;
		if(!temp->next) return;
		temp->next = temp->next->next;
	}
	
	free(state);
	state = NULL;
}

void online_scan()
{
	/*--------------------------------------------------------------
	 * scan and print all of the on-line state
	 *--------------------------------------------------------------
	 */
	printf("[DEBUG]online list: %p\n", head);
	for(online_state* temp = head; temp != NULL; temp = temp->next) printf("\t%s %15s %2d %p -> %p\n", temp->name, inet_ntoa(temp->addr.sin_addr), temp->fd, temp, temp->next); 
}

void online_print(online_state* state)
{
	printf("[DEBUG]online state:\n");
	printf("\t%8s %15s %2d\n", state->name, inet_ntoa(state->addr.sin_addr), state->fd);
}


void message_push(offline_message** msg_head, message_packet* packet)
{
	offline_message* nw_message = (offline_message*)malloc(sizeof(offline_message));
	offline_message* temp = *msg_head;
	
	if(!temp) *msg_head = nw_message;
	else{
		
		while(temp->next) temp = temp->next;
		temp->next = nw_message;
	}
	
	nw_message->packet = packet;
	nw_message->next = NULL;
}

void message_pop(offline_message** msg_head)
{
	offline_message* temp = *msg_head;
	*msg_head = (*msg_head)->next;
	free(temp);
}

void message_scan(offline_message* msg_head)
{
	printf("[DEBUG]offline message:\n");
	for(offline_message* temp = msg_head; temp; temp = temp->next) printf("\t%s\n", temp->packet->content);
}

void state_errctrl(online_state* state)
{
	close(state->fd);
	sem_destroy(&state->mutex);
	free(state);
	state = NULL;
}

void packet_print(message_packet* packet, int size)
{
	printf("[DEBUG]packet information:\n");
	printf("\tsize: %d\n", size);
	printf("\ttype: %d | name: %8s\n", packet->type, packet->name);
	printf("\ttime: %ld\n", packet->time);
	printf("\t%s\n", packet->content);
}

void state_print(online_state* state)
{
	printf("[DEBUG]online state:\n");
	printf("\t%s\n", state->name);
	printf("\t%s\n", inet_ntoa(state->addr.sin_addr));
	printf("\t%d\n", state->fd);
}








/*code for multi-process*/
	/*struct sigaction sigchld_action;
	memset(&sigchld_action, 0, sizeof(struct sigaction));
	sigchld_action.sa_handler = &sigchld_handler;
	sigchld_action.sa_flags = 0;
	if(sigfillset(&sigchld_action.sa_mask)){
		
		erroutput("[MASTER PROCESS ERROR]failed to set signal mask: %d\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	if(sigaction(SIGCHLD, &sigchld_action, NULL)){
		
		erroutput("[MASTER PROCESS ERROR]failed to register signal handler: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	
	pid_t c_pid;
	
	while(1){
		
		if((slave_fd = accept(master_fd, (sockaddr*)&cli_addr, &addr_len)) < 0){
			
			if(errno != EINTER) erroutput("[MASTER PROCESS ERROR]failed to accept new connection: %s", strerror(errno));
			
			#ifdef DEBUG
				printf("[DEBUG]accept new connection, new socket = %d\n", slave_fd);
			#endif
			
			continue;
		}
		
		switch((c_pid = fork())){
			
			case 0:
				
				close(master_fd);
				serve_client(slave_fd);
				exit(EXIT_SUCCESS);
			default:
				
				close(slave_fd);
				printf("[MASTER PROCESS MESSAGE]fork new process %d to handle new socket %d", c_pid, slave_fd);
				break;
			case -1:
				
				erroutput("[MASTER PROCESS ERROR]failed to fork new process: %s\n", strerror(errno));
		}
	}*/


/*void sigchld_handler(int signum)
{
	*--------------------------------------------------------------
	 * release resource of slave process
	 *  
	 *--------------------------------------------------------------
	 *

	int status = 0;
	int rslt = 0;
	while((rslt = waitpid(-1, &status, WNOHANG)) >= 0){
		
		if(WEXITSTATUS(status) != EXIT_SUCCESS) printf("[MASTER PROCESS MESSAGE]unusual close of slave process %d\n", rslt);
		else if(rslt) printf("[MASTER PROCESS MESSAGE]resource of slave process %d has been released\n", rslt);
	}
}*/





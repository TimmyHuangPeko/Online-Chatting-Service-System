#include "chatsystem.h"

//#define DEBUG

#define DFL_PORT 12000
#define DFL_ADDR "127.0.0.1"
#define FILENAME "help.txt"

typedef struct tm tm;

sockaddr_in srv_addr;
static volatile fd = -1;
char command[][8] = {"connect", "chat", "leave", "exit", "help"};

extern int errno;
volatile sig_atomic_t loop_flag = 1;

void erroutput(const char* format, ...);
int active_connect(uint16_t port, char* addr);
void sigchld_handler(int signum);
void sigterm_handler(int signum);
void socket_close(int c_pid);
void message_send(char* name_list, char* message);
int packet_send(const char type, char* name, char* message);
int packet_receive();
void time_output(tm* human_time);
void packet_print(message_packet* packet);


int main(void)
{
	/*--------------------------------------------------------------
	 *                         MAIN
	 * 
	 * line 1: [add] multiline input
	 *--------------------------------------------------------------
	 */
	 
	char cmd[8 + NAMESIZE*8 + 10 + MSS];
	char action[8];
	pid_t c_pid;
	
	while(1){
		
		strcpy(cmd, "");
		strcpy(action, "");
		
		fgets(cmd, sizeof(cmd), stdin); 	//1
		
		if(!strncmp(cmd, command[0], strlen(command[0]))){
			
			/*--------------------------------------------------------------
			 *					| Connect to chat server |
			 *					--------------------------
			 * line 1: [test] try char*
			 * line 2: [error] %d may cause error
			 * line x: [add] waiting visualization
			 * line x: [add] show online user list and more description
			 *--------------------------------------------------------------
			 */
			 
			uint16_t port;
			char addr[16];	//1
			char name[NAMESIZE];
			
			
			if(fd != -1){
				
				printf("[SYSTEM MESSAGE]you are already on-line\n");
			}
			else if(sscanf(cmd, "%s %s %hd %[a-zA-Z0-9]\n", action, addr, &port, name) == 4){	//2
				
				if(active_connect(port, addr)){
					
					struct sigaction sigchld_action;
					memset(&sigchld_action, 0, sizeof(struct sigaction));
					sigchld_action.sa_handler = &sigchld_handler;
					sigchld_action.sa_flags = 0;
					if(sigfillset(&sigchld_action.sa_mask)){
						
						erroutput("[SYSTEM ERROR]failed to initialize parent signal handler: %s\n", strerror(errno));
						exit(EXIT_FAILURE);
					}
					
					if(sigaction(SIGCHLD, &sigchld_action, NULL)){
						
						erroutput("[SYSTEM ERROR]failed to register child signal handler: %s\n", strerror(errno));
						exit(EXIT_FAILURE);
					}
					
					
					
					c_pid = fork();
					switch(c_pid){
						
						case 0:;	/*child process*/
							
							struct sigaction sigterm_action;
							
							memset(&sigterm_action, 0, sizeof(struct sigaction));
							sigterm_action.sa_handler = &sigterm_handler;
							sigterm_action.sa_flags = 0;
							if(sigfillset(&sigterm_action.sa_mask)){
								
								erroutput("[SYSTEM ERROR]failed to initialize child signal handler: %s\n", strerror(errno));
								exit(EXIT_FAILURE);
							}
							
							if(sigaction(SIGTERM, &sigterm_action, NULL)){
								
								erroutput("[SYSTEM ERROR]failed to register signal handler: %s\n", strerror(errno));
								exit(EXIT_FAILURE);
							}
							
							#ifdef DEBUG
								printf("[DEBUG]successfully registering signal handler\n");
							#endif
							
							packet_receive();
							
							exit(EXIT_SUCCESS);
						default:	/*parent process*/
							
							printf("---------- Welcome to Chat System! ----------\n");
							if(packet_send(0, name, "") < 0){
								
								printf("[SYSTEM MESSAGE]failed to login the chat system\n");
								socket_close(c_pid);
							}
							break;
						case -1:
							
							erroutput("[SYSTEM ERROR]failed to fork: %s\n", strerror(errno));
					}
				}
			}
			else printf("[COMMAND ERROR]undefine arguments format\n");
			
		}
		else if(!strncmp(cmd, command[1], strlen(command[1]))){
			
			/*--------------------------------------------------------------
			 *					| Chat to other users |
			 *					-----------------------
			 * [add]construct sending list and a child process to handle
			 *--------------------------------------------------------------
			 */
			 
			 char name_list[NAMESIZE*8 + 10];
			 char message[MSS];
			
			if(sscanf(cmd, "%s %[^\"]\"%[^\"]\"", action, name_list, message) >= 3){
				
				//printf("%s\n%s\n%d\n%d\n", name_list, message, sizeof(message), strlen(message));
				message_send(name_list, message);
			}
			else printf("[COMMAND ERROR]undefine arguments format\n");
			
		}
		else if(!strncmp(cmd, command[2], strlen(command[2]))){
			
			/*--------------------------------------------------------------
			 *					| disconnect with chat server |
			 *					-------------------------------
			 *
			 *--------------------------------------------------------------
			 */
			 
			if(fd == -1){
				
				printf("[SYSTEM MESSAGE]you are already off-line\n");
			}
			else if(sscanf(cmd, "%s\n", action) == 1){
				
				socket_close(c_pid);
				sleep(1);
				printf("----------  Leave the Chat System  ----------\n");
			}
			else printf("[COMMAND ERROR]undefine arguments format\n");
			
		}
		else if(!strncmp(cmd, command[3], strlen(command[3]))){
			
			/*--------------------------------------------------------------
			 *					| exit the chat system |
			 *					------------------------
			 * line 1: exit without reaping child process may cause problem
			 *--------------------------------------------------------------
			 */
			 
			if(sscanf(cmd, "%s\n", action) == 1){
				
				if(fd != -1){
					
					socket_close(c_pid);
					while(fd != -1);
				}
				sleep(1);
				return 0;	//1
			}
			else printf("[COMMAND ERROR]undefine arguments format\n");
			
		}
		else if(!strncmp(cmd, command[4], strlen(command[4]))){
			/*--------------------------------------------------------------
			 *				| client program instruction |
			 *				------------------------------
			 * 
			 *--------------------------------------------------------------
			 */
			
			if(sscanf(cmd, "%s\n", action) == 1){
				
				FILE* file = fopen(FILENAME, "r");
				char str[128] = "";
				
				while(fgets(str, sizeof(str), file)) printf("%s", str);
				fclose(file);
			}
			else printf("[COMMAND ERROR]undefine arguments format\n");
		}
		else{
			
			if(errno == EINTR) continue;
			printf("[COMMAND ERROR]invalid command\n");
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

void sigchld_handler(int signum)
{
	/*--------------------------------------------------------------
	 * release resource of child process
	 *  
	 *--------------------------------------------------------------
	 */
	
	int status = 0;
	int rslt = 0;
	
	rslt = waitpid(-1, &status, WNOHANG);
	#ifdef DEBUG
		printf("[DEBUG]release resource of child process\n");
	#endif
	
	if(rslt > 0 && fd != -1){
		
		if(WEXITSTATUS(status) != EXIT_SUCCESS){
			#ifdef DEBUG
		printf("%d\n", __LINE__);
	#endif
			printf("[SYSTEM MESSAGE]close the system due to child process error\n");
		}
		
		close(fd);
		fd = -1;
	}
}

void sigterm_handler(int signum)
{
	/*--------------------------------------------------------------
	 * end the loop of packet_receive()
	 *  
	 *--------------------------------------------------------------
	 */
	
	loop_flag = 0;
	#ifdef DEBUG
		printf("[DEBUG]end the receive process\n");
	#endif
}

int active_connect(uint16_t port, char* addr)
{
	/*--------------------------------------------------------------
	 * create socket and connect to server
	 *  
	 *--------------------------------------------------------------
	 */
	 
	memset(&srv_addr, 0, sizeof(sockaddr_in));
	if(!port) port = DFL_PORT;
	if(!addr) addr = DFL_ADDR;	//test
	
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_port = htons(port);
	srv_addr.sin_addr.s_addr = inet_addr(addr);

	
	if((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
		
		erroutput("[SYSTEM ERROR]failed to create socket: %s\n", strerror(errno));
		return 0;
	}
	
	if(connect(fd, (sockaddr*)&srv_addr, sizeof(sockaddr_in)) < 0){
		
		erroutput("[SYSTEM ERROR]failed to connect to server %s %hu: %s\n", addr, port, strerror(errno));
		return 0;
	}
	
	return 1;
}

void socket_close(int c_pid)
{
	/*--------------------------------------------------------------
	 * kill the child process and close the socket
	 *  
	 * line 1: sending SIGTERM may not end the loop of child process because of read() blocking
	 *--------------------------------------------------------------
	 */
	
	while(kill(c_pid, SIGTERM) == -1){	//1
		
		erroutput("[SYSTEM ERROR]failed to kill child process %d: %s\n", c_pid, strerror(errno));
	}
	#ifdef DEGUG
		printf("[DEBUG]success to kill child process %d\n", c_pid);
	#endif
	
	close(fd);
	fd = -1;
}

void message_send(char* name_list, char* message)
{
	/*--------------------------------------------------------------
	 * send given message to specified users throuah server
	 *  
	 *--------------------------------------------------------------
	 */
	 
	message_packet* packet = (message_packet*)malloc(sizeof(message_packet));
	char* name;
	name = strtok(name_list, " ");
	do{
		
		packet_send(2, name, message);
		//printf("\"%s\"\n", name);
		name = strtok(NULL, " ");
	}while(name);
}

int packet_send(const char type, char* name, char* message)
{
	/*--------------------------------------------------------------
	 * send packet to server with given information
	 *  
	 * line 1: [error]strlen(content) may cause problem (solved)
	 * line 2: [add]try to resend for serveral times if failed
	 * line 3: [error]static may cause problem
	 *--------------------------------------------------------------
	 */
	 
	int snd_size = 0;
	static message_packet packet; //3
	//printf("%s", addr); if(addr[strlen(addr)] == '\0') printf("yes\n");
	//printf("%s", name); if(name[strlen(name)] == '\0') printf("yes\n");
	//printf("%s", message); if(message[strlen(message)] == '\0') printf("yes\n");
	
	memset(&packet, 0, sizeof(message));
	packet.type = type;
	strcpy(packet.name, name);
	packet.time = time(NULL);
	strcpy(packet.content, message);
	
	if((snd_size = write(fd, &packet, sizeof(message_packet))) < 0){	//2
		
		erroutput("[SYSTEM ERROR]failed to send packet: %s\n", strerror(errno));
	}
	
	#ifdef DEBUG
		printf("[DEBUG]packet information:\n");
		printf("\tpacket detail: actual size = %d, send size = %d %d\n", sizeof(char) + strlen(name) + sizeof(time_t) + strlen(message), snd_size, sizeof(message_packet));
		printf("\t------------------------------------------\n");
		printf("\ttype: %d | name: %8s\n", packet.type, packet.name);
		printf("\ttime: %ld\n", packet.time);
		printf("\t%s\n", packet.content);
	#endif
	
	
	return snd_size;
}

int packet_receive()
{
	/*--------------------------------------------------------------
	 * receive packet from server
	 *  
	 *--------------------------------------------------------------
	 */
	
	message_packet* packet = (message_packet*)malloc(sizeof(message_packet));
	int rcv_size = 0;
	tm* human_time = (tm*)malloc(sizeof(tm));
	
	while(loop_flag){
		
		memset(packet, 0, sizeof(message_packet));
		memset(human_time, 0, sizeof(tm));
		
		rcv_size = read(fd, packet, sizeof(message_packet));
		#ifdef DEBUG
			packet_print(packet);
		#endif
		
		
		switch(rcv_size){
			
			case 0:
				
				if(errno == EINTR) continue;
				erroutput("[SYSTEM MESSAGE]lost connection to server %s %d\n",inet_ntoa(srv_addr.sin_addr), ntohs(srv_addr.sin_port));
				exit(EXIT_FAILURE);
				
			default:
				
				packet->time += (8*60*60);
				human_time = gmtime(&packet->time);
				
				switch(packet->type){
					
					case 0:
						
						if(packet->name[strlen(packet->name) - 1] == '-') printf("<User %5.8s is off-line>", strtok(packet->name, "-"));
						else printf("<User %5.8s is online, IP address: %9.15s>", packet->name, packet->content);
						time_output(human_time);
						printf("\n");
						break;
					case 1:
						
						if(packet->name[strlen(packet->name) - 1] == '-') printf("<User %5.8s has sent you a message \"%s\">", strtok(packet->name, "-"), packet->content);
						else printf("<User %5.8s sends a message \"%s\">", packet->name, packet->content);
						time_output(human_time);
						printf("\n");
						break;
					case 2:
						
						if(packet->name[strlen(packet->name) - 1] == '-') printf("<User %5.8s is off-line. The message will be passed when he/she comes back>", strtok(packet->name, "-"));
						else if(packet->name[strlen(packet->name) - 1] == ' ') printf("<User %5.8s does not exist>", strtok(packet->name, " "));
						else printf("<User %5.8s receives your message>", packet->name);
						time_output(human_time);
						printf("\n");
				}
				break;
				
			case -1:
				
				if(errno == EINTR) continue;
				erroutput("[SYSTEM ERROR]failed to receive packet: %s\n", strerror(errno));
				sleep(1);
				
		}
	}
	
	//if(packet) free(packet);
	//if(human_time) free(human_time);
	
}

inline void time_output(tm* human_time)
{
	/*--------------------------------------------------------------
	 * output time fit for human consumption
	 *  
	 *--------------------------------------------------------------
	 */
	
	printf(" -%02d:%02d %04d/%02d/%02d", human_time->tm_hour, human_time->tm_min, human_time->tm_year + 1900, human_time->tm_mon + 1, human_time->tm_mday);
}

void packet_print(message_packet* packet)
{
	/*--------------------------------------------------------------
	 * output the detail of packet
	 *  
	 *--------------------------------------------------------------
	 */
	 
	printf("[DEBUG]packet information:\n");
	printf("\ttype: %d | name: %8s\n", packet->type, packet->name);
	printf("\ttime: %ld\n", packet->time);
	printf("\t%s\n", packet->content);
}






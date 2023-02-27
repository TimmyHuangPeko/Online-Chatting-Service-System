#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <memory.h>
#include <unistd.h>			/*read(), write(), close(), fork()*/
#include <netinet/in.h>		/*struct sockaddr_in*/
#include <arpa/inet.h>		/*htonl(), inet_addr()*/
#include <sys/types.h>		/*pid_t, time_t, uint32_t*/
#include <sys/socket.h>		/*socket(), accept()*/
#include <sys/wait.h>		/*wait_pid(), WNOHANG*/
#include <sys/signal.h>		/*signal(), SIGCHLD*/
#include <sys/time.h>
#include <sys/errno.h>

#define MSS 4096
#define NAMESIZE 16

typedef struct sockaddr_in sockaddr_in;
typedef struct sockaddr sockaddr;

typedef struct message_packet{
	char type; // 0:introduce, 1:receive message, 2:send message
	char name[NAMESIZE];
	time_t time;
	char content[MSS];
}message_packet;



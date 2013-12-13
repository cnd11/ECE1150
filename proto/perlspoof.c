#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <math.h>
#include <memory.h>
#include <netdb.h>
#include <net/if.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define PERL_PORT 48083

#define MY_IP "169.254.0.1"

int main(void){
	char userInput[100];
	char data[100];
	
	int connectSocket = -1; // socket for connections
	printf("Connecting to name node.\n");
	
	printf("Creating name node socket... ");
	if ((connectSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) <= 0){
		perror("PERL: Error creating socket");
	}
	printf("Created!\n");
	
	struct sockaddr_in dataNodeAddr;
	memset(&dataNodeAddr, 0, sizeof(dataNodeAddr)); // reset dataNodeAddr to all 0's
	dataNodeAddr.sin_family = AF_INET; // sets address family to AF_INET
	dataNodeAddr.sin_addr.s_addr = inet_addr(MY_IP); // sets IP address to data node IP
	dataNodeAddr.sin_port = htons(PERL_PORT); // converts DATA_NODE_PORT to TCP/IP network byte order and sets it
	
	printf("Connecting to name node... ");
	if ((connect(connectSocket, (struct sockaddr *) &dataNodeAddr, sizeof(dataNodeAddr))) < 0){
		perror("PERL: Error connecting to data node");
	}
	printf("Connected to name node!\n");
	
	
	while(1){
		printf("Perl Spoofer -- Enter operation.\n");
		printf("1 = Read, 2 = Write, 3 = Search, 4 = Rename, 5 = Delete, exit to quit\n");
		if (fgets(userInput, sizeof userInput, stdin)!=NULL){
			if (strcmp(userInput, "exit") == 0){
				break;
			}
			memset(&data[0], 0, sizeof(data));
			switch(atoi(userInput)){
			case 1:
				strcat(data, "0\n");
				strcat(data, "home/benkisley/\n");
				strcat(data, "testread.txt\n");
				strcat(data, "benkisley\n");
				strcat(data, "n\n");
				strcat(data, "n\n");
				send(connectSocket, data, sizeof(data), 0);
				break;
			case 2:
				strcat(data, "1\n");
				strcat(data, "BOB/\n");
				strcat(data, "data.txt\n");
				strcat(data, "BOB\n");
				strcat(data, "this\n");
				strcat(data, "n\n");
				send(connectSocket, data, sizeof(data), 0);
				break;
			case 3:
				strcat(data, "2\n");
				strcat(data, "home/benkisley/\n");
				strcat(data, "testread.txt\n");
				strcat(data, "benkisley\n");
				strcat(data, "this\n");
				strcat(data, "n\n");
				send(connectSocket, data, sizeof(data), 0);
				break;
			case 4:
				strcat(data, "3\n");
				strcat(data, "BOB/\n");
				strcat(data, "data2.txt\n");
				strcat(data, "BOB\n");
				strcat(data, "this\n");
				strcat(data, "data.txt\n");
				send(connectSocket, data, sizeof(data), 0);
				break;
			case 5:
				strcat(data, "4\n");
				strcat(data, "BOB/\n");
				strcat(data, "data.txt\n");
				strcat(data, "BOB\n");
				strcat(data, "this\n");
				strcat(data, "n\n");
				send(connectSocket, data, sizeof(data), 0);
				break;
			default:
				break;			
			}
		}
	
	}


	return(0);
}
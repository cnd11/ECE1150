#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <math.h>
#include <memory.h>
#include <netdb.h>
#include <net/if.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define DATA_NODE_PORT 48007
#define CHUNK_SIZE 1024
#define MY_IP "169.254.0.2"

typedef enum {
	READ,
	WRITE,
	SEARCH
} reqType;

typedef struct nameNodeRequest{
	reqType operation;
	char directory[30];
	char filename[30];
	char chunkNo;
} nameNodeRequest;


int main(void){
	int nameNodeSocket = -1, listenSocket = -1;

	if(listenSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP) == -1){
		perror( "Error creating socket: " );
		exit( EXIT_FAILURE );
	}

	struct sockaddr_in listenSocketAddr;
	memset(&listenSocketAddr, 0, sizeof(listenSocketAddr));
	listenSocketAddr.sin_family = AF_INET;
	listenSocketAddr.sin_port   = htons( DATA_NODE_PORT );
	listenSocketAddr.sin_addr.s_addr = inet_addr(MY_IP);

	if(bind(listenSocket, (struct sockaddr *) &listenSocketAddr, sizeof(listenSocketAddr)) == -1){
		perror( "Error binding socket: " );
		close( listenSocket );
		exit( EXIT_FAILURE );
	}

	if(listen(listenSocket, 20) == -1){
		perror( "Error listen" );
	}

	while(1){
		struct sockaddr_in nameNodeSocketAddr;
		socklen_t nameNodeSocketAddrLen = sizeof(nameNodeSocketAddr);

		if(nameNodeSocket = accept(listenSocket, (struct sockaddr *) &nameNodeSocketAddr, &nameNodeSocketAddrLen) < 0){
			perror("Error accept: ");
			close(listenSocket);
			exit(EXIT_FAILURE);
		}

		nameNodeRequest* packet = (nameNodeRequest*)malloc(sizeof(nameNodeRequest));
		int errFlag = recv(nameNodeSocket, &(*packet), sizeof(*packet), 0);
		chdir(packet->directory);
		strcat(packet->filename, packet->chunkNo); // e.g. open testfile.wav4
		FILE* fp = fopen(packet->filename, "ab+");
		char tempBuffer[CHUNK_SIZE];
		
		switch(packet->operation){
		case READ:{
			size_t bytesRead = fread(tempBuffer, CHUNK_SIZE, 1, fp); // read chunk
			errFlag = send(nameNodeSocket, &tempBuffer, sizeof(tempBuffer)/sizeof(char), 0); // send data back to name node
			break;}
		case WRITE:{
			errFlag = recv(nameNodeSocket, &tempBuffer, sizeof(tempBuffer)/sizeof(char), 0); // receive chunk
			fprintf(fp, "%s", tempBuffer); // write chunk
			char ack[2] = "ok"; // send back an ACK
			errFlag = send(nameNodeSocket, &ack, sizeof(ack)/sizeof(char), 0);
			break;}
		}
		fclose(fp);
		
		if(shutdown(nameNodeSocket, SHUT_RDWR) == -1){
			perror("Error shutting down socket: ");
			close(nameNodeSocket);
			close(listenSocket);
			exit(EXIT_FAILURE);
		}
		close(nameNodeSocket);
	}

	close(listenSocket);
	return EXIT_SUCCESS;
}

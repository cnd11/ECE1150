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

#define DATA_NODE_PORT 48009
#define CHUNK_SIZE 1024
#define MY_IP "169.254.0.2"

typedef enum {
	READ,
	WRITE,
	SEARCH,
	RENAME,
	DELETE
} reqType;

typedef struct nameNodeRequest{
	reqType operation;
	char directory[30];
	char filename[30];
	char newFilename[30];
	char chunkNo;
} nameNodeRequest;

const char* ackPacket; // ACK and NAK packets
const char* nakPacket;

int main(void){
	int nameNodeSocket = -1, listenSocket = -1;
	FILE* fp;
	
	ackPacket = "ok";
	nakPacket = "no";
	static const nameNodeRequest EmptyStruct; // empty structure used for clearing structs
	
	printf("Creating data node listen socket... ");
	if((listenSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1){
		perror("DATA NODE: Error creating listen socket");
	}
	printf("Created!\n");

	struct sockaddr_in listenSocketAddr;
	memset(&listenSocketAddr, 0, sizeof(listenSocketAddr));
	listenSocketAddr.sin_family = AF_INET;
	listenSocketAddr.sin_port = htons( DATA_NODE_PORT );
	listenSocketAddr.sin_addr.s_addr = inet_addr(MY_IP);
	
	printf("Binding data node listen socket... ");
	if((bind(listenSocket, (struct sockaddr *) &listenSocketAddr, sizeof(listenSocketAddr))) == -1){
		perror("DATA NODE: Error binding listen socket");
		close(listenSocket);
	}
	printf("Bound!\n");

	printf("Starting listener for name node connection attempts... ");
	if(listen(listenSocket, 20) == -1){
		perror("DATA NODE: Error listening for incoming connections");
	}
	printf("Listening.\n");

	nameNodeRequest* packet = (nameNodeRequest*)(malloc (sizeof(nameNodeRequest))); // request packet from name node
	char tempBuffer[CHUNK_SIZE]; // buffer for reading and writing
	while(1){
		struct sockaddr_in nameNodeSocketAddr;
		socklen_t nameNodeSocketAddrLen = sizeof(nameNodeSocketAddr);

		if((nameNodeSocket = accept(listenSocket, (struct sockaddr *) &nameNodeSocketAddr, &nameNodeSocketAddrLen)) <= 0){
			perror("DATA NODE: Error accepting new connection");
			close(listenSocket);
		}

		*packet = EmptyStruct;
		printf("Waiting for data from name node...");
		if (recv(nameNodeSocket, &(*packet), sizeof(*packet), 0) <= 0){
			perror("DATA NODE: Error receiving request packet from name node");
		}
		printf(" Received packet from name node!\n");
		chdir(packet->directory);
		strcat(packet->filename, packet->chunkNo); // e.g. testfile.wav4
		
		switch(packet->operation){
		case READ:
			printf("Read: Reading chunk %s%s... ", packet->directory, packet->filename);
			fp = fopen(packet->filename, "a+");
			memset(&tempBuffer[0], 0, sizeof(tempBuffer));
			if (fread(tempBuffer, CHUNK_SIZE, 1, fp) != CHUNK_SIZE){ // reads chunk from file
				perror("READ: Error during file reading on data node");
			}
			printf("Read!\n");
			printf("Sending chunk back to name node... ");
			if (send(nameNodeSocket, &tempBuffer, sizeof(tempBuffer)/sizeof(char), 0) <= 0){ // sends chunk back to name node
				perror("READ: Error during chunk transmission from data node");
			}
			printf("Sent!\nFinished reading.\n");
			fclose(fp);
			break;
		case WRITE:
			printf("Write: Receiving chunk %s%s... ", packet->directory, packet->filename);
			fp = fopen(packet->filename, "a+");
			memset(&tempBuffer[0], 0, sizeof(tempBuffer));
			if (recv(nameNodeSocket, &tempBuffer, sizeof(tempBuffer)/sizeof(char), 0) <= 0){
				perror("WRITE: Error during data node chunk reception");
			}
			printf("Received data!");
			printf("Writing data to chunk... ");
			fprintf(fp, "%s", tempBuffer); // write chunk
			printf("Written!\n");
			printf("Sending ACK back to name node... ");
			if (send(nameNodeSocket, &ackPacket, sizeof(ackPacket)/sizeof(char), 0) <= 0){
				perror("WRITE: Error during data node ACK transmission");
			}
			printf("Sent!\nFinished writing.\n");
			fclose(fp);
			break;
		case RENAME:
			printf("Renaming file %s to %s... ", packet->filename, packet->newFilename);
			strcat(packet->newFilename, packet->chunkNo);
			if(rename(packet->filename, packet->newFilename) == 0){
				printf("Renamed!\n");
				printf("Sending ACK to name node... ");
				if (send(nameNodeSocket, &ackPacket, sizeof(ackPacket)/sizeof(char), 0) <= 0){
					perror("RENAME: Error during data node ACK transmission");
				}
				printf("Sent!\nFinished renaming.\n");
			}
			else{
				printf("Failed to rename!\n");
				printf("Sending NAK to name node... ");
				if (send(nameNodeSocket, &nakPacket, sizeof(nakPacket)/sizeof(char), 0) <= 0){
					perror("RENAME: Error during data node NAK transmission");
				}
				printf("Sent!\nFinished renaming.\n");
			}
			break;
		case DELETE:
			printf("Deleting file %s%s... ", packet->directory, packet->filename);
			if(remove(packet->filename) == 0){
				printf("Deleted!\n");
				printf("Sending ACK to name node... ");
				if (send(nameNodeSocket, &ackPacket, sizeof(ackPacket)/sizeof(char), 0) <= 0){
					perror("DELETE: Error during data node ACK transmission");
				}
				printf("Sent!\nFinished deleting.\n");
			}
			else{
				printf("Failed to delete!\n");
				printf("Sending NAK to name node... ");
				if (send(nameNodeSocket, &nakPacket, sizeof(nakPacket)/sizeof(char), 0) <= 0){
					perror("DELETE: Error during data node NAK transmission");
				}
				printf("Sent!\nFinished deleting.\n");
			}
			break;
		}
	}
	
	if(shutdown(nameNodeSocket, SHUT_RDWR) == -1){
		perror("DATA NODE: Error shutting down name node socket");
		close(nameNodeSocket);
		close(listenSocket);
		exit(EXIT_FAILURE);
	}
	close(nameNodeSocket);
	close(listenSocket);
	return EXIT_SUCCESS;
}

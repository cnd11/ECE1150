/*
* This program performs the duties of a data node on the distributed file system.
* Written by Ben Kisley, 11/1/13
*/

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
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define DATA_NODE_PORT 48011
#define CHUNK_SIZE 5
#define MY_IP "169.254.0.2"

typedef enum { // operation request type
	READ,
	WRITE,
	SEARCH,
	RENAME,
	DELETE
} reqType;

typedef struct nameNodeRequest{ // request sent to data node from name node
	reqType operation; // requested operation
	char directory[30]; // directory of file
	char filename[30]; // name of file
	char newFilename[30]; // optional new filename
	char chunkNo[3]; // requested chunk number
} nameNodeRequest;

int main(void){
	int nameNodeSocket = -1, listenSocket = -1;
	FILE* fp;
	
	char ackPacket[2] = "ok";
	char nakPacket[2] = "no";
	static const nameNodeRequest EmptyStruct; // empty structure used for clearing structs
	
	printf("Creating data node listen socket... "); // creates socket
	if((listenSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1){
		perror("DATA NODE: Error creating listen socket");
		exit(EXIT_FAILURE);
	}
	printf("Created!\n");

	struct sockaddr_in listenSocketAddr; // sets up socket
	memset(&listenSocketAddr, 0, sizeof(listenSocketAddr));
	listenSocketAddr.sin_family = AF_INET;
	listenSocketAddr.sin_port = htons(DATA_NODE_PORT);
	listenSocketAddr.sin_addr.s_addr = inet_addr(MY_IP);
	
	printf("Binding data node listen socket... "); // binds listen socket
	if((bind(listenSocket, (struct sockaddr *) &listenSocketAddr, sizeof(listenSocketAddr))) == -1){
		perror("DATA NODE: Error binding listen socket");
		close(listenSocket);
		exit(EXIT_FAILURE);
	}
	printf("Bound!\n");

	printf("Starting listener for name node connection attempts... ");
	if(listen(listenSocket, 20) == -1){ // starts listening
		perror("DATA NODE: Error listening for incoming connections");
		exit(EXIT_FAILURE);
	}
	printf("Listening.\n");

	struct sockaddr_in nameNodeSocketAddr;
	socklen_t nameNodeSocketAddrLen = sizeof(nameNodeSocketAddr);
	printf("Accepting connections from name node... "); // starts accepting connections
	if((nameNodeSocket = accept(listenSocket, (struct sockaddr *) &nameNodeSocketAddr, &nameNodeSocketAddrLen)) <= 0){
		perror("DATA NODE: Error accepting new connection");
		close(listenSocket);
		exit(EXIT_FAILURE);
	}
	printf("Name node connected!\n");
	
	nameNodeRequest* packet = (nameNodeRequest*)(malloc (sizeof(nameNodeRequest))); // request packet from name node
	char tempBuffer[CHUNK_SIZE]; // buffer for reading and writing
	int success = 1; // used for determining ACKs or NAKs being sent out
	while(1){
		*packet = EmptyStruct; // clears struct
		printf("Waiting for request from name node...");
		if (recv(nameNodeSocket, &(*packet), sizeof(*packet), 0) <= 0){ // receives a packet from the name node
			perror("DATA NODE: Error receiving request packet from name node");
			exit(EXIT_FAILURE); // connection lost, so exit
		}
		printf(" Received packet from name node!\n");
		
		
		struct stat st = {0};
		if (stat(packet->directory, &st) == -1){ // check if directory exists
			mkdir(packet->directory, 0777); // if it doesn't, create it
		}
		if(chdir(packet->directory) == -1){ // changes working directory to specified one
			perror("DATA NODE: Could not change working directory");
			exit(EXIT_FAILURE);
		}
		strcat(packet->filename, packet->chunkNo); // e.g. testfile.wav4
		
		switch(packet->operation){
		case READ:
			printf("Read: Reading chunk %s%s... ", packet->directory, packet->filename);
			if((fp = fopen(packet->filename, "r")) == NULL){ // opens chunk for reading
				perror("READ: File chunk does not exist"); // sends NAK if file chunk does not exist
				strcpy(tempBuffer, "~~");
				printf("Transmitting NAK to name node... ");
				if (send(nameNodeSocket, tempBuffer, sizeof(tempBuffer)/sizeof(char), 0) <= 0){
					perror("READ: Error during data node NAK transmission");
				}
				printf("Sent!\n");
				break;
			}
			memset(&tempBuffer[0], 0, sizeof(tempBuffer));
			fread(tempBuffer, sizeof(tempBuffer), 1, fp); // reads chunk from file
			printf("Read!\n");
			printf("%s\n", tempBuffer);
			printf("Sending chunk %s back to name node... ", packet->chunkNo);
			if (send(nameNodeSocket, tempBuffer, sizeof(tempBuffer)/sizeof(char), 0) <= 0){ // sends chunk back to name node
				perror("READ: Error during chunk transmission from data node");
			}
			printf("Sent!\n");
			fclose(fp);
			break;
		case WRITE:
			success = 1;
			while (success){
				printf("Write: Receiving chunk %s%s... ", packet->directory, packet->filename);
				if((fp = fopen(packet->filename, "w")) == NULL){ // opens file chunk for writing
					perror("WRITE: File chunk could not be created");
					success = 0;
				}
				memset(&tempBuffer[0], 0, sizeof(tempBuffer)); // receives file chunk
				if (recv(nameNodeSocket, tempBuffer, sizeof(tempBuffer)/sizeof(char), 0) <= 0){
					perror("WRITE: Error during data node chunk reception");
					success = 0;
				}
				printf("Received data!\n");
				printf("%s\n", tempBuffer);
				printf("Writing chunk to memory... ");
				if(fprintf(fp, "%s", tempBuffer) < 0){ // write chunk
					perror("WRITE: Error writing to file chunk");
					success = 0;
				}
				printf("Written!\n");
				break;
			}
			if (success){
				printf("Sending ACK back to name node... "); // send ACK to name node
				if (send(nameNodeSocket, ackPacket, sizeof(ackPacket)/sizeof(char), 0) <= 0){
					perror("WRITE: Error during data node ACK transmission");
				}
				printf("Sent!\n");
			}
			else{
				printf("Sending NAK back to name node... "); // send NAK to name node
				if (send(nameNodeSocket, nakPacket, sizeof(nakPacket)/sizeof(char), 0) <= 0){
					perror("WRITE: Error during data node NAK transmission");
				}
				printf("Sent!\n");
			}
			fclose(fp);
			break;
		case RENAME:
			printf("Renaming file %s to %s%s... ", packet->filename, packet->newFilename, packet->chunkNo);
			strcat(packet->newFilename, packet->chunkNo); // sets up filename to rename
			if(rename(packet->filename, packet->newFilename) == 0){ // renames file
				printf("Renamed!\n");
				printf("Sending ACK to name node... "); // sends ACK to name node
				if (send(nameNodeSocket, &ackPacket, sizeof(ackPacket)/sizeof(char), 0) <= 0){
					perror("RENAME: Error during data node ACK transmission");
				}
				printf("Sent!\n");
			}
			else{
				printf("Failed to rename!\n");
				printf("Sending NAK to name node... "); // if it fails, then send NAK to name node
				if (send(nameNodeSocket, &nakPacket, sizeof(nakPacket)/sizeof(char), 0) <= 0){
					perror("RENAME: Error during data node NAK transmission");
				}
				printf("Sent!\n");
			}
			break;
		case DELETE:
			printf("Deleting file %s%s... ", packet->directory, packet->filename); // sets up file to be deleted
			if(remove(packet->filename) == 0){ // deletes file
				printf("Deleted!\n");
				printf("Sending ACK to name node... "); // sends ACK to name node
				if (send(nameNodeSocket, &ackPacket, sizeof(ackPacket)/sizeof(char), 0) <= 0){
					perror("DELETE: Error during data node ACK transmission");
				}
				printf("Sent!\n");
			}
			else{
				printf("Failed to delete!\n");
				printf("Sending NAK to name node... "); // if it fails, then send NAK to name node
				if (send(nameNodeSocket, &nakPacket, sizeof(nakPacket)/sizeof(char), 0) <= 0){
					perror("DELETE: Error during data node NAK transmission");
				}
				printf("Sent!\n");
			}
			break;
		}
	}

	return EXIT_SUCCESS;
}

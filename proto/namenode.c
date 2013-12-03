/*
* This program performs the duties of the NameNode on the distributed file system.
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

#define NUM_OF_CHUNKS 8
#define CHUNK_SIZE 1024

#define MY_IP "169.254.0.2"
#define PERL_PORT 48053

#define NUM_OF_DATA_NODES 1
#define DATA_NODE_IP_1 "169.254.0.1"
#define DATA_NODE_IP_2 "169.254.0.2"
#define DATA_NODE_PORT 48010

#define SL 30

typedef char* string;

typedef enum{
	READ,
	WRITE,
	SEARCH,
	RENAME,
	DELETE
} reqType;

typedef struct nameNodeRequest{
	reqType operation;
	char directory[SL];
	char filename[SL];
	char newFilename[SL];
	char chunkNo[3];;
} nameNodeRequest;

typedef struct perlRequest{
	reqType operation;
	char directory[SL];
	char filename[SL];
	char username[SL];
	char searchTerm[SL];
	char newFilename[SL];
} perlRequest;

typedef struct perlRequestQueue{
	int capacity; // max elements held
	int size; // current size
	int front; // index of first element (where elements are removed)
	int rear; // index of last element (where elements are inserted)
	int filling; // blocking variables to prevent read/write collisions
	int emptying;
	perlRequest* requests; // Perl requests
}perlRequestQueue;


FILE* readFile(char directory[SL], char filename[SL]);
void writeFile(char username[SL], char directory[SL], char filename[SL]);
void searchFile(char directory[SL], char filename[SL], char searchTerm[SL]);
void renameFile(char username[SL], char directory[SL], char filename[SL], char newFilename[SL]);
void deleteFile(string username, char directory[SL], char filename[SL]);

void *perlListener(void* ptr);
int dataNodeConnector(char* dataNodeIP);

nameNodeRequest* createPacket(reqType type, char dr[SL], char fn[SL], char nf[SL], int chunkNo);
perlRequestQueue* createQueue();
perlRequest* Dequeue(perlRequestQueue* Q);
void Enqueue(perlRequestQueue* Q, int op, char un[SL], char dr[SL], char fn[SL], char st[SL], char nf[SL]);


perlRequestQueue* Q; // queue of all requests from user
int perlConnection = 0; // state of connection to Perl program
int perlSocket = -1; // Perl socket descriptor

int sockets[NUM_OF_DATA_NODES]; // array of data node socket descriptors
int numOfSockets = 0; // number of currently connected data nodes

static const nameNodeRequest EmptyStruct; // empty structure used for clearing structs
const char* ackPacket; // ACK and NAK packets
const char* nakPacket;
const char* baseDir; // base directory of filesystem

int main(void){
	perlRequest* request;
	int operation, errFlag;
	char directory[SL];
	FILE* fp;
	ackPacket = "ok";
	nakPacket = "no";
	//baseDir = "/var/www/data/";
	baseDir = "/";
	
	printf("Establishing connection to Perl program...\n");
	pthread_t perlListenThread; // Establish connection to Perl program
	pthread_create(&perlListenThread, NULL, perlListener, NULL);
	while (!perlConnection);
	
	printf("Connecting to data nodes...\n");
	/*while (errFlag != 0){ // wait for connection to be established to data nodes
		errFlag = dataNodeConnector(DATA_NODE_IP_1) + dataNodeConnector(DATA_NODE_IP_2);
	}*/
	dataNodeConnector(DATA_NODE_IP_2);
	
	while(1){ // Main loop -- Dequeue requests as they come in and process them
		if (Q->size != 0){
			printf("New request from Perl\n");
			request = Dequeue(Q);
			if (request->operation == -1){
				perror("NAME NODE: Request queue is full");
			}
			
			memset(&directory[0], 0, sizeof(directory));
			strcpy(directory, baseDir); // change directory
			strcat(directory, request->directory);
			if(chdir(directory) == -1){
				perror("NAME NODE: Error changing working directory");
			}
			
			switch (request->operation){
			case READ:
				readFile(directory, request->filename);
				break;
			case WRITE:
				writeFile(request->username, directory, request->filename);
				break;
			case SEARCH:
				searchFile(directory, request->filename, request->searchTerm);
				break;
			case RENAME:
				renameFile(request->username, directory, request->filename, request->newFilename);
				break;
			case DELETE:
				deleteFile(request->username, directory, request->filename);
				break;
			}
		}
	}
	
	// int err = dataNodeConnector(DATA_NODE_IP_2);
	// if (err == 0){
		// printf("Failed to connect to data node\n");
	// }
	// nameNodeRequest* packet;
	// packet->operation = WRITE;
	// packet->chunkNo = 6;
	// strcpy(packet->directory, "benadsfasdf");
	// strcpy(packet->filename, "wandoadsfasfdf.mp3");
	// send(sockets[0], &(*packet), sizeof(*packet), 0);
	
	return(0);
}

/*
 * This function retrieves and concatenates file chunks from data nodes
*/
FILE* readFile(char directory[SL], char filename[SL]){
	nameNodeRequest* reqPacket = (nameNodeRequest*)(malloc (sizeof(nameNodeRequest))); // packet to request data from data node
	char data[CHUNK_SIZE]; // data from data node
	int i, currSocket = 0, success = 1;
	FILE* fp;
	
	if((fp = fopen(filename, "w")) < 0){ // and open file
		perror("READ: Error opening file on name node");
	}
	printf("Reading file %s%s from data nodes.\n", directory, filename);
	for (i = 0; i < NUM_OF_CHUNKS; i++){ // iterate through each chunk until all are concatenated
		reqPacket = createPacket(READ, directory, filename, "", i);
		printf("Sending read request packet for chunk %d... ", i);
		if (send(sockets[currSocket], &(*reqPacket), sizeof(*reqPacket), 0) <= 0){ // sends request to data node
			perror("READ: Error sending data chunk request to data node");
		}
		printf("Sent!\n");
		
		printf("Receiving file chunk %d from data node %d... ", i, currSocket);
		memset(&data[0], 0, sizeof(data));
		if (recv(sockets[currSocket], data, sizeof(data), 0) <= 0){ // receives data chunk back
			perror("READ: Error while receiving file chunk from data node");
		}
		printf("Received!\n");
		printf("%s\n", data);
		if (data[0] == '~' && data[1] == '~'){ // file chunk reading error on data node side
			success = 0;
			printf("Data chunk %d not found.\nSending read NAK to Perl... ", i);
			if (send(perlSocket, &nakPacket, sizeof(nakPacket)/sizeof(char), 0) <= 0){
				perror("READ: Error during Perl NAK transmission");
			}
			printf("Sent!\nRead failed.\n");
			break;
		}
		printf("Writing chunk %d to file\n", i);
		if(fprintf(fp, "%s", data) < 0){ // concatenates data chunk to file
			perror("READ: Error writing chunk to local file");
		}
		
		currSocket = (currSocket++) % NUM_OF_DATA_NODES; // iterate to next data node for next chunk retrieval
	}
	
	if (success){
		printf("Sending read ACK to Perl... ");
		if (send(perlSocket, &ackPacket, sizeof(ackPacket)/sizeof(char), 0) <= 0){
			perror("READ: Error during Perl ACK transmission");
		}
		printf("Sent!\nFinished reading.\n");
		fclose(fp);
	}
	return fp;
}

/*
 * This function chunks a file and distributes it evenly to each data node
*/
void writeFile(char username[SL], char directory[SL], char filename[SL]){
	char tempBuffer[CHUNK_SIZE]; // temporary reading buffer
	nameNodeRequest* packet; // packet to send to data node
	char ack[2]; // ACK packet from data node
	FILE* fp;
	
	if((fp = fopen(filename, "r")) < 0){ // and open file
		perror("WRITE: Error opening file");
	}
	
	printf("Writing file %s%s to data nodes.\n", directory, filename);
	int currSocket = 0, i;
	for (i = 0; i < NUM_OF_CHUNKS; i++){ // iterate through each chunk until all are concatenated
		printf("Creating chunk %d... ", i);
		memset(&tempBuffer[0], 0, sizeof(tempBuffer));
		fread(tempBuffer, CHUNK_SIZE, 1, fp); // reads chunk from file
		printf("File chunk created!\n");
		
		printf("Sending write request for chunk %d to data node %d... ", i, currSocket);
		packet = createPacket(WRITE, directory, filename, "", i);
		if (send(sockets[currSocket], &(*packet), sizeof(*packet), 0) <= 0){ // sends header packet to data node
			perror("WRITE: Error during header transmission");
		}
		printf("Sent!\n");
		
		printf("Sending data chunk %d to data node %d... ", i, currSocket);
		if (send(sockets[currSocket], &tempBuffer, sizeof(tempBuffer)/sizeof(char), 0) <= 0){ // sends data to data node
			perror("WRITE: Error during data transmission");
		}
		printf("Sent!\n");
		
		printf("Waiting for ACK from data node %d... ", currSocket);
		memset(&ack[0], 0, sizeof(ack));
		if (recv(sockets[currSocket], &ack, sizeof(ack)/sizeof(char), 0) <= 0){ // receives ACK packet from data node
			perror("WRITE: Error while waiting for ACK from data node");
		}
		printf("Received ACK!\n");
		currSocket = (currSocket++) % NUM_OF_DATA_NODES; // iterate to next data node for next chunk retrieval
	}
	
	printf("Adding new file to user's file structure... ");
	char baseDirectory[SL];
	strcpy(baseDirectory, baseDir); // initializes directory prefix
	char per[] = ".";
	char period[SL];
	strcpy(period, per); // initializes period
	
	char* fileStructureFileName = strcat(period, username);
	
	char fileDir[30];
	strcpy(fileDir, baseDirectory);
	if (chdir(strcat(fileDir, username)) < 0){
		perror("WRITE: Error changing directory to user's directory");
	}
	FILE* userFiles;
	if ((userFiles = fopen(fileStructureFileName, "a+")) < 0){ // open user file structure
		perror("WRITE: Error opening user file structure");
	}
	if(fprintf(userFiles, "%s\n", strcat(strcat(baseDirectory, directory), filename)) < 0){ // adds new filename to list of files
		perror("WRITE: Error adding new filename to user file structure");
	}
	fclose(userFiles);
	printf("Added!\n");
	
	printf("Sending ACK to Perl... ");
	if (send(perlSocket, &ackPacket, sizeof(ackPacket)/sizeof(char), 0) <= 0){
		perror("WRITE: Error during Perl ACK transmission");
	}
	printf("Sent!\n");
	
	printf("Deleting temporary local file... ");
	if (remove(filename) != 0){ // Delete file after sending it
		perror("WRITE: File deletion error");
	}
	printf("Deleted!\nFinished writing.\n");
}

/*
 * This function searches a read-in file for a desired string
*/
void searchFile(char directory[SL], char filename[SL], char searchTerm[SL]){
	int lineNum = 0, findResult = 0;
	char temp[512];
	FILE* fp = readFile(directory, searchTerm); // gets file pointer to read-in file
	printf("Searching %s%s for: %s\n", directory, filename, searchTerm);
	while(fgets(temp, 512, fp) != NULL) { // reads in chunks of 512 bytes and searches for target search term
		if((strstr(temp, searchTerm)) != NULL) { // if they are a match, break out and send ACK
			printf("%s was found!\n", searchTerm);
			findResult = 1;
			break;
		}
		lineNum++;
	}

	if(findResult != 1){
		printf("%s was NOT found.\n", searchTerm);
		printf("Sending NAK to Perl... ");
		if (send(perlSocket, &nakPacket, sizeof(nakPacket)/sizeof(char), 0) <= 0){
			perror("SEARCH: Error during Perl NAK transmission");
		}
		printf("Sent!");
	}
	else{
		printf("Sending ACK to Perl... ");
		if (send(perlSocket, &ackPacket, sizeof(ackPacket)/sizeof(char), 0) <= 0){
			perror("SEARCH: Error during Perl ACK transmission");
		}
		printf("Sent!");
	}
}

/*
 * This function renames a file on the distributed file system
*/
void renameFile(char username[SL], char directory[SL], char filename[SL], char newFilename[SL]){
	nameNodeRequest* packet; // packet to send to data node
	char ack[2]; // ACK packet from data node
	
	printf("Renaming file %s%s to %s\n", directory, filename, newFilename);
	int currSocket = 0, i;
	for (i = 0; i < NUM_OF_CHUNKS; i++){ // iterate through each chunk until all are renamed
		packet = createPacket(RENAME, directory, filename, newFilename, i); // sets up request packet
		printf("Sending rename request for chunk %d to data node %d... ", i, currSocket);
		if (send(sockets[currSocket], &(*packet), sizeof(*packet), 0) <= 0){ // sends rename request to data node
			perror("RENAME: Error during rename request transmission to data node");
		}
		printf("Sent!");
		
		printf("Waiting for ACK from data node %d... ", currSocket);
		memset(&ack[0], 0, sizeof(ack));
		if (recv(sockets[currSocket], &ack, sizeof(ack)/sizeof(char), 0) <= 0){ // receives ACK packet from data node
			perror("RENAME: Error while waiting for ACK from data node");
		}
		printf("Received!\n");
		currSocket = (currSocket++) % NUM_OF_DATA_NODES; // iterate to next data node for next chunk retrieval
	}
	
	printf("Modifying user's file structure... ");
	char baseDirectory[SL];
	strcpy(baseDirectory, baseDir); // initializes directory prefix
	char per[] = ".";
	char period[SL];
	strcpy(period, per); // initializes period
	
	char* fileStructureFileName = strcat(period, username);
	
	char fileDir[30];
	strcpy(fileDir, baseDirectory);
	if(chdir(strcat(fileDir, username)) < 0){
		perror("RENAME: Error changing directory to user's directory");
	}
	FILE* oldUserFiles;
	if ((oldUserFiles = fopen(fileStructureFileName, "a+")) < 0){ // open user file structure
		perror("RENAME: Error opening old file structure file");
	}
	FILE* newUserFiles;
	if ((newUserFiles = fopen("tempfile.txt", "w")) < 0){ // open new file for copying over to
		perror("RENAME: Error opening new user file structure file");
	}
	if (fprintf(newUserFiles, "%s\n", strcat(strcat(baseDirectory, directory), newFilename)) < 0){ // adds new filename to list of files
		perror("RENAME: Error adding new filename to new user file structure file");
	}
	
	char temp[512];
	char* strLoc;
	while(fgets(temp, 512, oldUserFiles) != NULL) { // reads in chunks of 512 bytes and searches for old filename
		if((strLoc = strstr(temp, filename)) == NULL){ // copy everything but old filename
			if(fprintf(newUserFiles, "%s", temp) < 0){
				perror("RENAME: Error copying over file structure");
			}
		}
	}
	
	if (remove(fileStructureFileName) < 0){ // finishes file rename by overwriting old file structure
		perror("RENAME: Error deleting old file structure");
	}
	if (rename("tempfile.txt", fileStructureFileName) != 0){
		perror("RENAME: Error renaming new file structure to original name");
	}
	
	fclose(oldUserFiles);
	fclose(newUserFiles);
	printf("Modified!\n");
	
	printf("Sending ACK to Perl... ");
	if (send(perlSocket, &ackPacket, sizeof(ackPacket)/sizeof(char), 0) <= 0){
		perror("RENAME: Error during Perl ACK transmission");
	}
	printf("Sent!\nFinished renaming.\n");
}

/*
 * This function deletes a file from the distributed file system
*/
void deleteFile(string username, char directory[SL], char filename[SL]){
	nameNodeRequest* packet; // packet to send to data node
	char ack[2]; // ACK packet from data node
	
	printf("Deleting file %s%s\n", directory, filename);
	int currSocket = 0, i;
	for (i = 0; i < NUM_OF_CHUNKS; i++){ // iterate through each chunk until all are deleted
		printf("Requesting deletion of file chunk %d on data node %d... ", i, currSocket);
		packet = createPacket(DELETE, directory, filename, NULL, i); // sets up request packet
		if (send(sockets[currSocket], &(*packet), sizeof(*packet), 0) <= 0){ // sends delete request to data node
			perror("DELETE: Error during delete request transmission to data node");
		}
		printf("Sent!\n");
		
		printf("Waiting for ACK from data node %d... ", currSocket);
		memset(&ack[0], 0, sizeof(ack));
		if (recv(sockets[currSocket], &ack, sizeof(ack)/sizeof(char), 0) <= 0){ // receives ACK packet from data node
			perror("DELETE: Error while waiting for ACK from data node");
		}
		printf("Received!\n");
		currSocket = (currSocket++) % NUM_OF_DATA_NODES; // iterate to next data node for next chunk retrieval
	}
	
	printf("Removing file from user's file structure... \n");
	char baseDirectory[SL];
	strcpy(baseDirectory, baseDir); // initializes directory prefix
	char per[] = ".";
	char period[SL];
	strcpy(period, per); // initializes period
	
	char* fileStructureFileName = strcat(period, username);
	
	chdir(strcat(baseDirectory, username));
	FILE* oldUserFiles;
	if ((oldUserFiles = fopen(fileStructureFileName, "a+")) < 0){ // open user file structure
		perror("DELETE: Error opening old user file structure file");
	}
	FILE* newUserFiles;
	if ((newUserFiles = fopen("tempfile.txt", "w")) < 0){ // open new file for copying over to
		perror("DELETE: Error opening new user file structure file");
	}
	
	char temp[512];
	char* strLoc;
	while(fgets(temp, 512, oldUserFiles) != NULL) { // reads in chunks of 512 bytes and searches for old filename
		if((strLoc = strstr(temp, filename)) == NULL){ // copy everything but old filename
			if (fprintf(newUserFiles, "%s", temp) < 0){
				perror("DELETE: Error copying over old file structure");
			}
		}
	}
	
	if (remove(fileStructureFileName) < 0){ // finishes file deletion by overwriting old file structure
		perror("DELETE: Error removing old file structure file");
	}
	if (rename("tempfile.txt", fileStructureFileName) != 0){
		perror("DELETE Error renaming new file structure file to original name");
	}
	
	fclose(oldUserFiles);
	fclose(newUserFiles);
	printf("Removed!\n");
	
	printf("Sending ACK to Perl... ");
	if (send(perlSocket, &ackPacket, sizeof(ackPacket)/sizeof(char), 0) <= 0){
		perror("DELETE: Error during Perl ACK transmission");
	}
	printf("Sent!\nFinished deleting.\n");
}

/*
 * This function creates a packet for sending to data nodes
*/
nameNodeRequest* createPacket(reqType type, char dr[SL], char fn[SL], char nf[SL], int chunkNo){
	nameNodeRequest* packet = (nameNodeRequest*)(malloc (sizeof(nameNodeRequest)));
	*packet = EmptyStruct; // sets up header packet
	strcpy(packet->directory, dr);
	strcpy(packet->filename, fn);
	strcpy(packet->newFilename, nf);
	sprintf(packet->chunkNo, "%d", chunkNo);
	packet->operation = type;
	
	return packet;
}

/*
 * This thread continuously listens for file operation requests from the user via the Perl program
*/
void *perlListener(void *ptr){
	unsigned char reqPacket[200]; // request from Perl
	int listenSocket = -1;
	
	Q = createQueue(); // initializes request queue
	while(1){
		printf("Creating Perl listener socket... ");
		if((listenSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1){ // creates listen socket
			perror("PERL LISTENER: Error creating socket");
		}
		printf("Created!\n");

		struct sockaddr_in listenSocketAddr; // sets up the listen socket
		memset(&listenSocketAddr, 0, sizeof(listenSocketAddr));
		listenSocketAddr.sin_family = AF_INET;
		listenSocketAddr.sin_port   = htons(PERL_PORT);
		listenSocketAddr.sin_addr.s_addr = inet_addr(MY_IP);

		printf("Binding Perl listener socket... ");
		if(bind(listenSocket, (struct sockaddr *) &listenSocketAddr, sizeof(listenSocketAddr)) == -1){ // binds listen socket
			perror("PERL LISTENER: Error binding socket");
			close(listenSocket);
			exit(EXIT_FAILURE);
		}
		printf("Bound!\n");

		printf("Starting listener for Perl connection attempts... ");
		if(listen(listenSocket, 20) == -1){ // listens for connection attempts
			perror("PERL LISTENER: Error listening for connection attempts");
		}
		printf("Listening.\n");
		
		while(1){
			if (!perlConnection){
				printf("Accepting incoming Perl connections... ");
				struct sockaddr_in perlSocketAddr;
				socklen_t perlSocketAddrLen = sizeof(perlSocketAddr);
				if((perlSocket = accept(listenSocket, (struct sockaddr *) &perlSocketAddr, &perlSocketAddrLen)) < 0){ // accepts connections from Perl program
					perror("PERL LISTENER: Error accepting Perl connection");
					close(listenSocket);
					exit(EXIT_FAILURE);
				}
				else{
					perlConnection = 1;
					printf("Connected to Perl program!\n");
				}
			}
		
			memset(&reqPacket[0], 0, sizeof(reqPacket));
			if (recv(perlSocket, reqPacket, sizeof(reqPacket), 0) <= 0){ // continuously listens for data from Perl
				perror("PERL LISTENER: Error while receiving data from Perl");
			}
			printf("Perl listener received new Perl request!\nAdding it to queue... ");
			
			Enqueue(Q, atoi(strtok(reqPacket,"\n")), strtok(NULL,"\n"), strtok(NULL,"\n"), strtok(NULL,"\n"), strtok(NULL,"\n"), strtok(NULL,"\n")); // data received is line-delimited
			printf("Enqueued!\n");
		}
	}
}

/*
 * This function connects to a data node
*/
int dataNodeConnector(char *dataNodeIP){ // Connects to data node and returns socket descriptor for that data node
	int connectSocket = -1; // socket for connections
	printf("Connecting to data node with IP %s.\n", dataNodeIP);
	
	printf("Creating data node socket... ");
	if ((connectSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) <= 0){
		perror("DATA NODE CONNECTION: Error creating socket");
	}
	printf("Created!\n");
	
	struct sockaddr_in dataNodeAddr;
	memset(&dataNodeAddr, 0, sizeof(dataNodeAddr)); // reset dataNodeAddr to all 0's
	dataNodeAddr.sin_family = AF_INET; // sets address family to AF_INET
	dataNodeAddr.sin_addr.s_addr = inet_addr(dataNodeIP); // sets IP address to data node IP
	dataNodeAddr.sin_port = htons(DATA_NODE_PORT); // converts DATA_NODE_PORT to TCP/IP network byte order and sets it
	
	printf("Connecting to data node... ");
	if (connect(connectSocket, (struct sockaddr *) &dataNodeAddr, sizeof(dataNodeAddr)) < 0){
		perror("DATA NODE CONNECTION: Error connecting to data node");
		return 0;
	}
	printf("Connected to data node %s!\n", dataNodeIP);
	sockets[numOfSockets] = connectSocket; // add data node socket to array of data node sockets
	numOfSockets++; // increment index
	return 1;
}



/*
 * This function initializes a new queue
*/
perlRequestQueue* createQueue(){
	perlRequestQueue* Q = (perlRequestQueue*)(malloc (sizeof(perlRequestQueue)));
	Q->size = 0;
	Q->capacity = 20;
	Q->front = 0;
	Q->rear = -1;
	Q->filling = 0;
	Q->emptying = 0;
	Q->requests = (perlRequest*)(malloc (SL*sizeof(perlRequest)));
	return Q;
}

/*
 * This function pops a Perl request from the queue
*/
perlRequest* Dequeue(perlRequestQueue *Q){
	perlRequest* returned;

	if(Q->size==0){ // If perlRequestQueue size is zero then it is empty
		returned->operation = -1;
		printf("perlRequestQueue is Empty\n");
		return returned;
	}
	else{ // Not empty, so remove the front element
		while (Q->filling){}
		Q->emptying = 1;
		returned = &Q->requests[Q->front];
		Q->size--;
		Q->front++;
		if(Q->front==Q->capacity){
			Q->front=0;
		}
		Q->emptying = 0;
	}
	return returned;
}

/*
 * This function pushes a Perl request to the queue
*/
void Enqueue(perlRequestQueue* Q, int op, char dr[SL], char fn[SL], char un[SL], char st[SL], char nf[SL]){
	if(Q->size == Q->capacity){ // this queue is full
		printf("perlRequestQueue is Full\n");
	}
	else{
		printf("Enqueuing %d, %s, %s, %s, %s, %s... ", op, dr, fn, un, st, nf);
		while (Q->emptying);
		Q->filling = 1;
		Q->size++;
		Q->rear = Q->rear + 1;
		if(Q->rear == Q->capacity){
			Q->rear = 0;
		}
		perlRequest* temp = (perlRequest*)(malloc (sizeof(perlRequest)));
		switch(op){
		case 0:
			temp->operation = READ;
			break;
		case 1:
			temp->operation = WRITE;
			break;
		case 2:
			temp->operation = SEARCH;
			break;
		case 3:
			temp->operation = RENAME;
			break;
		case 4:
			temp->operation = DELETE;
			break;
		}
		strcpy(temp->directory, dr); // inserts element at rear of queue
		strcpy(temp->filename, fn);
		strcpy(temp->username, un);
		strcpy(temp->searchTerm, st);
		strcpy(temp->newFilename, nf);
		Q->requests[Q->rear] = *temp;
		Q->filling = 0;
	}
	return;
}

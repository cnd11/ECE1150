/*
* This program performs the duties of the NameNode on the distributed file system.
* Written by Ben Kisley, 11/1/13
*/

// TODO: .BOB file structure
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

#define MY_IP "169.254.0.1"
#define PERL_PORT 48001

#define NUM_OF_DATA_NODES 2
#define DATA_NODE_IP_1 "169.254.0.1"
#define DATA_NODE_IP_2 "169.254.0.2"
#define DATA_NODE_PORT 48007

typedef char* string;

typedef enum{
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

typedef struct perlRequest{
	reqType operation;
	string directory;
	string filename;
	string searchTerm;
} perlRequest;

typedef struct perlRequestQueue{
	int capacity; // max elements held
	int size; // current size
	int front; // index of first element (where elements are removed)
	int rear; // index of last element (where elements are inserted)
	int filling; // blocking variables to prevent read/write collisions
	int emptying;
	perlRequest requests[30]; // Perl requests
}perlRequestQueue;


FILE* readFile(FILE* fp, string directory, string filename);
void writeFile(FILE* fp, string directory, string filename);
void searchFile(FILE* fp, string directory, string filename, string searchTerm);
void *perlListener(void* ptr);
int dataNodeConnector(char* dataNodeIP);
perlRequestQueue* createQueue();
perlRequest* Dequeue(perlRequestQueue* Q);
void Enqueue(perlRequestQueue* Q, int op, string dr, string fn, string st);


perlRequestQueue* Q; // queue of all requests from user
int perlConnection = 0; // state of connection to Perl program
int perlSocket = -1; // Perl socket descriptor

int sockets[NUM_OF_DATA_NODES]; // array of data node socket descriptors
int numOfSockets = 0; // number of currently connected data nodes

static const nameNodeRequest EmptyStruct; // empty structure used for clearing structs
const char* ackPacket; // ACK and NAK packets
const char* nakPacket;

int main(void){
	perlRequest* request;
	int operation, errFlag;
	char directory[40];
	FILE* fp;
	ackPacket = "ok";
	nakPacket = "no";
	
	pthread_t perlListenThread; // Establish connection to Perl program
	pthread_create(&perlListenThread, NULL, perlListener, NULL);
	while (!perlConnection);
	
	while (errFlag != 0){ // wait for connection to be established to data nodes
		errFlag = dataNodeConnector(DATA_NODE_IP_1) + dataNodeConnector(DATA_NODE_IP_2);
	}
	
	while(1){ // Main loop -- Dequeue requests as they come in and process them
		if (Q->size != 0){
			request = Dequeue(Q);
			if (request->operation == -1){
				perror("MAIN: Error: Request queue is full");
			}

			memset(&directory[0], 0, sizeof(directory));
			strcpy(directory, "/var/www/data/"); // change directory
			strcat(directory, request->directory);
			chdir(directory);
			fp = fopen(request->filename, "ab+"); // and open file
			
			switch (request->operation){
			case READ:
				readFile(fp, directory, request->filename);
				break;
			case WRITE:
				writeFile(fp, directory, request->filename);
				break;
			case SEARCH:
				searchFile(fp, directory, request->filename, request->searchTerm);
				break;
			}
			fclose(fp);
		}
	}
	
	// int err = dataNodeConnector(DATA_NODE_IP_2);
	// if (err == 0){
		// printf("Failed to connect to data node\n");
	// }
	// nameNodeRequest* packet = (nameNodeRequest*)malloc(sizeof(nameNodeRequest));
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
FILE* readFile(FILE* fp, string directory, string filename){
	nameNodeRequest* reqPacket; // packet to request data from data node
	char dataPacket[CHUNK_SIZE]; // data from data node
	int i, currSocket = 0;
	
	for (i = 0; i < NUM_OF_CHUNKS; i++){ // iterate through each chunk until all are concatenated
		*reqPacket = EmptyStruct; // sets up request packet
		reqPacket->operation = READ;
		strcpy(reqPacket->directory, directory);
		strcpy(reqPacket->filename, filename);
		reqPacket->chunkNo = (char)(((int)'0')+i);
		if (send(sockets[currSocket], &(*reqPacket), sizeof(*reqPacket), 0) <= 0){ // sends request to data node
			perror("READ: Error sending data chunk request to data node");
		}
		
		memset(&dataPacket[0], 0, sizeof(dataPacket));
		if (recv(sockets[currSocket], dataPacket, sizeof(dataPacket), 0) <= 0){ // receives data chunk back
			perror("READ: Error while waiting for file chunk from data node");
		}
		fprintf(fp, "%s", dataPacket); // concatenates data chunk to file
		
		currSocket = (currSocket++) % NUM_OF_DATA_NODES; // iterate to next data node for next chunk retrieval
	}

	if (send(perlSocket, &ackPacket, sizeof(ackPacket)/sizeof(char), 0) <= 0){
		perror("READ: Error during Perl ACK transmission");
	}
	
	if (remove(filename) != 0){ // Delete file after sending it
		perror("READ: File deletion error");
	}

	return fp;
}

/*
 * This function chunks a file and distributes it evenly to each data node
*/
void writeFile(FILE* fp, string directory, string filename){
	char tempBuffer[CHUNK_SIZE]; // temporary reading buffer
	nameNodeRequest* packet; // packet to send to data node
	char ackPacket[2]; // ACK packet from data node
	
	int currSocket = 0, i;
	for (i = 0; i < NUM_OF_CHUNKS; i++){ // iterate through each chunk until all are concatenated
		if (fread(tempBuffer, CHUNK_SIZE, 1, fp) != CHUNK_SIZE){ // reads chunk from file
			perror("WRITE: Error during file reading on name node");
		}
		
		*packet = EmptyStruct; // sets up header packet
		packet->operation = WRITE;
		strcpy(packet->directory, directory);
		strcpy(packet->filename, filename);
		packet->chunkNo = (char)(((int)'0')+i);
		if (send(sockets[currSocket], &(*packet), sizeof(*packet), 0) <= 0){ // sends header packet to data node
			perror("WRITE: Error during header transmission");
		}
		
		memset(&tempBuffer[0], 0, sizeof(tempBuffer));
		if (send(sockets[currSocket], &tempBuffer, sizeof(tempBuffer)/sizeof(char), 0) <= 0){ // sends data to data node
			perror("WRITE: Error during data transmission");
		}
		
		memset(&ackPacket[0], 0, sizeof(ackPacket));
		if (recv(sockets[currSocket], &ackPacket, sizeof(ackPacket)/sizeof(char), 0) <= 0){ // receives ACK packet from data node
			perror("WRITE: Error while waiting for ACK from data node");
		}
		currSocket = (currSocket++) % NUM_OF_DATA_NODES; // iterate to next data node for next chunk retrieval
	}
	
	if (send(perlSocket, &ackPacket, sizeof(ackPacket)/sizeof(char), 0) <= 0){
		perror("WRITE: Error during Perl ACK transmission");
	}
	
	if (remove(filename) != 0){ // Delete file after sending it
		perror("WRITE: File deletion error");
	}
}

/*
 * This function searches a read-in file for a desired string
*/
void searchFile(FILE* fp, string directory, string filename, string searchTerm){
	int lineNum = 0, findResult = 0;
	char temp[512];
	FILE* newFp = readFile(fp, directory, searchTerm); // gets file pointer to read-in file
	while(fgets(temp, 512, newFp) != NULL) { // reads in chunks of 512 bytes and searches for target search term
		if((strstr(temp, searchTerm)) != NULL) { // if they are a match, break out and send ACK
			findResult = 1;
			break;
		}
		lineNum++;
	}

	if(findResult != 1){
		if (send(perlSocket, &nakPacket, sizeof(nakPacket)/sizeof(char), 0) <= 0){
			perror("SEARCH: Error during Perl NAK transmission");
		}
	}
	else{
		if (send(perlSocket, &ackPacket, sizeof(ackPacket)/sizeof(char), 0) <= 0){
			perror("SEARCH: Error during Perl ACK transmission");
		}
	}
}

/*
 * This thread continuously listens for file operation requests from the user via the Perl program
*/
void *perlListener(void *ptr){
	unsigned char reqPacket[48]; // request from Perl
	int listenSocket = -1;
	
	Q = createQueue(); // initializes request queue
	while(1){
		if(listenSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP) == -1){ // creates listen socket
			perror("PERL LISTENER: Error creating socket");
			exit(EXIT_FAILURE);
		}

		struct sockaddr_in listenSocketAddr; // sets up the listen socket
		memset(&listenSocketAddr, 0, sizeof(listenSocketAddr));
		listenSocketAddr.sin_family = AF_INET;
		listenSocketAddr.sin_port   = htons(PERL_PORT);
		listenSocketAddr.sin_addr.s_addr = inet_addr(MY_IP);

		if(bind(listenSocket, (struct sockaddr *) &listenSocketAddr, sizeof(listenSocketAddr)) == -1){ // binds listen socket
			perror("PERL LISTENER: Error binding socket");
			close(listenSocket);
			exit(EXIT_FAILURE);
		}

		if(listen(listenSocket, 20) == -1){ // listens for connection attempts
			perror("PERL LISTENER: Error listening for connection attempts");
		}
		
		while(1){
			struct sockaddr_in perlSocketAddr;
			socklen_t perlSocketAddrLen = sizeof(perlSocketAddr);
			if(perlSocket = accept(listenSocket, (struct sockaddr *) &perlSocketAddr, &perlSocketAddrLen) < 0){ // accepts connections from Perl program
				perror("PERL LISTENER: Error accepting Perl connection");
				close(listenSocket);
				exit(EXIT_FAILURE);
			}
			else if(!perlConnection){
				perlConnection = 1;
				printf("Connection to Perl successful!\n");
			}
		
			memset(&reqPacket[0], 0, sizeof(reqPacket));
			if (recv(perlSocket, reqPacket, sizeof(reqPacket), 0) <= 0){ // continuously listens for data from Perl
				perror("PERL LISTENER: Error while receiving data from Perl");
			}
			Enqueue(Q, atoi(strtok(reqPacket,"\n")), strtok(reqPacket,"\n"), strtok(reqPacket,"\n"), strtok(reqPacket,"\n")); // data received is line-delimited
		}
	}
}

/*
 * This function connects to a data node
*/
int dataNodeConnector(char *dataNodeIP){ // Connects to data node and returns socket descriptor for that data node
	int connectSocket = -1; // socket for connections
	
	if (connectSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP) <= 0){
		perror("DATA NODE CONNECTION: Error creating socket error");
	}
	
	struct sockaddr_in dataNodeAddr;
	memset(&dataNodeAddr, 0, sizeof(dataNodeAddr)); // reset dataNodeAddr to all 0's
	dataNodeAddr.sin_family = AF_INET; // sets address family to AF_INET
	dataNodeAddr.sin_addr.s_addr = inet_addr(dataNodeIP); // sets IP address to data node IP
	dataNodeAddr.sin_port = htons(DATA_NODE_PORT); // converts DATA_NODE_PORT to TCP/IP network byte order and sets it
	
	if (connect(connectSocket, (struct sockaddr *) &dataNodeAddr, sizeof(dataNodeAddr)) <= 0){
		perror("DATA NODE CONNECTION: Error connecting to data node\n");
		return 0;
	}
	sockets[numOfSockets] = connectSocket; // add data node socket to array of data node sockets
	numOfSockets++; // increment index
	printf("DATA NODE CONNECTION: Success connecting to data node!\n");
	return 1;
}



/*
 * This function initializes a new queue
*/
perlRequestQueue* createQueue(){
	perlRequestQueue *Q;
	Q = (perlRequestQueue *)malloc(sizeof(perlRequestQueue));
	Q->size = 0;
	Q->capacity = 20;
	Q->front = 0;
	Q->rear = -1;
	Q->filling = 0;
	Q->emptying = 0;
	return Q;
}

/*
 * This function pops a Perl request from the queue
*/
perlRequest* Dequeue(perlRequestQueue *Q){
	perlRequest* returned;
	returned = (perlRequest*)(malloc(sizeof(perlRequest)));

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
void Enqueue(perlRequestQueue* Q, int op, string dr, string fn, string st){
	if(Q->size == Q->capacity){ // this queue is full
		printf("perlRequestQueue is Full\n");
	}
	else{
		while (Q->emptying);
		Q->filling = 1;
		Q->size++;
		Q->rear = Q->rear + 1;
		if(Q->rear == Q->capacity){
			Q->rear = 0;
		}
		perlRequest* temp;
		temp = (perlRequest*)(malloc(sizeof(perlRequest)));
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
		}
		temp->directory = dr; // inserts element at rear of queue
		temp->filename = fn;
		temp->searchTerm = st;
		Q->requests[Q->rear] = *temp;
		Q->filling = 0;
	}
	return;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define PORT_NUM 10004
pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;
void error(const char *msg)
{
	perror(msg);
	exit(1);
}

typedef struct _USR {
	int clisockfd;		// socket file descriptor
	char *username;		// user name
	char ip_address[32];	// IPv4 address of user
	struct _USR* next;	// for linked list queue
} USR;

USR *head = NULL;
USR *tail = NULL;

void print_users(){
	USR *cur = head;
	if (cur == NULL){
		printf("NO ONE IN CHAT ROOM.\n");
	}
	else{
		printf("CONNECTED USERS:\n");
		while(cur != NULL){
			printf("USER: %s(%s) %d\n", cur -> username, cur -> ip_address, cur -> clisockfd);
			cur = cur -> next;
		}
	}
}

void broadcast_join(int fromfd, char* user_name, char* ip_address){
	//char buffer[512];
	//snprintf(buffer, sizeof(buffer), "%s (%s) has joined the chat!\n", user_name, ip_address);
	//for (USR* cur = head; cur != NULL; cur = cur -> next){
	USR* cur = head;
	while (cur != NULL)
	{
		if(cur -> clisockfd != fromfd){
			char buffer[512];
			snprintf(buffer, sizeof(buffer), "%s (%s) has joined the chat!\n", user_name, ip_address);
			printf("%s", buffer);
			fflush(stdout);
			long bytes_sent = send(cur -> clisockfd, buffer, sizeof(buffer), 0);
			//printf("adpepf: %ld", bytes_sent);
			if(bytes_sent != sizeof(buffer)){
				error("ERROR send() failed");
			} else{
				printf("%s\n", "sent data");
			}
		}
		cur = cur -> next;
	}
}

void broadcast(int fromfd, char* message)
{
	// figure out sender address
	struct sockaddr_in cliaddr;
	socklen_t clen = sizeof(cliaddr);
	if (getpeername(fromfd, (struct sockaddr*)&cliaddr, &clen) < 0) error("ERROR Unknown sender!");

	// traverse through all connected clients
	USR* cur = head;
	char *username = NULL;
	while (cur != NULL){
		if(cur -> clisockfd == fromfd){
			username = strdup(cur -> username);
			break;
		}
		cur = cur -> next;
	}

	cur = head;
	while (cur != NULL) {
		// check if cur is not the one who sent the message
		if (cur->clisockfd != fromfd) {
			char buffer[512];

			// prepare message
			sprintf(buffer, "[%s(%s)]:%s\n", username, inet_ntoa(cliaddr.sin_addr), message);
			//printf("%s",buffer);
			fflush(stdout);
			int nmsg = strlen(buffer);

			// send!
			int nsen = send(cur->clisockfd, buffer, nmsg, 0);
			if (nsen != nmsg) error("ERROR send() failed");
		}

		cur = cur->next;
	}
}

void add_tail(int newclisockfd, char* username, char ip_address[32])
{
	//printf("In add tail\n");
	if (head == NULL) {
		head = (USR*) malloc(sizeof(USR));
		head->clisockfd = newclisockfd;
		head->username = strdup(username); 
		strncpy(head->ip_address, ip_address, 32); // Safely copy the IP address
    	head->ip_address[31] = '\0'; // Ensure null-termination
		head->next = NULL;
		tail = head;
	} else {
		tail->next = (USR*) malloc(sizeof(USR));
		tail->next->clisockfd = newclisockfd;
		tail->next->username = strdup(username);
		strncpy(tail->next->ip_address, ip_address, 32); // Safely copy the IP address
    	tail->next->ip_address[31] = '\0';
		tail->next->next = NULL;
		tail = tail->next;
	}
	
	//printf("BROADCASTING\n");	
	//printf("FINISHED BROADCASTING\n");
	print_users();
}

typedef struct _ThreadArgs {
	int clisockfd;
} ThreadArgs;

void* thread_main(void* args)
{
	// make sure thread resources are deallocated upon return
	pthread_detach(pthread_self());

	// get socket descriptor from argument
	int clisockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	//-------------------------------
	// Now, we receive/send messages
	char buffer[256];
	int nsen, nrcv;

	nrcv = recv(clisockfd, buffer, 255, 0);
	if (nrcv < 0) error("ERROR recv() failed");

	while (nrcv > 0) {
		// we send the message to everyone except the sender
		broadcast(clisockfd, buffer);

		nrcv = recv(clisockfd, buffer, 255, 0);
		if (nrcv < 0) error("ERROR recv() failed");
	}

	close(clisockfd);
	//-------------------------------

	return NULL;
}

int main(int argc, char *argv[])
{
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");

	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;	
	//serv_addr.sin_addr.s_addr = inet_addr("192.168.1.171");	
	serv_addr.sin_port = htons(PORT_NUM);

	int status = bind(sockfd, 
			(struct sockaddr*) &serv_addr, slen);
	if (status < 0) error("ERROR on binding");

	listen(sockfd, 5); // maximum number of connections = 5

	while(1) {
		//printf("At the top\n");

		struct sockaddr_in cli_addr;
		socklen_t clen = sizeof(cli_addr);
		int newsockfd = accept(sockfd, 
			(struct sockaddr *) &cli_addr, &clen);
		if (newsockfd < 0) error("ERROR on accept");
		//Get the username from the client
		char username_ip[256];
		int nrecv = recv(newsockfd, username_ip, sizeof(username_ip) - 1, 0);
		if(nrecv < 0) error("ERROR recv() failed");
		username_ip[nrecv] = '\0';
		//get the username and client IP////
		int i = 0;
		int j = 0;
		while(username_ip[i] != '\0'){
			if(username_ip[i] != '\n'){
				username_ip[j++] = username_ip[i];
			}
			i++;
		}
		username_ip[j] = '\0';
		char* tok = strtok(username_ip, "|");
		char* username_ip_sep[2];
		int pos = 0;
		while(tok != NULL){
			printf("%s\n", tok);
			username_ip_sep[pos] = tok;
			tok = strtok(NULL, "|");
			pos++;
		}

		add_tail(newsockfd, username_ip_sep[0], username_ip_sep[1]); // add this new client to the client list
		broadcast_join(newsockfd, username_ip_sep[0], username_ip_sep[1]); 

		// prepare ThreadArgs structure to pass client socket
		ThreadArgs* args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
		if (args == NULL) error("ERROR creating thread argument");
		
		args->clisockfd = newsockfd;

		pthread_t tid;
		if (pthread_create(&tid, NULL, thread_main, (void*) args) != 0) error("ERROR creating a new thread");
	}

	return 0; 
}


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h> 
#include <pthread.h>

#define PORT_NUM 10004

void error(const char *msg)
{
  perror(msg);
  exit(0);
}

typedef struct _ThreadArgs {
  int clisockfd;
} ThreadArgs;

void* thread_main_recv(void* args)
{
  pthread_detach(pthread_self());

  int sockfd = ((ThreadArgs*) args)->clisockfd;
  free(args);

  // keep receiving and displaying message from server
  char buffer[512];
  int n;
  memset(buffer, 0, 512);
  n = recv(sockfd, buffer, 512, 0);
  while (n > 0) {
    memset(buffer, 0, 512);
    n = recv(sockfd, buffer, 512, 0);
    if (n < 0) error("ERROR recv() failed");

    printf("\n%s\n", buffer);
  }

  return NULL;
}

void* thread_main_send(void* args)
{
  pthread_detach(pthread_self());

  int sockfd = ((ThreadArgs*) args)->clisockfd;
  free(args);

  // keep sending messages to the server
  char buffer[256];
  int n;

  while (1) {
    // You will need a bit of control on your terminal
    // console or GUI to have a nice input window.
    //printf("\nPlease enter the message: ");
    memset(buffer, 0, 256);
    fgets(buffer, 255, stdin);
    
    if (strlen(buffer) == 1) buffer[0] = '\0';

    n = send(sockfd, buffer, strlen(buffer), 0);
    if (n < 0) error("ERROR writing to socket");

    if (n == 0) break; // we stop transmission when user type empty string
  }

  return NULL;
}

int main(int argc, char *argv[])
{
  if (argc < 2) error("Please specify hostname");

  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) error("ERROR opening socket");

  
  char username[256];
  printf("Enter your username: ");
  fgets(username, sizeof(username), stdin);

  struct sockaddr_in serv_addr;
  socklen_t slen = sizeof(serv_addr);
  memset((char*) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
  serv_addr.sin_port = htons(PORT_NUM);

  printf("Try connecting to %s...\n", inet_ntoa(serv_addr.sin_addr));

  int status = connect(sockfd, 
      (struct sockaddr *) &serv_addr, slen);
  if (status < 0) error("ERROR connecting");
  
  printf("Successfully connected to %s...\n", inet_ntoa(serv_addr.sin_addr));
  struct sockaddr_in localAddress;
  printf("Fail1\n");
  socklen_t addrLen = sizeof(localAddress);
  printf("Fail2\n");
  memset(&localAddress, 0, addrLen);
  printf("Fail3\n");
  localAddress.sin_family = AF_INET;
  localAddress.sin_addr.s_addr = INADDR_ANY;
  localAddress.sin_port = 0;  

  bind(sockfd, (struct sockaddr *) &localAddress, addrLen);
   getsockname(sockfd, (struct sockaddr*)&localAddress, &addrLen);
  char ip_address[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &(localAddress.sin_addr), ip_address, INET_ADDRSTRLEN);

  // Send the username and IP address to the server
  char send_data[512];
  snprintf(send_data, sizeof(send_data), "%s|%s\n", username, ip_address);
  send(sockfd, send_data, strlen(send_data), 0);


  pthread_t tid1;
  pthread_t tid2;

  ThreadArgs* args;

  args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
  args->clisockfd = sockfd;
  pthread_create(&tid1, NULL, thread_main_send, (void*) args);

  args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
  args->clisockfd = sockfd;
  pthread_create(&tid2, NULL, thread_main_recv, (void*) args);

  // parent will wait for sender to finish (= user stop sending message and disconnect from server)
  pthread_join(tid1, NULL);

  close(sockfd);

  return 0;
}



/* A simple server in the internet domain using TCP
   The port number is passed as an argument */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

void error(const char *msg)
{
   perror(msg);
   exit(1);
}

void append_timestamp(char *str)
{
    char temp[256];
    char buf[1024];

    struct timeval tv;
    gettimeofday(&tv,NULL);
    long double timestamp = (long double)tv.tv_sec + (long double)tv.tv_usec/1000000.0;
    printf("Saved Timestamp: %Lf\n", timestamp);

    snprintf(temp, 512, "%Lf", timestamp);

    strcat(str, ",");
    strcat(str, temp);
    return;
}

void send_to_sarm(int from_socket, char *message)
{
    int n;
    n = 0;

    printf("Sending...\n");
    n = send(from_socket, message, strlen(message), 0);
    
    printf("%s has been sent!\n", message);
    if (n < 0)
      error("ERROR in sendto");
}

void init_gateway_server()
{
    return;
}

void connect_to_sarm(int *sockfd, char *hostname, int port, struct sockaddr_in *server_addr)
{

    *sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    struct hostent *server;
    server = gethostbyname(hostname);
    if (server == NULL) 
    {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    bzero((char *) server_addr, sizeof(*server_addr));
    server_addr->sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&(server_addr->sin_addr.s_addr), server->h_length);
    server_addr->sin_port = htons(port);

    printf("Attempting to connect...\n");
    if (connect(*sockfd, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0)
    {
        perror("Connect failed. Error");
        return;
    }

    puts("Connected\n");
}

int main(int argc, char *argv[])
{
    int sockfd, sarm_sockfd, newsockfd, portno, sarm_portno;
    socklen_t clilen;
    char buffer[1024];
    struct sockaddr_in serv_addr, cli_addr, sarm_addr;
    int n;
    char *sarm_ip;

    sarm_ip = "192.168.1.3";
    sarm_portno = 4040;

    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    connect_to_sarm(&sarm_sockfd, sarm_ip, sarm_portno, &sarm_addr);

    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR on binding");

    listen(sockfd,5);

    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

    if (newsockfd < 0) 
        error("ERROR on accept");

    while(1){
        bzero(buffer, 1024);
        n = read(newsockfd, buffer, 1023);
        if (n < 0) error("ERROR reading from socket");
        printf("Here is the message: %s\n", buffer);
        append_timestamp(buffer);
        send_to_sarm(sarm_sockfd, buffer);
    }

    close(sockfd);
    close(sarm_sockfd);
    return 0; 
}
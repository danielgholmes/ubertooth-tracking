/* A simple server in the internet domain using TCP
   The port number is passed as an argument */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

void error(const char *msg)
{
   perror(msg);
   exit(1);
}

// void send_to_sarm()
// {
//     return;
// }

// void connect_to_sarm(int *sockfd, char *hostname, int port, struct sockaddr_in *server_addr)
// {

//     *sockfd = socket(AF_INET, SOCK_STREAM, 0);
//     if (sockfd < 0)
//         error("ERROR opening socket");

//     struct hostent *server;
//     server = gethostbyname(hostname);
//     if (server == NULL) 
//     {
//         fprintf(stderr,"ERROR, no such host as %s\n", hostname);
//         exit(0);
//     }

//     bzero((char *) server_addr, sizeof(*server_addr));
//     server_addr->sin_family = AF_INET;
//     bcopy((char *)server->h_addr, (char *)&(server_addr->sin_addr.s_addr), server->h_length);
//     server_addr->sin_port = htons(port);

//     printf("Attempting to connect...\n");
//     if (connect(*sockfd, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0)
//     {
//         perror("Connect failed. Error");
//         return;
//     }

//     puts("Connected\n");
// }

int main(int argc, char *argv[])
{
    // int sarm_sockfd, sarm_portno;
    // struct sockaddr_in sarm_addr;
    // char *sarm_ip;

    // sarm_ip = "192.168.1.3";
    // sarm_portno = 4040;

    // connect_to_sarm(&sarm_sockfd, sarm_ip, sarm_portno, &sarm_addr);

    int sockfd, newsockfd, portno;
    socklen_t clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    int n;

    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

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
        bzero(buffer,256);
        n = read(newsockfd,buffer,255);
        if (n < 0) error("ERROR reading from socket");
        printf("Here is the message: %s\n",buffer);
        // send_to_sarm();
    }
    // n = write(newsockfd,"I got your message",18);
    // if (n < 0) error("ERROR writing to socket");

    close(newsockfd);
    close(sockfd);
    return 0; 
}
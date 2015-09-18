#include <stdio.h>
#include <stdlib.h>
#include <time.h>
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
    int n = 0;

    printf("Sending...\n");
    n = send(from_socket, message, strlen(message), 0);
    
    printf("%s has been sent!\n", message);
    if (n < 0)
      error("ERROR in sendto");
}

void init_gateway_server(int *sockfd, int port, struct sockaddr_in *server_addr)
{
    printf("Initialising gateway server...\n");
    socklen_t client_len;

    *sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (*sockfd < 0) 
        error("ERROR opening socket");

    bzero((char *) server_addr, sizeof(*server_addr));
    server_addr->sin_family = AF_INET;
    server_addr->sin_addr.s_addr = INADDR_ANY;
    server_addr->sin_port = htons(port);

    if (bind(*sockfd, (struct sockaddr *) server_addr, sizeof(*server_addr)) < 0) 
        error("ERROR on binding");

    printf("Done!\n");
}

void connect_to_sarm(int *sockfd, char *hostname, int port, struct sockaddr_in *server_addr)
{
    *sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (*sockfd < 0)
        error("ERROR opening socket");

    struct hostent *server;
    server = gethostbyname(hostname);
    if (server == NULL) 
    {
        fprintf(stderr,"ERROR no such host as %s\n", hostname);
        exit(0);
    }

    bzero((char *) server_addr, sizeof(*server_addr));
    server_addr->sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&(server_addr->sin_addr.s_addr), server->h_length);
    server_addr->sin_port = htons(port);

    printf("Attempting to connect to SARM...\n");
    if (connect(*sockfd, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0)
    {
        perror("ERROR connect failed ");
        return;
    }

    puts("Connected\n");
}

void get_random_position(char *x_char, char *y_char) // temporary function
{
    float a = 5.0;
    float x = 0.0;
    float y = 0.0;

    x = ((float)rand()/(float)(RAND_MAX)) * a * 100.0;
    y = ((float)rand()/(float)(RAND_MAX)) * a * 100.0;

    snprintf(x_char, 8, "%f", x);
    snprintf(y_char, 8, "%f", y);
    
    return;
}

void get_timestamp(char *str)
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    long double timestamp = (long double)tv.tv_sec + (long double)tv.tv_usec/1000000.0;
    snprintf(str, 32, "%Lf", timestamp);
    return;
}

// local name, x, y, timestamp
void create_message(char *arg1, char *arg2, char *arg3, char *arg4, char *buffer)
{
    strcpy(buffer, arg1);
    strcat(buffer, ",");
    strcat(buffer, arg2);
    strcat(buffer, ",");
    strcat(buffer, arg3);
    strcat(buffer, ",");
    strcat(buffer, arg4);
    return;
}

void get_message_elements(char message[1024], char rx_strings[4][64])
{
    int i = 0;
    char *pch;
    pch = strtok(message, ",");

    while (pch != NULL)
    {
        strcpy(rx_strings[i], pch);
        pch = strtok(NULL, ",");
        i++;
    }
    return;
}

int main(int argc, char *argv[])
{   
    int sockfd, sarm_sockfd, portno, sarm_portno;
    char buffer[1024];
    char message[1024];
    char rx_strings[4][64]; // pi id, local name, distance, timestamp
    struct sockaddr_in server_addr, client_addr, sarm_addr;
    int n;
    socklen_t len = sizeof(client_addr);
    char *sarm_ip;

    // used for storing received data
    char pi_id[4];
    char local_name[32];
    char distance[16];
    char pi_timestamp[32];

    // used for sending to SARM
    char x[8];
    char y[8];
    char own_timestamp[32];

    sarm_ip = "192.168.1.3";
    sarm_portno = 4040;

    if (argc < 2) {
        fprintf(stderr,"ERROR no port provided\n");
        exit(1);
    }
    portno = atoi(argv[1]);

    //connect_to_sarm(&sarm_sockfd, sarm_ip, sarm_portno, &sarm_addr);
    init_gateway_server(&sockfd, portno, &server_addr);

    while(1){
        bzero(buffer, 1024);
        bzero(message, 1024);

        n = recvfrom(sockfd, buffer ,1024, 0, (struct sockaddr *) &client_addr, &len);

        if (n < 0) 
            error("ERROR reading from socket\n");

        printf("Received: %s\n", buffer);
        get_message_elements(buffer, rx_strings);

        strcpy(pi_id, rx_strings[0]);
        strcpy(local_name, rx_strings[1]);
        strcpy(distance, rx_strings[2]);
        strcpy(pi_timestamp, rx_strings[3]);

        get_timestamp(own_timestamp);
        get_random_position(x, y); // temporary
        create_message(local_name, x, y, own_timestamp, message);
        //send_to_sarm(sarm_sockfd, message);
    }

    close(sockfd);
    close(sarm_sockfd);
    return 0; 
}
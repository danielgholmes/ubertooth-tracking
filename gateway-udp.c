#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_multifit_nlin.h>

// TODO: find correct memory allocations for strings

#define N 3
#define NUM_OF_DEVICES 2
#define NUM_OF_NODES 3
#define TIME_FRAME 100 // milli-seconds

typedef struct {
    double x;
    double y;
} position;

typedef struct {
    position pos;
    double r;
} circle;

struct node {
    double distance;
    long double timestamp;
};

struct device {
    position pos;
    int count;
    int base_id;
    long double ref_timestamp;
    char local_name[32];
    struct node nodes[NUM_OF_NODES];
};

struct data {
    size_t n;
    double *_x;
    double *_y;
    double *_r;
};


int circle_f(const gsl_vector *x, void *data, gsl_vector *f)
{
    size_t n = ((struct data *)data)->n;
    double *_x = ((struct data *)data)->_x;
    double *_y = ((struct data *)data)->_y;
    double *_r = ((struct data *)data)->_r; 

    double x_e = gsl_vector_get(x, 0);
    double y_e = gsl_vector_get(x, 1);

    size_t i;

    for (i = 0; i < n; i++)
    {
        double t = i;
        double y_i = _r[i] - sqrt(pow(_x[i] - x_e, 2.0) + pow(_y[i] - y_e, 2.0));
        gsl_vector_set(f, i, y_i);
    }

    return GSL_SUCCESS;
}

int circle_df(const gsl_vector *x, void *data, gsl_matrix *J)
{
    size_t n = ((struct data *)data)->n;
    double *_x = ((struct data *) data)->_x;
    double *_y = ((struct data *) data)->_y;

    double x_e = gsl_vector_get(x, 0);
    double y_e = gsl_vector_get(x, 1);

    size_t i;

    for (i = 0; i < n; i++)
    {
        double n1 = _x[i] - x_e;
        double n2 = _y[i] - y_e;
        double denom = sqrt(pow(_x[i] - x_e, 2.0) + pow(_y[i] - y_e, 2.0));
        gsl_matrix_set (J, i, 0, n1/denom); 
        gsl_matrix_set (J, i, 1, n2/denom);
    }
    return GSL_SUCCESS;
}

int circle_fdf(const gsl_vector *x, void *data, gsl_vector *f, gsl_matrix *J)
{
    circle_f(x, data, f);
    circle_df(x, data, J);

    return GSL_SUCCESS;
}

void print_state (size_t iter, gsl_multifit_fdfsolver * s)
{
    printf ("iter: %3u x = % 15.8f % 15.8f % 15.8f "
            "|f(x)| = %g\n",
            iter,
            gsl_vector_get(s->x, 0), 
            gsl_vector_get(s->x, 1),
            gsl_blas_dnrm2(s->f));
}

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

position trilateration(circle circle_1, circle circle_2, circle circle_3)
{
    position result;
    const gsl_multifit_fdfsolver_type *T;
    gsl_multifit_fdfsolver *s;
    int status;
    unsigned int i, iter = 0;
    const size_t n = N;
    const size_t p = 2;

    double _x[N] = {circle_1.pos.x, circle_2.pos.x, circle_3.pos.x};
    double _y[N] = {circle_1.pos.y, circle_2.pos.y, circle_3.pos.y};
    double _r[N] = {circle_1.r, circle_2.r, circle_3.r};

    struct data d = {n, _x, _y, _r};
    gsl_multifit_function_fdf f;
    double x_init[2] = {1.0, 1.0}; // initial values are uninformed as there is only 1 minima
    gsl_vector_view x = gsl_vector_view_array(x_init, p);

    f.f = &circle_f;
    f.df = &circle_df;
    f.fdf = &circle_fdf;
    f.n = n;
    f.p = p;
    f.params = &d;

    T = gsl_multifit_fdfsolver_lmsder;
    s = gsl_multifit_fdfsolver_alloc(T, n, p);
    gsl_multifit_fdfsolver_set(s, &f, &x.vector);

    // print_state(iter, s);

    do
    {
        iter++;
        status = gsl_multifit_fdfsolver_iterate(s);

        // printf ("status = %s\n", gsl_strerror(status));
        // print_state(iter, s);

        if (status)
            break;

        status = gsl_multifit_test_delta(s->dx, s->x, 1e-4, 1e-4);

        result.x = gsl_vector_get(s->x, 0);
        result.y = gsl_vector_get(s->x, 1);
    }
    while (status == GSL_CONTINUE && iter < 500);

    gsl_multifit_fdfsolver_free(s);

    return result;
}

void reset_device(struct device *dev)
{
    dev->pos.x = 0.0;
    dev->pos.y = 0.0;
    dev->count = 0;
    dev->base_id = 0;
    dev->ref_timestamp = 0.0;
    strcpy(dev->local_name, "");
    int c;
    for (c = 0; c < NUM_OF_NODES; c++){ 
        dev->nodes[c].distance = 0.0;
        dev->nodes[c].timestamp = 0.0;
    }  
    return; 
}
    
void init_devices(struct device devices[NUM_OF_DEVICES])
{
    int a;
    for (a = 0; a < NUM_OF_DEVICES; a++)
        reset_device(&devices[a]);
    return;
}

void sync_message(char rx_strings0[], char rx_strings1[], char rx_strings2[],
               char rx_strings3[], struct device devices[NUM_OF_DEVICES], int send_device)
{
    // rx_id = atoi(rx_strings[0]);
    // strcpy(local_name, rx_strings[1]);
    // rx_distance = atof(rx_strings[2]);
    // rx_timestamp = strtold(rx_strings[3]);

    int rx_id;
    char local_name[32];
    double rx_distance;
    long double rx_timestamp;

    rx_id = atoi(rx_strings0);
    strcpy(local_name, rx_strings1);
    rx_distance = atof(rx_strings2);
    rx_timestamp = strtold(rx_strings3, NULL);
    rx_timestamp = rx_timestamp*1000;


    int ready_count = 0;

    int i;
    for (i = 0; i < NUM_OF_DEVICES; i++)
    { // we ignore packets from node with base_id because we have received one from that node already
        if ( strcmp(devices[i].local_name, local_name) == 0 && rx_id != devices[i+1].base_id){
            if ( devices[i].nodes[rx_id-1].distance == 0.0 ){ // information for that node has not been recorded
                // zero value of the distance shows that the value hasnt been used as yet
                if ( abs(rx_timestamp - devices[i].ref_timestamp) <= TIME_FRAME ){
                    devices[i].nodes[rx_id-1].distance = rx_distance;
                    devices[i].count = devices[i].count + 1;
                    devices[i].ref_timestamp = rx_timestamp;
                    break;
                }
                else // it took too long to receive packets from other nodes
                {
                    reset_device(&devices[i]); // reset the information for this device
                    strcpy(devices[i].local_name, local_name); // now set this value as the new base value
                    devices[i].ref_timestamp = rx_timestamp; 
                    devices[i].nodes[rx_id-1].distance = rx_distance;
                    devices[i].count = devices[i].count + 1;
                    devices[i].base_id = rx_id; // this is the reference node
                    printf("base_id: %d\n", rx_id);
                    // printf("Discarded %d\n", i);
                    break;
                }
            }
            else break; // means that we received from device with rx_id already, so ignore it
        }
        else if ( strcmp(devices[i].local_name, "") == 0 ) // if new device detected
        {
            strcpy(devices[i].local_name, local_name); // save the new found device
            devices[i].ref_timestamp = rx_timestamp; // first timestamp received for this new device
            devices[i].nodes[rx_id-1].distance = rx_distance;
            devices[i].count = devices[i].count + 1;
            devices[i].base_id = rx_id; // this is the reference node
            printf("base_id: %d\n", rx_id);
            break;
        }
    }

    if ( devices[i].count == 3 ) // check if we have received 3 distances within the timeframe
    {
        printf("Ready to send! %s\n", devices[i].local_name);
        reset_device(&devices[i]);
        send_device = i;
        return;
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
    int send_device = 0;
    position ans;
    struct device devices[NUM_OF_DEVICES];

    // used for storing received data
    char rx_id[4];
    char local_name[32];
    char distance[16];
    char rx_timestamp[32];

    // used for sending to SARM
    char x_send[8];
    char y_send[8];
    char own_timestamp[32];

    // data for trilateration
    circle circle_1;
    circle circle_2;
    circle circle_3;

    // postions of the nodes
    circle_1.pos.x = 0.0;
    circle_1.pos.y = 0.0;
    circle_2.pos.x = 4.0;
    circle_2.pos.y = 0.0;
    circle_3.pos.x = 2.0;
    circle_3.pos.y = 3.0;

    circle_1.r = 0.0;
    circle_2.r = 0.0;
    circle_3.r = 0.0;

    sarm_ip = "192.168.1.3";
    sarm_portno = 4040;

    if (argc < 2) {
        fprintf(stderr,"ERROR no port provided\n");
        exit(1);
    }
    portno = atoi(argv[1]);

    // connect_to_sarm(&sarm_sockfd, sarm_ip, sarm_portno, &sarm_addr);
    init_gateway_server(&sockfd, portno, &server_addr);
    init_devices(devices);

    while(1)
    {
        bzero(buffer, 1024);
        bzero(message, 1024);

        n = recvfrom(sockfd, buffer ,1024, 0, (struct sockaddr *) &client_addr, &len);

        if (n < 0) 
            error("ERROR reading from socket\n");

        printf("Received: %s\n", buffer);
        get_message_elements(buffer, rx_strings);

        sync_message(rx_strings[0], rx_strings[1], rx_strings[2], rx_strings[3], devices, send_device);

        if (send_device)
        {
            circle_1.r = devices[send_device].nodes[0].distance;
            circle_2.r = devices[send_device].nodes[1].distance;
            circle_3.r = devices[send_device].nodes[2].distance;

            ans = trilateration(circle_1, circle_2, circle_3);
            snprintf(x_send, 8, "%f", ans.x);
            snprintf(y_send, 8, "%f", ans.y);

            get_timestamp(own_timestamp);  

            create_message(local_name, x_send, y_send, own_timestamp, message);
            send_to_sarm(sarm_sockfd, message);

            send_device = 0;
            circle_1.r = 0.0;
            circle_3.r = 0.0;
            circle_3.r = 0.0;
        }

        // temporarily store the received data
        // strcpy(rx_id, rx_strings[0]); 
        // strcpy(local_name, rx_strings[1]);
        // strcpy(distance, rx_strings[2]);
        // strcpy(rx_timestamp, rx_strings[3]); 

        // if (*rx_id == '1')
        //     circle_1.r = strtod(distance, NULL);
        // else if (*rx_id == '2')
        //     circle_2.r = strtod(distance, NULL);
        // else if (*rx_id == '3')
        //     circle_3.r = strtod(distance, NULL);

        // if (circle_1.r != 0.0 && circle_2.r != 0.0 && circle_3.r != 0.0)
        //     send_device = 1;

        // if (send_device)
        // {
        //     ans = trilateration(circle_1, circle_2, circle_3);
        //     get_timestamp(own_timestamp);  
        //     snprintf(x_send, 8, "%f", ans.x);
        //     snprintf(y_send, 8, "%f", ans.y);
        //     create_message(local_name, x_send, y_send, own_timestamp, message);
        //     send_to_sarm(sarm_sockfd, message);
        //     send_device = 0;
        //     circle_1.r = 0.0;
        //     circle_3.r = 0.0;
        //     circle_3.r = 0.0;
        // }
    } // end while

    close(sockfd);
    close(sarm_sockfd);
    return 0; 
}
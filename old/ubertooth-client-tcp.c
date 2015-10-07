#include "ubertooth.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <math.h>

#define BUF_SIZE 1024
#define SEND_RSSI_MSG_SIZE 32
#define RSSI_BASE -54
#define SIGNAL_PROP_CONST 0.77634 // in free space
#define REF_RSSI -19.0
#define ADV_CHANNEL 2402 // channel 37
#define ID_FILENAME "/home/daniel/pi/id.txt"

  // from bluetooth_le_packet.h
#define MAX_LE_SYMBOLS 64
#define ADV_IND			0
#define ADV_DIRECT_IND	1
#define ADV_NONCONN_IND	2
#define SCAN_REQ		3
#define SCAN_RSP		4
#define CONNECT_REQ		5
#define ADV_SCAN_IND	6

// from bluetooth_le_packet.h
typedef struct _le_packet_t {
	// raw unwhitened bytes of packet, including access address
	uint8_t symbols[MAX_LE_SYMBOLS];
	uint32_t access_address;
	// channel index
	uint8_t channel_idx;
	// number of symbols
	int length;
	uint32_t clk100ns;
	// advertising packet header info
	uint8_t adv_type;
	int adv_tx_add;
	int adv_rx_add;
} le_packet_t;

typedef struct { // used for easily moving around the data
	char id[8];
	char local_name[100]; // assumption of length, may need to change this
	char rssi[8] ;
	char distance[32];
	char timestamp[32];
} ubertooth_rx;

void get_id(char *id)
{
	FILE *fp;
	fp = fopen(ID_FILENAME,"r");

	if (fp == NULL)
	{
		perror("Could not open ID file\n");
		return;
	}

	fgets(id, sizeof(id) - 1, fp);
	strtok(id, "\n");
	fclose(fp);
	return;
}

libusb_device_handle* init_ubertooth()
{
	libusb_device_handle * devh = NULL;
	char ubertooth_device = -1;

	devh = ubertooth_start(ubertooth_device);
	if (devh == NULL) 
	{
		printf("No ubertooth found.");
		return;
	}

	cmd_set_modulation(devh, MOD_BT_LOW_ENERGY);
	cmd_set_channel(devh, ADV_CHANNEL);
	cmd_btle_sniffing(devh, 2);

	return devh;
}  

void error(char *msg) {
    perror(msg);
    exit(0);
}

void connect_to_server(int *sockfd, char *hostname, int port, struct sockaddr_in *server_addr)
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

void send_to_server(int from_socket, const struct sockaddr_in *server_addr, char *message)
{
	int n = 0;

 	n = send(from_socket, message, strlen(message), 0);
    
    printf("%s has been sent!\n", message);
    if (n < 0)
    	error("ERROR in sendto");
}

// from bluetooth_le_packet.c
void get_local_name(le_packet_t *le_pkt, ubertooth_rx *rx) { // was le_print
	if (!le_packet_is_data(le_pkt)) {
		if (le_pkt->adv_type == ADV_IND){
			if (le_pkt->length-6 > 0) {

				int pos = 0;
				int sublen, i;
				uint8_t type;
				uint8_t *buf = &le_pkt->symbols[12];
				int len = le_pkt->length-6;

				while (pos < len) {
					sublen = buf[pos];
					++pos;
					if (pos + sublen > len) {
						printf("Error: attempt to read past end of buffer (%d + %d > %d)\n", pos, sublen, len); // remove on UberPi
						return;
					}
					if (sublen == 0) {
						printf("Early return due to 0 length\n"); // on UberPi
						return;
					}
					type = buf[pos];
					if (type == 0x09){
						for (i = 1; i < sublen; ++i){
							rx->local_name[i-1] = buf[pos+i];
						}
					}
					pos += sublen;
				}
			}
		}
	}
}

float calculate_distance(int rssi)
{
	return powf(10.0, (REF_RSSI - (float)rssi)/10.0*SIGNAL_PROP_CONST);
}

ubertooth_rx get_ubertooth_data(struct libusb_device_handle *devh, int *new_data)
{
	int i;
	int rssi;
	ubertooth_rx rx = {0};
	le_packet_t le_pkt;
	usb_pkt_rx usb_pkt;

	int r = cmd_poll(devh, &usb_pkt); // problem here for local names that have integer values in them
	if (r < 0) 
		printf("USB error\n");
	
	if (r == sizeof(usb_pkt_rx))
	{
		decode_le(usb_pkt.data, usb_pkt.channel + ADV_CHANNEL, usb_pkt.clk100ns, &le_pkt);

    	// can make a function here
    	struct timeval tv;
    	gettimeofday(&tv, NULL);
   		long double tmstamp = (long double)tv.tv_sec + (long double)tv.tv_usec/1000000.0;
   		rssi = usb_pkt.rssi_max + RSSI_BASE;

		sprintf(rx.rssi,"%i", rssi);
		sprintf(rx.distance,"%f", calculate_distance(rssi));
    	sprintf(rx.timestamp,"%Lf", tmstamp);

		get_local_name(&le_pkt, &rx);

		*new_data = 1;
		return rx;
	}
}
// id, local name, distance, timestamp
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

int main(void) 
{
	char id[4];
	char message[1024]; 
    int sockfd, server_port, client_port, new_data;
    struct sockaddr_in server_addr;
    char *server_ip, *client_ip;
    struct libusb_device_handle *devh = NULL;
    ubertooth_rx rx_data;

    server_ip = "127.0.0.1";
    server_port = 4040;
    new_data = 0;

  	get_id(id);
    devh = init_ubertooth();
    connect_to_server(&sockfd, server_ip, server_port, &server_addr);
    
	while (1) {
		rx_data = get_ubertooth_data(devh, &new_data); // should pass in rx_data as a pointer, for consistancy
		if (new_data && (strcmp(rx_data.local_name, "iPhone") == 0)){
			bzero(message, 1023);
			create_message(id, rx_data.local_name, rx_data.distance, rx_data.timestamp, message); 
			send_to_server(sockfd, &server_addr, message); 
			new_data = 0;
		}
	}

    close(sockfd);
    ubertooth_stop(devh);

    return 0;
}

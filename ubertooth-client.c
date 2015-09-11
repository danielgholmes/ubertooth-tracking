/*
 * Copyright 2014 Seecrypt
 *
 */

#include "ubertooth.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

#define BUF_SIZE 1024
#define SEND_RSSI_MSG_SIZE 32
#define RSSI_BASE -54
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

char id; 

typedef struct { // used for easily moving around the data
	char rec_type;
	char logger_id;
	char local_name[100]; // assumption of length, may need to change this
	char rssi;
	long double timestamp;
} ubertooth_rx;

void get_id()
{
	char line[4];
	FILE *fp;
	fp = fopen(ID_FILENAME,"r");

	if (fp == NULL)
	{
		perror("Could not open ID file\n");
		return;
	}

	fgets(line, sizeof(line) - 1, fp);
	id = atoi(line);
	fclose(fp);
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

void send_to_server(int from_socket, const struct sockaddr_in *server_addr, ubertooth_rx *data)
{
	int n;
	n = 0;

	printf("Sending...\n");

    char buf[50];
   	snprintf(buf,50,"%Lf",data->timestamp);

	if (data->rec_type == 0x01)
		n = send(from_socket, buf, strlen(buf), 0);
    
    printf("%s has been sent!\n", buf);
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
						printf("Complete Local Name: "); // remove on UberPi
						for (i = 1; i < sublen; ++i){
							printf("%c", isprint(buf[pos+i]) ? buf[pos+i] : '.'); // remove on UberPi
							rx->local_name[i-1] = buf[pos+i];
						}
					}
					pos += sublen;
				}
			}
		}
	}
	printf("\n"); // remove on UberPi
	int i;
	printf("Saved Local Name: "); // remove on UberPi
	for (i = 0; i < strlen(rx->local_name)-1; ++i){
		printf("%c", rx->local_name[i]);
	}
	printf("\n");
}

ubertooth_rx get_ubertooth_data(struct libusb_device_handle *devh, int *new_data)
{
	int i;
	int8_t rssi;
	ubertooth_rx rx = {0};
	le_packet_t le_pkt;
	usb_pkt_rx usb_pkt;

	rx.rec_type = 0x00;
	rx.logger_id = id; // should do this once in main

	int r = cmd_poll(devh, &usb_pkt);
	if (r < 0) 
		printf("USB error\n");
	
	if (r == sizeof(usb_pkt_rx))
	{
		decode_le(usb_pkt.data, usb_pkt.channel + ADV_CHANNEL, usb_pkt.clk100ns, &le_pkt);

		rx.rec_type = 0x01;
		rx.rssi = usb_pkt.rssi_max + RSSI_BASE;
    	
    	// can make a function here
    	struct timeval tv;
    	gettimeofday(&tv,NULL);
    	printf("tv_sec = %ld\n", tv.tv_sec); // remove on UberPi
   		printf("tv_usec = %ld\n", tv.tv_usec); // remove on UberPi
   		long double tmstamp = (long double)tv.tv_sec + (long double)tv.tv_usec/1000000.0;

    	rx.timestamp = tmstamp;
    	printf("Saved Timestamp: %Lf\n", rx.timestamp); // remove on UberPi

		get_local_name(&le_pkt, &rx);
		rssi = usb_pkt.rssi_max + RSSI_BASE;
		printf("\t RSSI: %d \n", rssi); // remove on UberPi

		*new_data = 1;
		return rx;
	}
}

int main(void) 
{
    int sockfd, server_port, client_port, new_data;
    struct sockaddr_in server_addr;
    char *server_ip, *client_ip;
    struct libusb_device_handle *devh = NULL;

    server_ip = "192.168.1.3";
    server_port = 4040;
    new_data = 0;

    devh = init_ubertooth();

    connect_to_server(&sockfd, server_ip, server_port, &server_addr);
    get_id();

    ubertooth_rx rx_data;

	while (1) {
		rx_data = get_ubertooth_data(devh, &new_data); // should pass in rx_data as a pointer
		if (new_data){
			send_to_server(sockfd, &server_addr, &rx_data);
			new_data = 0;
		}
	}

    close(sockfd);
    ubertooth_stop(devh);

    return 0;
}


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
#define SIGNAL_PROP_CONST 2.0 // in free space
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

char id; 

typedef struct { // used for easily moving around the data
	char rec_type;
	char logger_id;
	char local_name[100]; // assumption of length, may need to change this
	int rssi;
	float distance;
	long double timestamp;
} ubertooth_rx;


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

ubertooth_rx get_ubertooth_data(struct libusb_device_handle *devh, int *samples, int *total)
{
	int i;
	int8_t rssi;
	ubertooth_rx rx = {0};
	le_packet_t le_pkt;
	usb_pkt_rx usb_pkt;

	rx.rec_type = 0x00;
	rx.logger_id = id; // should do this once in main

	int r = cmd_poll(devh, &usb_pkt);
	// printf("%d\n", r);
	if (r < 0) 
		printf("USB error\n");
	
	if (r == sizeof(usb_pkt_rx))
	{
		decode_le(usb_pkt.data, usb_pkt.channel + ADV_CHANNEL, usb_pkt.clk100ns, &le_pkt);

		rx.rec_type = 0x01;
		rx.rssi = usb_pkt.rssi_max + RSSI_BASE;

		// rx.distance = calculate_distance(rx.rssi);
		// printf("Distance: %f\n", rx.distance);
    	
    	// can make a function here
    	struct timeval tv;
    	gettimeofday(&tv, NULL);
   		long double tmstamp = (long double)tv.tv_sec + (long double)tv.tv_usec/1000000.0;

    	rx.timestamp = tmstamp;

		get_local_name(&le_pkt, &rx);
		printf("RSSI: %d\n", rx.rssi);
		*total += rx.rssi*(-1);
		printf("Total: %d\n", *total);
		*samples = *samples + 1;
		printf("Samples: %d\n", *samples);
		return rx;
	}
}

int main(void) 
{
	int samples = 1; // counter for number of samples
	float n = 0.0; // signal propagation constant
	int A = 0; // rssi at 1m
	int rssi = 0;
	int total = 0;
	int max_dist = 10;
	float n_total = 0;
    struct libusb_device_handle *devh = NULL;
    char enter;

    devh = init_ubertooth();

    ubertooth_rx rx_data;

    printf("Calculation of the average of A at 1m. Press enter when ready.\n");
	enter = getchar();

	while (samples <= 1000) {
		rx_data = get_ubertooth_data(devh, &samples, &total);
	}

    A = (total/samples)*(-1.0);
    printf("Average of A: %d\n", A);
    ubertooth_stop(devh);

    printf("Calculation of n.\n");
    int d = 0;
    for (d = 1; d < max_dist + 1; ++d)
    {
    	devh = init_ubertooth();
    	samples = 0;
    	total = 0;
    	rssi = 0;
    	printf("Move to %dm and press enter.\n", d);
    	enter = getchar();

    	while (samples <= 500) {
			rx_data = get_ubertooth_data(devh, &samples, &total);
		}

		rssi = (total/samples)*(-1.0);
		n_total += (float)rssi/((float)A - 10.0*logf((float)d));
		ubertooth_stop(devh);
    }

    n = n_total/(float)max_dist;
    printf("n = %f\n", n);
    
    return 0;
}


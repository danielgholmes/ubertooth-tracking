/* Filename: ubertooth-rssi.c
 * Version: 2
 * Date: Jan 2015
 * Authors: Daniel Holmes & Dylan Jaconbsen (mostly borrowed and edited code from existing ubertooth functions)
 * Description: Code that gets the RSSI data with an associated complete local name and outputs to the terminal
 * Notes: This code can be run from the terminal, as with other Ubertooth terminal commands. Refer to ubertooth-setup.txt
 * 		  for information on how this is all set up.
 */

#include "ubertooth.h"
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#define RSSI_BASE -54

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

// from bluetooth_le_packet.c
void decode_le(uint8_t *stream, uint16_t phys_channel, uint32_t clk100ns, le_packet_t *p) {
	memcpy(p->symbols, stream, MAX_LE_SYMBOLS);

	p->channel_idx = le_channel_index(phys_channel);
	p->clk100ns = clk100ns;

	p->access_address = 0;
	p->access_address |= p->symbols[0];
	p->access_address |= p->symbols[1] << 8;
	p->access_address |= p->symbols[2] << 16;
	p->access_address |= p->symbols[3] << 24;

	if (le_packet_is_data(p)) {
		// data PDU
		p->length = p->symbols[5] & 0x1f;
	} else {
		// advertising PDU
		p->length = p->symbols[5] & 0x3f;
		p->adv_type = p->symbols[4] & 0xf;
		p->adv_tx_add = p->symbols[4] & 0x40 ? 1 : 0;
		p->adv_rx_add = p->symbols[4] & 0x80 ? 1 : 0;
	}
}

// from bluetooth_le_packet.c
int le_packet_is_data(le_packet_t *p) {
	return p->channel_idx < 37;
}

// from bluetooth_le_packet.c
static void _dump_scan_rsp_data(uint8_t *buf, int len) {
	int pos = 0;
	int sublen, i;
	uint8_t type;

	while (pos < len) {
		sublen = buf[pos];
		++pos;
		if (pos + sublen > len) {
			printf("Error: attempt to read past end of buffer (%d + %d > %d)\n", pos, sublen, len);
			return;
		}
		if (sublen == 0) {
			printf("Early return due to 0 length\n");
			return;
		}
		type = buf[pos];
		if (type == 0x09){
			printf("Complete Local Name: ");
			for (i = 1; i < sublen; ++i)
				printf("%c", isprint(buf[pos+i]) ? buf[pos+i] : '.');
		}
		pos += sublen;
	}
}

// from bluetooth_le_packet.c
void le_print(le_packet_t *p) {
	int i;
	if (!le_packet_is_data(p)) {
		if (p->adv_type == ADV_IND){
			if (p->length-6 > 0) {
				_dump_scan_rsp_data(&p->symbols[12], p->length-6);
			}
		}
	}
}

// should go to ubertooth.c, which then calls functions from ubertrooth_le_packet.c
void display_rssi(usb_pkt_rx* rx)
{
	int i;
	int8_t rssi;

	rssi = rx->rssi_max + RSSI_BASE;

	le_packet_t p;
	decode_le(rx->data, rx->channel + 2402, rx->clk100ns, &p);
	le_print(&p);
	printf("\t RSSI: %d \n", rssi);
	return;
}

int main(int argc, char *argv[])
{
	char ubertooth_device = -1;
	struct libusb_device_handle *devh = NULL;
	int r;

	devh = ubertooth_start(ubertooth_device);
	if (devh == NULL) {
		printf("No ubertooth found.");
		return 1;
	}

	usb_pkt_rx pkt;

	cmd_set_modulation(devh, MOD_BT_LOW_ENERGY);

	u16 channel;
	channel = 2402; // channel 37
	cmd_set_channel(devh, channel);
	cmd_btle_sniffing(devh, 2);

	while (1) {
		int r = cmd_poll(devh, &pkt);
		if (r < 0) {
			printf("USB error\n");
			break;
		}
		if (r == sizeof(usb_pkt_rx))
			display_rssi(&pkt); 
		usleep(500);
	}
	ubertooth_stop(devh);

	return 0;
}

#ifndef _FRAME_H_
#define _FRAME_H_


#define HEADER_SIZE 4

#define NORMAL 0
#define ACK 1
#define NAK 2
#define REQ 3

#define MAX_SEQ 20
#define WINDOW_SIZE 10
#define inc(k) if(k == MAX_SEQ) k = 0; else k = k+1;	// increase sequence number

#include "rdt_struct.h"

typedef unsigned int seq_nr; 

struct frame{
	short checksum;			// 16 bit
	char payload_size;		// 8 bit
	char seq_ack;			// 8 bit
	char payload[RDT_PKTSIZE - HEADER_SIZE];
};


// pack a seq_ack, ack_mark(00 for normal, 01 for ack, 10 for nak)
char pack_seq_ack(int ack_mark, seq_nr seq);

/* fill with Internet checksum*/
void fill_checksum(struct frame *f);

/* verify the checksum*/
bool verify_checksum(struct frame *f);


#endif /*_FRAME_H_*/


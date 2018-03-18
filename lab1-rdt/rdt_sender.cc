/*
 * FILE: rdt_sender.cc
 * DESCRIPTION: Reliable data transfer sender.
 * NOTE: This implementation assumes there is no packet loss, corruption, or 
 *       reordering.  You will need to enhance it to deal with all these 
 *       situations.  In this implementation, the packet format is laid out as 
 *       the following:
 *       
 *       |<-  1 byte  ->|<-             the rest            ->|
 *       | payload size |<-             payload             ->|
 *
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */

/*
	frame structure
		total 128 byte
	checksum ================= 2 byte
	paylaod size   =========== 1 byte(8 bit) 2**8=256
	seq_or_ack	============== 1 byte
		|
		|----- 2 bit ack mark(00 for normal package, 01 for ack, 10 for nak)
		|----- 6 bit for seq numebr(MAX_SEQ = 10)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "frame.h"
#include "rdt_struct.h"
#include "rdt_sender.h"

/* SEQ number range is from 0 to 9, window size is 10 */

#define MIN_TIME 0.1

#define TIME_OUT 3

int buffered_cnt;									// bufferd frame count in frame buffer
													//     whick is 10
struct packet * packet_buffer[MAX_SEQ + 1];			// frame buffer
int buffer_timer[MAX_SEQ + 1];						// timer for packets have not been acknowledge
 													// timer is initialized with -1

seq_nr ack_expected;								// the oldest frame has not been ack
seq_nr next_to_send; 	// the seq number of next frame to send

static void resend_packet(seq_nr seq);
static bool between(seq_nr a, seq_nr b, seq_nr c);

FILE * log_file = NULL;

/* sender initialization, called once at the very beginning */
void Sender_Init()
{
	buffered_cnt = 0;
	ack_expected = 0;
	next_to_send = 0;
	// initialize packet buffer and their timer
	for(int i = 0; i <= MAX_SEQ; i++){
		packet_buffer[i] = (struct packet*) malloc(sizeof(struct packet));
		buffer_timer[i] = -1;
	}
	Sender_StartTimer(MIN_TIME);
    fprintf(stdout, "At %.2fs: sender initializing ...\n", GetSimulationTime());

	log_file = fopen("sender_log", "w");
}

/* sender finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to take this opportunity to release some 
   memory you allocated in Sender_init(). */
void Sender_Final()
{
	for(int i = 0; i <= MAX_SEQ; i++){
		free(packet_buffer[i]);
	}
    fprintf(stdout, "At %.2fs: sender finalizing ...\n", GetSimulationTime());
}


/* event handler, called when a message is passed from the upper layer at the 
   sender */
void Sender_FromUpperLayer(struct message *msg)
{

    /* maximum payload size */
    int maxpayload_size = RDT_PKTSIZE - HEADER_SIZE;
	
	// use frame pointer to fill pakcet
    packet pkt;
	frame *f = (frame *)&pkt;
    int cursor = 0;			// cursor point to the beginning of unsend data in message

    
	/* split the message if it is too big */
    while (msg->size-cursor > maxpayload_size) {
		/* fill in the packet */
		f->payload_size = maxpayload_size;
		memcpy(f->payload, msg->data+cursor, maxpayload_size);
		f->seq_ack = pack_seq_ack(NORMAL, next_to_send);

		// calculate checksum
		fill_checksum(f);

		/* send it out through the lower layer */
		if(buffered_cnt > MAX_SEQ){		
			// loop is wrong because this is single thread model
			for(seq_nr i = 0; i <= MAX_SEQ; i++){
				resend_packet(ack_expected + i);		// resend all packets in buffer first
			}
		}
		buffered_cnt ++;		// should add buffered_cnt first, because this number is the lock
		// copy into buffer
		memcpy(packet_buffer[next_to_send], &pkt, RDT_PKTSIZE);
		// send it through lower layer	
		Sender_ToLowerLayer(&pkt);

		// set buffer timer
		buffer_timer[next_to_send] = 0;

		/* move the cursor */
		cursor += maxpayload_size;

		// increase next_to_send
		inc(next_to_send);
    }

    /* send out the last packet */
    if (msg->size > cursor) {
		/* fill in the packet */
		f->payload_size = msg->size-cursor;
		memcpy(f->payload, msg->data+cursor, f->payload_size);
		f->seq_ack = pack_seq_ack(NORMAL, next_to_send);

		// calculate checksum
		fill_checksum(f);
		
		while(buffered_cnt > MAX_SEQ)		// loop for a while when buffer is full
			// loop is wrong because this is single thread model
			resend_packet(ack_expected);		// resend all packets in buffer
		
		buffered_cnt ++;		// should add buffered_cnt first, because this number is the lock

		// copy into buffer
		memcpy(packet_buffer[next_to_send], &pkt, RDT_PKTSIZE);
		/* send it out through the lower layer */
		Sender_ToLowerLayer(&pkt);
		
		// set buffer timer
		buffer_timer[next_to_send] = 0;
		
		// increase next_to_send
		inc(next_to_send);
    }
}

// return true if a <= b < c circularly
static bool between(seq_nr a, seq_nr b, seq_nr c){
	if(a <= b && b < c)
		return true;
	else if(b < c && a > c)
		return true;
	else if(a <= b && a > c)
		return true;
	else 
		return false;
}

static void resend_packet(seq_nr seq){
	packet *p = packet_buffer[seq];
	// already in buffer
	// reset timer
	buffer_timer[seq] = 0;
	// resend
	Sender_ToLowerLayer(p);

}

/* event handler, called when a packet is passed from the lower layer at the 
   sender */
void Sender_FromLowerLayer(struct packet *pkt)
{
	// the received ack could be bigger than ack_expected,
	// which indicate that the previous packets have already reached
	// for the reason that out GO BACK N protocal has only a buffer with size 
	// of one at the receiver side.
	frame *f = (frame *)pkt;
	// verify checksum value
	if(verify_checksum(f) == false){
		return;
	}
	
	char seq_ack =  f->seq_ack;
	int ack_mark = seq_ack >> 6;
	seq_nr ack_received = seq_ack & 0x3F;	// mask ack_mark(0011 1111)
	// acknowledge
	if(ack_mark == ACK){
		// if ack_reveived is bigger than ack_expected
		// clear buffer and timmer before ack_received

		/*TODO*/
		// print log
		fprintf(log_file, "before:\n");
		fprintf(log_file, "ack_expected %d, received_ack %d, next_to_send %d\n", ack_expected, ack_received, next_to_send);
		while(between(ack_expected, ack_received, next_to_send)){
			// buffer_cnt should be update last because it is a lock
			buffer_timer[ack_expected] = -1;	// clear timer
			buffered_cnt --;	// make a room for comming packet in buffer
			inc(ack_expected);
			// clear timmer
		}
		fprintf(log_file, "after\n");
		fprintf(log_file, "ack_expected %d, received_ack %d, next_to_send %d\n", ack_expected, ack_received, next_to_send);
		fprintf(log_file, "\n\n");
		fprintf(log_file, "===========================================================");
		fprintf(log_file, "\n\n");
		fflush(log_file);

	}else if(ack_mark == NAK){	// not acknowledge
		// packet occur data corrupt
		resend_packet(ack_received);	
	}else{
		// code should not reach here
		//assert(0);
		return;
	}
	
}

/* event handler, called when the timer expires */
void Sender_Timeout()
{
	// iterate the buffer_timer_list and update them
	// if find a timeout buffer, resend it.
	int *a_timer = buffer_timer;
	for(seq_nr i = 0; i <= MAX_SEQ; i++){
		if(*a_timer == -1)	// not set
			continue;
		else if(*a_timer == 1)	/*TODO: this is 0.4s*/
			resend_packet(i);
		else
			*a_timer = *a_timer + 1;

		a_timer ++;
	}

	// restart physical timer
	Sender_StartTimer(MIN_TIME);
}

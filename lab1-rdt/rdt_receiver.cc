/*
 * FILE: rdt_receiver.cc
 * DESCRIPTION: Reliable data transfer receiver.
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rdt_struct.h"
#include "rdt_receiver.h"
#include "frame.h"

seq_nr packet_expected;

/* receiver initialization, called once at the very beginning */
void Receiver_Init()
{
    fprintf(stdout, "At %.2fs: receiver initializing ...\n", GetSimulationTime());
}

/* receiver finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to use this opportunity to release some 
   memory you allocated in Receiver_init(). */
void Receiver_Final()
{
	packet_expected = 0;
    fprintf(stdout, "At %.2fs: receiver finalizing ...\n", GetSimulationTime());
}

static void send_ACK(int ack_mark, seq_nr seq){
	struct packet my_pkt;
	frame *my_f = (frame *)&my_pkt;
	my_f->payload_size = 0;
	my_f->seq_ack = pack_seq_ack(ack_mark, seq);
	fill_checksum(my_f);
	Receiver_ToLowerLayer(&my_pkt);
	
	return;
}

/* event handler, called when a packet is passed from the lower layer at the 
   receiver */
void Receiver_FromLowerLayer(struct packet *pkt)
{
	// verify checksum, if corrupted, send NAK
	frame *f = (frame *)pkt;
	seq_nr received_seq = (f->seq_ack & 0x3F);

	// if not expected packet, just drop it
	if(packet_expected != received_seq){
		send_ACK(REQ, packet_expected);
		return;
	}
	
	if(verify_checksum(f) == false){
		//send_ACK(NAK, received_seq);
		return;
	}

    /* construct a message and deliver to the upper layer */
    struct message *msg = (struct message*) malloc(sizeof(struct message));
    ASSERT(msg!=NULL);

    msg->size = f->payload_size;

    /* sanity check in case the packet is corrupted */
    if (msg->size<0) msg->size=0;
    if (msg->size>RDT_PKTSIZE-HEADER_SIZE) msg->size=RDT_PKTSIZE-HEADER_SIZE;

    msg->data = (char*) malloc(msg->size);
    ASSERT(msg->data!=NULL);
    memcpy(msg->data, f->payload, msg->size);
    Receiver_ToUpperLayer(msg);
	
	// receive success, send ACK
	send_ACK(ACK, received_seq);	
	// update expect packet
	inc(packet_expected);

    /* don't forget to free the space */
    if (msg->data!=NULL) free(msg->data);
    if (msg!=NULL) free(msg);
}

#include "frame.h"

char pack_seq_ack(int ack_mark, seq_nr seq){
	char result;
	result = (ack_mark << 6) | (seq & 0x3F);
	return result;
}


void fill_checksum(frame *f){
	short *buf = (short *)f;	
	buf ++;						// jump over checksum part
	int times = (RDT_PKTSIZE - sizeof(short)) / 2;
	// calculate the sum
	unsigned long long sum = 0;
	for(int i = 0; i < times; i++){
		sum += *buf++;
	}
	// turn to 16 bit
	while(sum >> 16){
		sum = (sum & 0xFFFF) + (sum >> 16);
	}
	short checksum = ~sum;	
	sum = checksum + sum;
	f->checksum = checksum;
}


bool verify_checksum(frame *f){
	return true;
	short *buf = (short *)f;
	int times = RDT_PKTSIZE / 2;
	unsigned long long sum = 0;
	for(int i = 0; i < times; i++){
		sum += *buf++;	
	}
	while(sum >> 16){
		sum = (sum & 0xFFFF) + (sum >> 16);	
	}
	short result = ~sum;
	return (result == 0);
}

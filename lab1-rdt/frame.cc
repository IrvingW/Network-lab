#include "frame.h"

char pack_seq_ack(int ack_mark, seq_nr seq){
	char result;
	result = (ack_mark << 6) | (seq & 0x3F);
	return result;
}


void fill_checksum(frame *f){
	short *buf = (short *)f;	
	buf ++;						// jump over checksum part
	//int invalid_bytes = HEADER_SIZE + f->payload_size;
	//int times = (invalid_bytes - sizeof(short) + 1) / 2;
	int times = (RDT_PKTSIZE - 2) /2;
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
	short *buf = (short *)f;	
	buf ++;						// jump over checksum part
	//int invalid_bytes = HEADER_SIZE + f->payload_size;
	//int times = (invalid_bytes - sizeof(short) + 1) / 2;
	int times = (RDT_PKTSIZE - 2) /2;
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
	if(checksum == f->checksum)
		return true;
	else
		return false;
}

/**
 * (c) 2015 Western Digital Technologies, Inc. All rights reserved.
 * Front-end tool for the ATA RESET WRITE POINTER command
 * Compliant to ZAC Specification draft, revision 0.8n (March 4, 2015)
 * Author: Austin Liou (austin.liou@wdc.com)
 */
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <scsi/sg.h>

#define ATA_PASS_THROUGH_16 0x85
#define ATA_PASS_THROUGH_16_LEN 16
#define ATA_RESET_WRITE_POINTER 0x9f

void usage(){
	printf(	"Usage: resetzones [-?] [-l zonestartlba] dev\n"
		"	-?	: Print out usage\n"
		"	-l	: First LBA of zone to reset.  Optional.  If omitted, will reset ALL zones.\n"
		"	dev	: The device handle to open (e.g. /dev/sdb).  Required.\n"
	);
}

int main(int argc, char * argv[])
{
	int opt;
	int sg_fd;
	uint8_t cdb[ATA_PASS_THROUGH_16_LEN];
	sg_io_hdr_t io_hdr;
	uint64_t lba;
	bool hasLbaArg = false;
	unsigned char senseBuff[64];

	while ((opt = getopt (argc, argv, "l:?")) != -1){
		char* endPtr;
		switch(opt){
			case 'l':
				lba = strtol(optarg,&endPtr,0);
				if(*endPtr!='\0'){
					fprintf(stderr, "Invalid -l argument.  Use -? for usage.\n");
					return 1;
				}
				hasLbaArg = true;
				break;
			case '?':
				usage();
				return 0;
		}
	}
	if (optind >= argc){
		printf("Requires device argument.  Use -? for usage\n");
		return 1;
	}

	char* deviceFile = argv[optind];
	if ((sg_fd = open(deviceFile, O_RDWR)) < 0) {
		perror("Error opening device");
		return 1;
	}

	printf("Sending RESET WRITE POINTER command...\n");
	memset(cdb, 0, sizeof(cdb));
	cdb[0] = ATA_PASS_THROUGH_16;
	cdb[1] = (0x3 << 1) | 0x01;	//Non-data, 48-bit ATA command
	cdb[2] = 0x1 << 5;		//Check condition bit set
	cdb[4] = 0x04;			//Action: 04h
	if(hasLbaArg){
		cdb[7] = (lba>>24)&0xff;
		cdb[8] = (lba&0xff);
		cdb[9] = (lba>>32)&0xff;
		cdb[10] = (lba>>8)&0xff;
		cdb[11] = (lba>>40)&0xff;
		cdb[12] = (lba>>16)&0xff;
	} else {
		cdb[3] |= 0x01;		//ALL bit set (reset all write pointers)
	}
	cdb[14] = ATA_RESET_WRITE_POINTER;
	memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
	io_hdr.interface_id = 'S';
	io_hdr.cmdp = cdb;
	io_hdr.cmd_len = sizeof(cdb);
	io_hdr.dxferp = NULL;
	io_hdr.dxfer_direction = SG_DXFER_NONE;
	io_hdr.dxfer_len = 0;
	io_hdr.sbp = senseBuff;
	io_hdr.mx_sb_len = sizeof(senseBuff);
	io_hdr.timeout = 10000;
	if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
		perror("ioctl error");
		close(sg_fd);
		return 1;
	}
	close(sg_fd);
/*
	uint8_t *ataStatusReturnDescriptor = &(senseBuff[8]);
	printf("EXT                   %02x\n", ataStatusReturnDescriptor[2]);
	printf("STATUS:               %02x\n", ataStatusReturnDescriptor[13]);
	printf("ERROR                 %02x\n", ataStatusReturnDescriptor[3]);
	printf("SECTOR COUNT [15:8]:  %02x\n", ataStatusReturnDescriptor[4]);
	printf("SECTOR COUNT [7:0]:   %02x\n", ataStatusReturnDescriptor[5]);
	printf("LBA LOW [15:8]:       %02x\n", ataStatusReturnDescriptor[6]);
	printf("LBA LOW [7:0]:        %02x\n", ataStatusReturnDescriptor[7]);
	printf("LBA MID [15:8]:       %02x\n", ataStatusReturnDescriptor[8]);
	printf("LBA MID [7:0]:        %02x\n", ataStatusReturnDescriptor[9]);
	printf("LBA HIGH [15:8]:      %02x\n", ataStatusReturnDescriptor[10]);
	printf("LBA HIGH [7:0]:       %02x\n", ataStatusReturnDescriptor[11]);
	printf("DEVICE:               %02x\n", ataStatusReturnDescriptor[12]);
*/

	/// Parse sense buffer for errors
	//TODO: Incorporate this with existing sense codes and strings in sg3_utils?
	uint8_t senseError = senseBuff[0] & 0x7F;
	uint8_t senseKey = 0;
	uint8_t asc = 0;
	uint8_t ascq = 0;
	switch(senseError){
		case 0x70:
		case 0x71:
			//Fixed format sense data
			senseKey = senseBuff[2] & 0xF;
			asc = senseBuff[12];
			ascq = senseBuff[13];
			break;
		case 0x72:
		case 0x73:
			//Descriptor format sense data
			senseKey = senseBuff[1] & 0xF;
			asc = senseBuff[2];
			ascq = senseBuff[3];
			break;
		default:
			fprintf(stderr, "Error: RESET WRITE POINTER failed, but could not interpret sense data.\n");
			return 1;
	}
	if (senseKey == 0x00 || (senseKey == 0x01 && asc == 0x00 && ascq == 0x1d)){
		printf("Done.\n");
	} else if(senseKey == 0x05 && asc == 0x24 && ascq == 0x00){	//INVALID FIELD IN CDB
		fprintf(stderr, "Error: RESET WRITE POINTER failed because input LBA does not specify start of zone\n");
	} else if (senseKey == 0x05 && asc == 0x2c && ascq == 0x0d){	//RESET WRITE POINTER NOT ALLOWED
		fprintf(stderr, "Error: RESET WRITE POINTER failed because zone condition is OFFLINE\n");
	} else if (senseKey == 0x07 && asc == 0x27 && ascq == 0x08){	//ZONE IS READ ONLY
		fprintf(stderr, "Error: RESET WRITE POINTER failed because zone condition is READ ONLY\n");
	} else if (senseKey == 0x0b && asc == 0x00 && ascq == 0x00){
		fprintf(stderr, "Error: RESET WRITE POINTER failed (Aborted Command with no additional sense code).\n");
	} else {
		fprintf(stderr, "Error: RESET WRITE POINTER failed.  Sense data: (SK=0x%02x, ASC=0x%02x, ASCQ=0x%02x)\n", senseKey, asc, ascq);
	}
	//for(int i = 0; i < sizeof(senseBuff); i++){ printf("%02X ", senseBuff[i]); } printf("\n");
}

/**
 * (c) 2015 Western Digital Technologies, Inc. All rights reserved.
 * Front-end tool for the ATA RESET WRITE POINTER command
 * Compliant to ZAC Specification draft, revision 0.8n (March 4, 2015)
 * Author: Austin Liou (austin.liou@wdc.com)
 */
#include "common.h"

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
	uint8_t senseBuff[32];

	while ((opt = getopt (argc, argv, "l:?")) != -1){
		char* endPtr;
		switch (opt){
			case 'l':
				lba = strtol(optarg,&endPtr,0);
				if (*endPtr!='\0'){
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
	cdb[1] = (0x3 << 1) | 0x01;	// Non-data, 48-bit ATA command
	cdb[2] = 0x1 << 5;		// Check condition bit set
	cdb[4] = 0x04;			// Action: 04h
	if (hasLbaArg){
		cdb[7] = (lba>>24)&0xff;
		cdb[8] = (lba&0xff);
		cdb[9] = (lba>>32)&0xff;
		cdb[10] = (lba>>8)&0xff;
		cdb[11] = (lba>>40)&0xff;
		cdb[12] = (lba>>16)&0xff;
	} else {
		cdb[3] |= 0x01;		// ALL bit set (reset all write pointers)
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

	// Check if command completed successfully
	struct KeyCodeQualifier kcq;
	if (!getSenseErrors(senseBuff, &kcq)){
		fprintf(stderr, "Error: Could not parse sense buffer from RESET WRITE POINTER command\n");
		close(sg_fd);
		return 1;
	}
	if (kcq.senseKey == 0x00 || (kcq.senseKey == 0x01 && kcq.asc == 0x00 && kcq.ascq == 0x1d)){
		printf("Done.\n");
		close(sg_fd);
		return 0;
	}
	
	// Issue REQUEST SENSE DATA EXT if reset failed
	memset(senseBuff, 0, sizeof(senseBuff));
	memset(cdb, 0, sizeof(cdb));
	cdb[0] = ATA_PASS_THROUGH_16;
	cdb[1] = (0x3 << 1) | 0x01;
	cdb[2] = 0x1 << 5;
	cdb[14] = ATA_REQUEST_SENSE_DATA_EXT;
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

	memset(&kcq, 0, sizeof(kcq));
	struct AtaStatusReturnDescriptor ataReturn;
	if (!getSenseErrors(senseBuff, &kcq) || !senseToAtaRegisters(senseBuff, &ataReturn)){
		fprintf(stderr, "Error: Could not parse sense buffer from REQUEST SENSE DATA EXT command\n");
		close(sg_fd);
		return 1;
	}
	if (kcq.senseKey == 0x01 && kcq.asc == 0x00 && kcq.ascq == 0x1d){
		// Key Code Qualifier is stored in LBA registers of ATA descriptor.  Use that to extract error codes.
		kcq.senseKey = ataReturn.lbaHigh & 0xff;
		kcq.asc = ataReturn.lbaMid & 0xff;
		kcq.ascq = ataReturn.lbaLow & 0xff;
	}
	
	if (kcq.senseKey == 0x0b && kcq.asc == 0x00 && kcq.ascq == 0x00){
		fprintf(stderr, "Error: Command was aborted, is this a ZAC drive?\n");
	} else if (kcq.senseKey == 0x05 && kcq.asc == 0x24 && kcq.ascq == 0x00){	//INVALID FIELD IN CDB
		fprintf(stderr, "Error: Input LBA does not specify start of write pointer zone\n");
	} else if (kcq.senseKey == 0x05 && kcq.asc == 0x2c && kcq.ascq == 0x0d){	//RESET WRITE POINTER NOT ALLOWED
		fprintf(stderr, "Error: Zone condition is OFFLINE\n");
	} else if (kcq.senseKey == 0x07 && kcq.asc == 0x27 && kcq.ascq == 0x08){	//ZONE IS READ ONLY
		fprintf(stderr, "Error: Zone condition is READ ONLY\n");
	} else {
		fprintf(stderr, "Error: RESET WRITE POINTER failed.  Sense data: (SK=0x%02x, ASC=0x%02x, ASCQ=0x%02x)\n", kcq.senseKey, kcq.asc, kcq.ascq);
	}

	return 0;
}

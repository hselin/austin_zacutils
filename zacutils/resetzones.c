/**
 * (c) 2015 Western Digital Technologies, Inc. All rights reserved.
 * Front-end tool for the ATA RESET WRITE POINTER command
 * Compliant to ZAC Specification draft, revision 0.8n (March 4, 2015)
 * Author: Austin Liou (austin.liou@wdc.com)
 */
#include "resetzones.h"

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
	uint64_t lba = 0;
	bool resetAll = true;
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
				resetAll = false;
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
	if (!ataPassthrough16(
		&sg_fd,
		ATA_RESET_WRITE_POINTER,
		(resetAll ? RESET_ALL_BIT : 0) | ACTION_RESET_WRITE_POINTER,
		0x0000,
		lba,
		0x00,
		ATA_PROTOCOL_NONDATA,
		ATA_FLAGS_CKCOND,
		SG_DXFER_NONE,
		NULL,
		0,
		senseBuff,
		sizeof(senseBuff)
	)){ return 1; }

	// Check if command completed successfully
	struct KeyCodeQualifier kcq;
	if (!getSenseErrors(senseBuff, &kcq)){
		fprintf(stderr, "Error: Could not parse sense buffer from RESET WRITE POINTER command\n");
		close(sg_fd);
		return 1;
	}
	if (kcq.senseKey == NO_SENSE || assertKcq(&kcq, RECOVERED_ERROR, ASC_ATA_PASS_THROUGH_INFORMATION_AVAILABLE)){
		printf("Done.\n");
		close(sg_fd);
		return 0;
	}
	
	// Issue REQUEST SENSE DATA EXT if reset failed
	memset(senseBuff, 0, sizeof(senseBuff));
	if (!ataPassthrough16(
		&sg_fd,
		ATA_REQUEST_SENSE_DATA_EXT,
		0x0000,
		0x0000,
		0,
		0x00,
		ATA_PROTOCOL_NONDATA,
		ATA_FLAGS_CKCOND,
		SG_DXFER_NONE,
		NULL,
		0,
		senseBuff,
		sizeof(senseBuff)
	)){ return 1; }

	close(sg_fd);

	memset(&kcq, 0, sizeof(kcq));
	struct AtaStatusReturnDescriptor ataReturn;
	if (!getSenseErrors(senseBuff, &kcq) || !senseToAtaRegisters(senseBuff, &ataReturn)){
		fprintf(stderr, "Error: Could not parse sense buffer from REQUEST SENSE DATA EXT command\n");
		return 1;
	}
	if (assertKcq(&kcq, RECOVERED_ERROR, ASC_ATA_PASS_THROUGH_INFORMATION_AVAILABLE)){
		// Key Code Qualifier is stored in LBA registers of ATA descriptor.  Use that to extract error codes.
		kcq.senseKey = ataReturn.lbaHigh & 0xff;
		kcq.asc = ataReturn.lbaMid & 0xff;
		kcq.ascq = ataReturn.lbaLow & 0xff;
	}
	
	if (assertKcq(&kcq, ABORTED_COMMAND, ASC_NO_ADDITIONAL_SENSE_INFORMATION)){
		fprintf(stderr, "Error: Command was aborted, is this a ZAC drive?\n");
	} else if (assertKcq(&kcq, ILLEGAL_REQUEST, ASC_INVALID_FIELD_IN_CDB)){
		fprintf(stderr, "Error: Input LBA does not specify start of write pointer zone\n");
	} else if (assertKcq(&kcq, ILLEGAL_REQUEST, ASC_RESET_WRITE_POINTER_NOT_ALLOWED)){
		fprintf(stderr, "Error: Zone condition is OFFLINE\n");
	} else if (assertKcq(&kcq, DATA_PROTECT, ASC_ZONE_IS_READ_ONLY)){
		fprintf(stderr, "Error: Zone condition is READ ONLY\n");
	} else {
		fprintf(stderr, "Error: RESET WRITE POINTER failed.  Sense data: (SK=0x%02x, ASC=0x%02x, ASCQ=0x%02x)\n", kcq.senseKey, kcq.asc, kcq.ascq);
	}

	return 1;
}

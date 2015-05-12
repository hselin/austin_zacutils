/**
 * (c) 2015 Western Digital Technologies, Inc. All rights reserved.
 * Front-end tool for the ATA REPORT ZONES DMA command
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
#define ATA_REPORT_ZONES_DMA 0x4a

/// Size of buffer, in zone entries.  This will cause ioctl to bug out if too large.
/// The actual buffer size will be larger to include the header
#define REPORT_ZONES_ENTRY_BUFFER_SIZE 2047

/// REPORT ZONES DMA header (64 bytes)
struct ReportZonesHeader {
	uint32_t zoneListLength;
	uint16_t options;
	uint8_t _reserved1[2];
	uint32_t maxOpenSeqZones;
	uint32_t unreliableSectors;
	uint8_t _reserved2[4];
	uint8_t _reserved3[44];
};

/// REPORT ZONES DMA record (64 bytes)
struct ReportZonesEntry {
	uint16_t options;
	uint8_t _reserved1[2];
	uint8_t _reserved2[4];
	uint64_t zoneLength;
	uint64_t zoneStartLba;
	uint64_t writePointer;
	uint64_t checkpoint;
	uint8_t _reserved3[24];
};

void usage(){
	printf(	"Usage: reportzones [-?] [-o offset] [-n maxzones] dev\n"
		"	-?	: Print out usage\n"
		"	-o	: Offset of first zone to list (default: 1).  Optional.\n"
		"	-n	: # of zones to list (default: to last zone).  Optional.\n"
		//"	-r	: Reporting options, 0x00 to 0x3F (default 0x00).  Optional.\n"
		"	-c	: Print raw zone table in CSV format.  Optional.\n"
		"	dev	: The device handle to open (e.g. /dev/sdb).  Required.\n"
	);
}

int main(int argc, char * argv[])
{
	int opt;
	int sg_fd;
	uint8_t cdb[ATA_PASS_THROUGH_16_LEN];
	int zoneOffset = 1;
	int maxReqZones = 0;
	uint8_t reportingOptions = 0;	// TODO: Implement reporting options
	bool csvOutput = false;
	sg_io_hdr_t io_hdr;
	struct ReportZonesHeader zoneHeader;
	uint8_t zoneHeaderBuff[512];	// Although the header is 64 bytes, we must retrieve at minimum one sector

	while ((opt = getopt (argc, argv, "o:n:r:c?")) != -1){
		char* endPtr;
		switch (opt){
			case 'o':
				zoneOffset = strtol(optarg,&endPtr,0);
				if (*endPtr!='\0' || zoneOffset <= 0){
					fprintf(stderr, "Invalid -o argument.  Use -? for usage.\n");
					return 1;
				}
				break;
			case 'n':
				maxReqZones = strtol(optarg,&endPtr,0);
				if (*endPtr!='\0' || maxReqZones <= 0){
					fprintf(stderr, "Invalid -n argument.  Use -? for usage.\n");
					return 1;
				}
				break;
			case 'r':
				reportingOptions = strtol(optarg,&endPtr,0);
				if (*endPtr!='\0' || reportingOptions < 0 || reportingOptions > 0x3f){
					fprintf(stderr, "Invalid -r argument.  Use -? for usage.\n");
					return 1;
				}
				break;
			case 'c':
				csvOutput = true;
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

	//Issue a one-sector REPORT ZONES DMA command to retrieve zone list length
	memset(cdb, 0, sizeof(cdb));
	cdb[0] = ATA_PASS_THROUGH_16;
	cdb[1] = (0x6 << 1) | 0x1;	// DMA in, 48-bit ATA command
	cdb[2] = (1<<3) | (1<<2) | 0x2;	// (T_DIR|BYT_BLOK|T_LENGTH=2): Transfer n 512-byte blocks from device, where n is sector count
	cdb[4] = 0x00;			// ACTION: 00h
	cdb[6] = 1;			// Retrieve 1 page
	cdb[13] = 0x1<<6;		// Device bit 6 "shall be set to one"
	cdb[14] = ATA_REPORT_ZONES_DMA;
	memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
	io_hdr.interface_id = 'S';
	io_hdr.cmd_len = sizeof(cdb);
	io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	io_hdr.dxfer_len = 512;
	io_hdr.dxferp = zoneHeaderBuff;
	io_hdr.cmdp = cdb;
	io_hdr.timeout = 15000;
	if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
		perror("ioctl error");
		close(sg_fd);
		return 1;
	}
	zoneHeader = *(struct ReportZonesHeader*)zoneHeaderBuff;
	uint32_t numZones = zoneHeader.zoneListLength/sizeof(struct ReportZonesEntry);
	
	if (numZones < 1){
		fprintf(stderr, "Error: Device reported %d zones, is this a ZAC drive?\n", numZones);
		return 1;
	}
	if (zoneOffset > numZones){
		fprintf(stderr, "Error: Invalid zone offset (%d)\n", zoneOffset);
		return 1;
	}
	// Request up to the last zone if -n argument would exceed zone range, or is missing
	if (zoneOffset-1 + maxReqZones > numZones){
		fprintf(stderr, "Warning: Requested zone range (%d-%d) exceeds number of zones (%d); truncating to max zone\n", zoneOffset, zoneOffset-1+maxReqZones, numZones);
		maxReqZones = numZones-zoneOffset+1;
	} else if (maxReqZones == 0){
		maxReqZones = numZones-zoneOffset+1;
	}

	struct ReportZonesEntry zoneTable[maxReqZones];
	char parameterDataBuff[sizeof(struct ReportZonesHeader) + sizeof(struct ReportZonesEntry)*REPORT_ZONES_ENTRY_BUFFER_SIZE];
	struct ReportZonesEntry* zoneEntries;
	
	memset(&zoneTable, 0, sizeof(zoneTable));
	
	// Calculate the zone start LBA for target offset zone
	uint16_t pagesRequested = sizeof(parameterDataBuff)/512 + (sizeof(parameterDataBuff)%512 == 0 ? 0 : 1);
	uint64_t zoneStartLba = 0;	//Current zone start offset
	uint8_t sameOption = zoneHeader.options & 0xF;
	switch (sameOption){
		default:
			fprintf(stderr,"Warning: Unrecognized 'same' option in REPORT ZONES DMA header (%d)", sameOption);
		case 0x0:
			// If zone lengths differ, we need to spool through preceding zones in chunks to reach offset zone (in order to retrieve correct zone start LBAs)
			for (int i=1; i<zoneOffset; i+=REPORT_ZONES_ENTRY_BUFFER_SIZE){
				memset(&parameterDataBuff, 0, sizeof(parameterDataBuff));
				memset(cdb, 0, sizeof(cdb));
				cdb[0] = ATA_PASS_THROUGH_16;
				cdb[1] = (0x6 << 1) | 0x01;
				cdb[2] = (1<<3) | (1<<2) | 0x2;
				cdb[4] = 0x00;
				cdb[5] = (pagesRequested>>8);
				cdb[6] = pagesRequested&0xff;
				cdb[7] = (zoneStartLba>>24)&0xff;
				cdb[8] = (zoneStartLba&0xff);
				cdb[9] = (zoneStartLba>>32)&0xff;
				cdb[10] = (zoneStartLba>>8)&0xff;
				cdb[11] = (zoneStartLba>>40)&0xff;
				cdb[12] = (zoneStartLba>>16)&0xff;
				cdb[13] = 0x1<<6;
				cdb[14] = ATA_REPORT_ZONES_DMA;
				memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
				io_hdr.interface_id = 'S';
				io_hdr.cmd_len = sizeof(cdb);
				io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
				io_hdr.dxfer_len = sizeof(parameterDataBuff);
				io_hdr.dxferp = parameterDataBuff;
				io_hdr.cmdp = cdb;
				io_hdr.timeout = 15000;
				if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
					perror("ioctl error");
					close(sg_fd);
					return 1;
				}
				zoneEntries = (struct ReportZonesEntry*)(&parameterDataBuff[sizeof(struct ReportZonesHeader)]);
				if (i+REPORT_ZONES_ENTRY_BUFFER_SIZE >= zoneOffset){
					// Target zone reached, calculate zone LBA offset from previous zone
					int32_t idx = (zoneOffset - 2) % REPORT_ZONES_ENTRY_BUFFER_SIZE;
					zoneStartLba = zoneEntries[idx].zoneStartLba + zoneEntries[idx].zoneLength;
				} else {
					// Update zone LBA offset for next chunk retrieval
					zoneStartLba = zoneEntries[REPORT_ZONES_ENTRY_BUFFER_SIZE-1].zoneStartLba + zoneEntries[REPORT_ZONES_ENTRY_BUFFER_SIZE-1].zoneLength;
				}
			}
			break;
		case 0x1:
		case 0x2:
		case 0x3:
			// Zone lengths are same as first zone, so we can retrieve the zone length from zone 1 and calculate correct offset LBA
			uint64_t zoneLength = (*(struct ReportZonesEntry*) &((struct ReportZonesHeader*)zoneHeaderBuff)[1]).zoneLength;
			zoneStartLba = (zoneOffset-1) * zoneLength;
			break;
	}

	// Get zone entries in chunks starting from detected LBA offset
	for (uint32_t i=0; i<maxReqZones; i+=REPORT_ZONES_ENTRY_BUFFER_SIZE){
		memset(&parameterDataBuff, 0, sizeof(parameterDataBuff));
		memset(cdb, 0, sizeof(cdb));
		cdb[0] = ATA_PASS_THROUGH_16;
		cdb[1] = (0x6 << 1) | 0x01;
		cdb[2] = (1<<3) | (1<<2) | 0x2;
		cdb[3] = 0;
		cdb[4] = 0x00;
		cdb[5] = (pagesRequested>>8);
		cdb[6] = pagesRequested&0xff;
		cdb[7] = (zoneStartLba>>24)&0xff;
		cdb[8] = (zoneStartLba&0xff);
		cdb[9] = (zoneStartLba>>32)&0xff;
		cdb[10] = (zoneStartLba>>8)&0xff;
		cdb[11] = (zoneStartLba>>40)&0xff;
		cdb[12] = (zoneStartLba>>16)&0xff;
		cdb[13] = 0x1<<6;
		cdb[14] = ATA_REPORT_ZONES_DMA;
		memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
		io_hdr.interface_id = 'S';
		io_hdr.cmd_len = sizeof(cdb);
		io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
		io_hdr.dxfer_len = sizeof(parameterDataBuff);
		io_hdr.dxferp = parameterDataBuff;
		io_hdr.cmdp = cdb;
		io_hdr.timeout = 15000;
		if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
			perror("ioctl error");
			close(sg_fd);
			return 1;
		}
		// Copy the retrieved zone entries into the complete zone table, and increment start LBA
		zoneEntries = (struct ReportZonesEntry*)(&parameterDataBuff[sizeof(struct ReportZonesHeader)]);
		uint32_t min = maxReqZones-i > REPORT_ZONES_ENTRY_BUFFER_SIZE ? REPORT_ZONES_ENTRY_BUFFER_SIZE : maxReqZones-i;
		memcpy(&zoneTable[i], zoneEntries, sizeof(struct ReportZonesEntry)*min);
		zoneStartLba = zoneEntries[REPORT_ZONES_ENTRY_BUFFER_SIZE-1].zoneStartLba + zoneEntries[REPORT_ZONES_ENTRY_BUFFER_SIZE-1].zoneLength;
	}

	close(sg_fd);

	// Parse and output zone information
	if (csvOutput){
		printf("Zone List Length, Total Zones, Options, Maximum Number of Open Sequential Write Required Zones, Unreliable Sector Count\n");
		printf("%u,%u,%#x,%d,%u\n",zoneHeader.zoneListLength,numZones,zoneHeader.options,zoneHeader.maxOpenSeqZones,zoneHeader.unreliableSectors);
		printf("Zone,Zone Start LBA,Zone Length,Write Pointer,Checkpoint,Option Flags,Zone Type,Zone Condition,Reset\n");
	} else {
		printf("Report Log header\n");
		printf("---------------------------------------------------------------\n");
		printf(" Zone list length    : %10d bytes\n",zoneHeader.zoneListLength);
		printf(" Total zones         : %10d zones\n",numZones);
		printf(" Options             :     0x%04x\n",zoneHeader.options);
		printf(" Max open seq. req.  : %10d zones\n",zoneHeader.maxOpenSeqZones);
		printf(" Unreliable sectors  : %10d sectors\n",zoneHeader.unreliableSectors);
		printf("---------------------------------------------------------------\n");
		printf("\nReport Log zone Entries\n");
		printf("|-------------------------------------------------------------------------------------|\n");
		printf("| Zone|  Start LBA  | Zone Length |  Write Ptr  |  Checkpoint | Type | Zone Condition |\n");
	}
	uint32_t zoneId = 0;
	uint64_t startLba = 0;
	uint64_t zoneLength = 0;
	uint64_t writePointer = 0;
	uint64_t checkpoint = 0;
	uint16_t optionFlag = 0;
	uint8_t zoneType = 0;
	uint8_t zoneCon = 0;
	uint8_t resetBit = 0;
	for (uint32_t i=0; i<maxReqZones; i++){
		zoneId = i+zoneOffset;
		startLba = zoneTable[i].zoneStartLba;
		zoneLength = zoneTable[i].zoneLength;
		writePointer = zoneTable[i].writePointer;
		checkpoint = zoneTable[i].checkpoint;
		optionFlag = zoneTable[i].options;
		zoneType = (optionFlag >> 8) & 0xF;
		zoneCon = (optionFlag >> 4) & 0xF;
		resetBit = optionFlag & 0x1;
		if (csvOutput){
			printf("%u,%#lx,%#lx,%#lx,%#lx,%#x,%#x,%#x,%u\n",zoneId,startLba,zoneLength,writePointer,checkpoint,optionFlag,zoneType,zoneCon,resetBit);
		} else {
			printf("|%5u|%12lXh|%12lXh|%12lXh|%12lXh|",zoneId,startLba,zoneLength,writePointer,checkpoint);
			switch (zoneType){
				case 1:
					printf("  CMR ");
					break;
				case 2:
					printf("  SMR ");
					break;
				default:
					printf(" ???? ");
					break;
			}
			printf("|");
			switch (zoneCon){
				case 0:
					printf("  NO_WP  ");
					break;
				case 1:
					printf("  EMPTY  ");
					break;
				case 2:
					printf(" IMP OPEN");
					break;
				case 4:
					printf("  CLOSED ");
					break;
				case 0xE:
					printf("  FULL   ");
					break;
				default:	//Reserved
					printf(" ??????? ");
					break;
			}
			if (resetBit == 1){
				printf(" RESET ");
			} else {
				printf("       ");
			}
			printf("|\n");
		}
	}
	if (!csvOutput){
		printf("|-------------------------------------------------------------------------------------|\n");
	}

	return 0;
}

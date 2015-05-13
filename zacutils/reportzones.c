/**
 * (c) 2015 Western Digital Technologies, Inc. All rights reserved.
 * Front-end tool for the ATA REPORT ZONES DMA command
 * Compliant to ZAC Specification draft, revision 0.8n (March 4, 2015)
 * Author: Austin Liou (austin.liou@wdc.com)
 */
#include "common.h"

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
		"	-r	: Reporting options, 0x00 to 0x07, 0x10, or 0x3F (default 0x00).  Optional.\n"
		"	-c	: Print raw zone table in CSV format.  Optional.\n"
		"	dev	: The device handle to open (e.g. /dev/sdb).  Required.\n"
	);
}

int main(int argc, char * argv[])
{
	int opt;
	int sg_fd;

	int zoneOffset = 1;
	int maxReqZones = 0;
	uint8_t reportingOptions = 0;
	bool csvOutput = false;

	uint8_t dataBuff[sizeof(struct ReportZonesHeader) + sizeof(struct ReportZonesEntry)*REPORT_ZONES_ENTRY_BUFFER_SIZE];
	struct ReportZonesHeader zoneHeader = {0};
	struct ReportZonesEntry* zoneEntries = {0};

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
				if (*endPtr!='\0' || reportingOptions > 0x3f || (reportingOptions >= 0x08 && reportingOptions <= 0x0f) || (reportingOptions >= 0x12 && reportingOptions <= 0x3e)){
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

	//Issue a one-sector REPORT ZONES DMA command to retrieve full header, and make sure command works
	uint8_t zoneHeaderBuff[512] = {0};	// Although the header is 64 bytes, we must retrieve at minimum one sector
	uint8_t senseBuff[32] = {0};
	if (!ataPassthrough16(
		sg_fd,
		ATA_REPORT_ZONES_DMA,
		0x0000,	// ACTION: 00h
		1,	// Retrieve 1 page
		0x0,
		0x1<<6,	// Device bit 6 "shall be set to one"
		ATA_PROTOCOL_DMA,
		ATA_FLAGS_CKCOND | ATA_FLAGS_TDIR | ATA_FLAGS_BYTBLK | ATA_FLAGS_TLEN_SECC,	// Transfer n 512-byte blocks from device, where n is sector count
		SG_DXFER_FROM_DEV,
		zoneHeaderBuff,
		sizeof(zoneHeaderBuff),
		senseBuff,
		sizeof(senseBuff)
	)){ return 1; }

	// Check if command failed to make sure this is a ZAC drive
	struct KeyCodeQualifier kcq;
	if (!getSenseErrors(senseBuff, &kcq)){
		fprintf(stderr, "Error: Could not parse sense buffer from REPORT ZONES DMA command\n");
		close(sg_fd);
		return 1;
	}
	if (kcq.senseKey == 0x0b && kcq.asc == 0x00 && kcq.ascq == 0x00){
		fprintf(stderr, "Error: Command was aborted, is this a ZAC drive?\n");
		close(sg_fd);
		return 1;
	}

	zoneHeader = *(struct ReportZonesHeader*)zoneHeaderBuff;
	uint64_t globalZoneLength = (*(struct ReportZonesEntry*) &((struct ReportZonesHeader*)zoneHeaderBuff)[1]).zoneLength;
	uint32_t totalNumZones = zoneHeader.zoneListLength/sizeof(struct ReportZonesEntry);

	if (zoneOffset > totalNumZones){
		fprintf(stderr, "Error: Invalid zone offset (%d)\n", zoneOffset);
		return 1;
	}
	
	// Calculate the zone start LBA for target offset zone
	uint16_t pagesRequested = sizeof(dataBuff)/512 + (sizeof(dataBuff)%512 == 0 ? 0 : 1);
	uint64_t offsetLba = 0;	//Current zone start offset
	uint8_t sameOption = zoneHeader.options & 0xF;
	switch (sameOption){
		default:
			fprintf(stderr,"Warning: Unrecognized 'same' option in REPORT ZONES DMA header (%d)", sameOption);
		case 0x0:
			// If zone lengths differ, we need to spool through preceding zones in chunks to reach offset zone (in order to retrieve correct zone start LBAs)
			for (int i=1; i<zoneOffset; i+=REPORT_ZONES_ENTRY_BUFFER_SIZE){
				if (!ataPassthrough16(
					sg_fd,
					ATA_REPORT_ZONES_DMA,
					0x0000,	//Do not filter with reporting options, since we need the previous zone
					pagesRequested,
					offsetLba,
					0x1<<6,
					ATA_PROTOCOL_DMA,
					ATA_FLAGS_TDIR | ATA_FLAGS_BYTBLK | ATA_FLAGS_TLEN_SECC,
					SG_DXFER_FROM_DEV,
					dataBuff,
					sizeof(dataBuff),
					NULL,
					0
				)){ return 1; }
				zoneEntries = (struct ReportZonesEntry*)(&dataBuff[sizeof(struct ReportZonesHeader)]);
				if (i+REPORT_ZONES_ENTRY_BUFFER_SIZE >= zoneOffset){
					// Target zone reached, calculate zone LBA offset from previous zone
					int32_t idx = (zoneOffset - 2) % REPORT_ZONES_ENTRY_BUFFER_SIZE;
					offsetLba = zoneEntries[idx].zoneStartLba + zoneEntries[idx].zoneLength;
				} else {
					// Update zone LBA offset for next chunk retrieval
					offsetLba = zoneEntries[REPORT_ZONES_ENTRY_BUFFER_SIZE-1].zoneStartLba + zoneEntries[REPORT_ZONES_ENTRY_BUFFER_SIZE-1].zoneLength;
				}
			}
			break;
		case 0x1:
		case 0x2:
		case 0x3:
			// Zone lengths are same as first zone, so we can calculate correct offset LBA
			offsetLba = (zoneOffset-1) * globalZoneLength;
			break;
	}

	// Now that we have a zone start LBA, we can re-retrieve the header to get the actual number of zones after filtering and offset
	if (!ataPassthrough16(
		sg_fd,
		ATA_REPORT_ZONES_DMA,
		(reportingOptions << 8) | 0x00,
		1,
		offsetLba,
		0x1<<6,
		ATA_PROTOCOL_DMA,
		ATA_FLAGS_CKCOND | ATA_FLAGS_TDIR | ATA_FLAGS_BYTBLK | ATA_FLAGS_TLEN_SECC,
		SG_DXFER_FROM_DEV,
		zoneHeaderBuff,
		sizeof(zoneHeaderBuff),
		NULL,
		0
	)){ return 1; }
	zoneHeader = *(struct ReportZonesHeader*)zoneHeaderBuff;

	// Parse number of zones in table
	uint32_t numZones = zoneHeader.zoneListLength/sizeof(struct ReportZonesEntry);
	if (numZones == 0){
		printf("Device reported 0 zones (with reporting options %#02x)\n", reportingOptions);
		return 0;
	}

	if (maxReqZones > numZones){
		fprintf(stderr, "Warning: Requested number of zones (%u) exceeds number of reported zones (%u), with reporting options %#02x\n", maxReqZones, numZones, reportingOptions);
		maxReqZones = numZones;
	}
	if (maxReqZones == 0){
		maxReqZones = numZones;
	}

	// Get zone entries in chunks starting from detected LBA offset
	struct ReportZonesEntry zoneTable[maxReqZones];
	memset(&zoneTable, 0, sizeof(zoneTable));
	uint32_t numRecordsRetrieved;
	uint64_t currLba = offsetLba;
	for (uint32_t i=0; i<maxReqZones; i+= REPORT_ZONES_ENTRY_BUFFER_SIZE){
		memset(&dataBuff, 0, sizeof(dataBuff));
		if (!ataPassthrough16(
			sg_fd,
			ATA_REPORT_ZONES_DMA,
			(reportingOptions << 8) | 0x00,
			pagesRequested,
			offsetLba,
			0x1<<6,
			ATA_PROTOCOL_DMA,
			ATA_FLAGS_TDIR | ATA_FLAGS_BYTBLK | ATA_FLAGS_TLEN_SECC,
			SG_DXFER_FROM_DEV,
			dataBuff,
			sizeof(dataBuff),
			NULL,
			0
		)){ return 1; }
		
		// Copy the retrieved zone entries into the complete zone table, and increment start LBA
		zoneEntries = (struct ReportZonesEntry*)(&dataBuff[sizeof(struct ReportZonesHeader)]);
		numRecordsRetrieved = (*(struct ReportZonesHeader*)dataBuff).zoneListLength / sizeof(struct ReportZonesEntry);
		uint32_t min = maxReqZones-i > REPORT_ZONES_ENTRY_BUFFER_SIZE ? REPORT_ZONES_ENTRY_BUFFER_SIZE : maxReqZones-i;
		memcpy(&zoneTable[i], zoneEntries, sizeof(struct ReportZonesEntry)*min);
		currLba = zoneEntries[REPORT_ZONES_ENTRY_BUFFER_SIZE-1].zoneStartLba + zoneEntries[REPORT_ZONES_ENTRY_BUFFER_SIZE-1].zoneLength;
	}

	close(sg_fd);

	// Parse and output zone information
	if (csvOutput){
		printf("Offset LBA,Requested Zone Count,Reporting Options\n");
		printf("%#lx,%u,%#x\n",offsetLba,maxReqZones,reportingOptions);
		printf("Zone List Length,Number of Zones,Offset LBA,Reporting Options,Options,Maximum Number of Open Sequential Write Required Zones,Unreliable Sector Count\n");
		printf("%u,%u,%#lx,%#x,%#x,%d,%u\n",zoneHeader.zoneListLength,numZones,offsetLba,reportingOptions,zoneHeader.options,zoneHeader.maxOpenSeqZones,zoneHeader.unreliableSectors);
		printf("Zone,Zone Start LBA,Zone Length,Write Pointer,Checkpoint,Option Flags,Zone Type,Zone Condition,Reset\n");
	} else {
		printf("Inputs\n");
		printf("------------------------------------------\n");
		printf(" Offset LBA: %lXh\n",offsetLba);
		printf(" Requested zone count: %u\n",maxReqZones);
		printf(" Reporting options: %02Xh\n",reportingOptions);
		printf("------------------------------------------\n");
		printf("\nReport Log header\n");
		printf("------------------------------------------\n");
		printf(" Zone list length   :  %10u bytes\n",zoneHeader.zoneListLength);
		printf(" Number of Zones    :  %10u zones\n",numZones);
		printf(" Options            :        %04x h\n",zoneHeader.options);
		printf(" Max open seq. req. :  %10d zones\n",zoneHeader.maxOpenSeqZones);
		printf(" Unreliable sectors :  %10u sectors\n",zoneHeader.unreliableSectors);
		printf("------------------------------------------\n");
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
		startLba = zoneTable[i].zoneStartLba;
		zoneLength = zoneTable[i].zoneLength;
		// If zone lengths are equal, we can reliably calculate zone ID for user convenience.  Else, just enumerate as reported.
		if (globalZoneLength != 0){
			zoneId = (startLba/globalZoneLength)+1;	// Make sure this casts correctly (uint64_t to uint32_t)
		} else {
			zoneId = i+1;
		}
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
	if(sameOption == 0x00){
		fprintf(stderr, "WARNING: Zone sizes may differ, so zone IDs may not reflect actual zone number");
	}
	return 0;
}

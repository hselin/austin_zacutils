/**
 * (c) 2015 Western Digital Technologies, Inc. All rights reserved.
 * Header for common functions and definitions for zacutils
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
#define ATA_RESET_WRITE_POINTER 0x9f
#define ATA_REQUEST_SENSE_DATA_EXT 0x0b

/// SCSI Key Code Qualifier
struct KeyCodeQualifier {
	uint8_t senseKey;
	uint8_t asc;
	uint8_t ascq;
};

/// ATA Status Return Descriptor (return registers)
struct AtaStatusReturnDescriptor {
	uint8_t status;
	uint8_t error;
	uint16_t sectorCount;
	uint16_t lbaLow;
	uint16_t lbaMid;
	uint16_t lbaHigh;
	uint8_t device;
};

bool getSenseErrors(uint8_t* senseBuff, struct KeyCodeQualifier* kcq);
bool senseToAtaRegisters(uint8_t* senseBuff, struct AtaStatusReturnDescriptor* descriptor);

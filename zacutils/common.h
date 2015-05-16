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
#include <stdbool.h>
#include <scsi/sg.h>
#include <scsi/scsi.h>

/// ATA Command Pass-Through definitions
#define ATA_PASS_THROUGH_16 0x85
#define ATA_PASS_THROUGH_16_LEN 16

#define ATA_RETURN_DESCRIPTOR_CODE 0x09
#define ATA_RETURN_DESCRIPTOR_LEN 0x0c

#define SG_IO_TIMEOUT 10000

/// ATA PASS-THROUGH(16) byte 2
enum AtaPassthroughFlags {
	ATA_FLAGS_TLEN_SECC = 0x02,
	ATA_FLAGS_BYTBLK = 0x1<<2,
	ATA_FLAGS_TDIR = 0x1<<3, 
	ATA_FLAGS_CKCOND = 0x1<<5
};

/// ATA PASS-THROUGH(16) protocols (4 bits)
enum AtaProtocols {
	ATA_PROTOCOL_NONDATA	= 0x3,
	ATA_PROTOCOL_DMA	= 0x6
};

/// ATA COMMAND register (16 bits)
enum AtaCommands {
	ATA_REQUEST_SENSE_DATA_EXT	= 0x0b,
	ATA_REPORT_ZONES_DMA		= 0x4a,
	ATA_RESET_WRITE_POINTER		= 0x9f
};

/// SCSI Sense response codes
enum SenseResponseCodes {
	SCSI_FIXED_CURR = 0x70,
	SCSI_FIXED_PREV = 0x71,
	SCSI_DESCRIPTOR_CURR = 0x72,
	SCSI_DESCRIPTOR_PREV = 0x73
};

/// SCSI Key Code Qualifier
struct KeyCodeQualifier {
	uint8_t senseKey;
	uint8_t asc;
	uint8_t ascq;
};

/// SCSI Sense ASC/ASCQ values joined in a 16-bit value.
enum SenseAscValues {
	ASC_NO_ADDITIONAL_SENSE_INFORMATION	= 0x0000,
	ASC_INVALID_FIELD_IN_CDB		= 0x2400,
	ASC_ZONE_IS_READ_ONLY			= 0x2708,
	ASC_RESET_WRITE_POINTER_NOT_ALLOWED	= 0x2c0d
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

bool assertKcq(struct KeyCodeQualifier* kcq, uint8_t senseKey, enum SenseAscValues asc);
bool getSenseErrors(uint8_t* senseBuff, struct KeyCodeQualifier* kcq);
bool senseToAtaRegisters(uint8_t* senseBuff, struct AtaStatusReturnDescriptor* descriptor);
bool ataPassthrough16(
	int* sg_fd,
	uint8_t cmd,
	uint16_t features,
	uint16_t count,
	uint64_t lba,
	uint8_t device,
	uint8_t protocol,
	uint8_t flags,
	int dxfer_dir,
	uint8_t* dxferp,
	unsigned int dxfer_len,
	uint8_t* sbp,
	unsigned char mx_sb_len
);

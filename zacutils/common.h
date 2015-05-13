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

/// ATA Command Pass-Through definitions
#define ATA_PASS_THROUGH_16 0x85
#define ATA_PASS_THROUGH_16_LEN 16

#define ATA_FLAGS_TLEN_SECC 0x02
#define ATA_FLAGS_BYTBLK (0x1<<2)
#define ATA_FLAGS_TDIR (0x1<<3)
#define ATA_FLAGS_CKCOND (0x1<<5)

#define ATA_PROTOCOL_NONDATA 0x3
#define ATA_PROTOCOL_DMA 0x6

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
bool ataPassthrough16(
	int& sg_fd,
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

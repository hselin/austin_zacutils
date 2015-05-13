/**
 * (c) 2015 Western Digital Technologies, Inc. All rights reserved.
 * Common functions and definitions for zacutils
 * Author: Austin Liou (austin.liou@wdc.com)
 */
#include "common.h"

/// Issue an ATA PASS-THROUGH (16)
bool ataPassthrough16(int& sg_fd, uint8_t cmd, uint16_t features, uint16_t count, uint64_t lba, uint8_t device, uint8_t protocol, uint8_t flags, int dxfer_dir, uint8_t* dxferp, unsigned int dxfer_len, uint8_t* sbp, unsigned char mx_sb_len){
	uint8_t cdb[ATA_PASS_THROUGH_16_LEN] = {0};
	sg_io_hdr_t io_hdr = {0};
	cdb[0] = ATA_PASS_THROUGH_16;
	cdb[1] = (protocol << 1) | 0x01;
	cdb[2] = flags;
	cdb[3] = features >> 8;
	cdb[4] = features & 0xff;
	cdb[5] = count >> 8;
	cdb[6] = count & 0xff;
	cdb[7] = (lba>>24)&0xff;
	cdb[8] = lba&0xff;
	cdb[9] = (lba>>32)&0xff;
	cdb[10] = (lba>>8)&0xff;
	cdb[11] = (lba>>40)&0xff;
	cdb[12] = (lba>>16)&0xff;
	cdb[13] = device;
	cdb[14] = cmd;
	io_hdr.interface_id = 'S';
	io_hdr.cmdp = cdb;
	io_hdr.cmd_len = sizeof(cdb);
	io_hdr.dxfer_direction = dxfer_dir;
	io_hdr.dxferp = dxferp;
	io_hdr.dxfer_len = dxfer_len;
	io_hdr.sbp = sbp;
	io_hdr.mx_sb_len = mx_sb_len;
	io_hdr.timeout = 10000;
	if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
		perror("ioctl error");
		close(sg_fd);
		return false;
	}
	return true;
}

/// Parse out SCSI Key Code Qualifier from sense buffer.  Returns success.
bool getSenseErrors(uint8_t* senseBuff, struct KeyCodeQualifier* kcq){
	uint8_t senseError = senseBuff[0] & 0x7F;
	switch (senseError){
		case 0x70:
		case 0x71:
			//Fixed format sense data
			kcq->senseKey = senseBuff[2] & 0xF;
			kcq->asc = senseBuff[12];
			kcq->ascq = senseBuff[13];
			break;
		case 0x72:
		case 0x73:
			//Descriptor format sense data
			kcq->senseKey = senseBuff[1] & 0xF;
			kcq->asc = senseBuff[2];
			kcq->ascq = senseBuff[3];
			break;
		default:
			return false;
	}
	return true;
}

/// Parse out ATA Status Return Descriptor from sense buffer.  Returns success.
bool senseToAtaRegisters(uint8_t* senseBuff, struct AtaStatusReturnDescriptor* descriptor){
	uint8_t senseError = senseBuff[0] & 0x7F;
	if (senseError != 0x72 && senseError != 0x73){	// Only support descriptor format sense
		return false;
	}
	uint8_t* descTable = &(senseBuff[8]);
	if (descTable[0] != 0x09 || descTable[1] != 0x0c){	// Invalid header
		return false;
	}
	descriptor->status = descTable[13];
	descriptor->error = descTable[3];
	descriptor->sectorCount = (descTable[4]<<8) | descTable[5];
	descriptor->lbaLow = (descTable[6]<<8) | descTable[7];
	descriptor->lbaMid = (descTable[8]<<8) | descTable[9];
	descriptor->lbaHigh = (descTable[10]<<8) | descTable[11];
	descriptor->device = descTable[12];
	// Invalidate the upper bytes of SECC/LBAL/LBAM/LBAH if extend bit isn't set
	bool ext = descTable[2] & 0x01;
	if (!ext){
		descriptor->sectorCount &= 0xff;
		descriptor->lbaLow &= 0xff;
		descriptor->lbaMid &= 0xff;
		descriptor->lbaHigh &= 0xff;
	}
	return true;
}

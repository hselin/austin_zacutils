/**
 * (c) 2015 Western Digital Technologies, Inc. All rights reserved.
 * Common functions and definitions for zacutils
 * Author: Austin Liou (austin.liou@wdc.com)
 */
#include "common.h"

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

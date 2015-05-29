/**
 * (c) 2015 Western Digital Technologies, Inc. All rights reserved.
 * Header for front-end tool for the ATA RESET WRITE POINTER command
 * Compliant to ZAC Specification draft, revision 0.8n (March 4, 2015)
 * Author: Austin Liou (austin.liou@wdc.com)
 */
#include "common.h"

#define RESET_ALL_BIT (1<<8)

/// ZONE MANAGEMENT actions (currently the 0.8n spec only lists one)
enum ZoneMgmtActions {
	ACTION_RESET_WRITE_POINTER = 0x04
};

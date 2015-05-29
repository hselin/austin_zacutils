/**
 * (c) 2015 Western Digital Technologies, Inc. All rights reserved.
 * Header for front-end tool for the ATA REPORT ZONES DMA command
 * Compliant to ZAC Specification draft, revision 0.8n (March 4, 2015)
 * Author: Austin Liou (austin.liou@wdc.com)
 */

#include "common.h"

/// Size of buffer, in zone entries.  This will cause ioctl to bug out if too large.
/// The actual buffer size will be larger to include the header
#define REPORT_ZONES_ENTRY_BUFFER_SIZE 2047

/// Absolute maximum number of zones supported by spec (uint32max/64)
#define MAX_ZONES 0x3FFFFFF

/// REPORT ZONES DMA header (64 bytes)
struct ReportZonesHeader {
	uint32_t zoneListLength;
	uint16_t options;
	uint8_t _reserved1[2];
	uint32_t maxOpenSeqZones;	// Maximum Number of Open Sequential Write Required Zones
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

/// "SAME" option in REPORT ZONES DMA header.
/// These don't actually have names so these may be subject to change
enum SameOptions {
	SAMEOPT_ALLDIFF = 0x0,		// All zone types/length different
	SAMEOPT_FIRSTSAME = 0x1,	// All zone types/length same as first zone
	SAMEOPT_LASTDIFF = 0x2,		// All zone types/length same as first zone, except last zone length
	SAMEOPT_TYPEDIFF = 0x3		// All zone lengths same as first zone; zone types different
};

/// Zone Type
enum ZoneTypes {
	ZONETYPE_CMR = 0x1,
	ZONETYPE_SMR = 0x2
};

/// Zone Condition
enum ZoneConditions {
	ZONECOND_NO_WP = 0x0,		// Zone has no write pointer (CMR)
	ZONECOND_EMPTY = 0x1,		// ZC1 Empty state
	ZONECOND_IMP_OPEN = 0x2,	// ZC2 Implicit Open state
	ZONECOND_CLOSED = 0x4,		// ZC4 Closed state
	ZONECOND_FULL = 0xe		// ZC5 Full state
};

/// Reporting Options (for filtering which zones to report)
enum ReportingOptions {
	ROPT_ALL = 0x00,	// All zones
	ROPT_EMPTY = 0x01,	// Empty zones
	ROPT_IMPOPEN = 0x02,	// Implicitly open zones
	ROPT_EXPOPEN = 0x03,	// Explicitly open zones
	ROPT_CLOSED = 0x04,	// Closed zones
	ROPT_FULL = 0x05,	// Full zones
	ROPT_RDONLY = 0x06,	// Read-only zones
	ROPT_OFFLINE = 0x07,	// Offline zones
	ROPT_RESET = 0x10,	// Zones with RESET bit set
	ROPT_NOWP = 0x3f	// Zones with no write pointer (e.g. CMR)
};

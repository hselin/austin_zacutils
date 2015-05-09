# zacutils

*(c) 2015 Western Digital Technologies, Inc. All rights reserved.*

**zacutils** is a set of command-line tools to issue Zoned ATA Commands (ZAC) in Linux.  The commands currently supported are:

- ATA REPORT ZONES DMA (4Ah)
- ATA RESET WRITE POINTER (9Fh)

Currently the tools are based on the ZAC Specification draft, revision 0.8n (March 4, 2015).

## Prerequisites
Linux-based environment with g++ installed.  For usage, the target HDD must be ZAC-compliant.

## Compilation
A makefile is included; simply type `make` within the working directory to compile all binaries.  To compile individual tools, you can issue `make reportzones`, `make resetzones`, etc.  To clean up, type `make clean`.

## Usage
You can run the tools with the `-?` flag to view usage details.

* **reportzones** [-?] [-o *zoneoffset*] [-n *numzones*] *device*
 * -? : Print out usage.
 * -o : Offset of first zone to list (default: 1).  Optional.
 * -n : Number of zones to list (default: to last zone).  Optional.
 * -c : Print out zone table in CSV format.  Optional.
 * device : Device handle to open (e.g. /dev/sdb).  Required.
* **resetzones** [-?] [-l *zonestartlba*] *device*
 * -? : Print out usage.
 * -l : First LBA of zone to reset.  Optional.  If omitted, will reset ALL zones.
 * device : Device handle to open (e.g. /dev/sdb).  Required.

## Known Issues
* **resetzones** does not retrieve the correct sense error codes for certain failures.  Instead, you may see a failure with (SK=0x0b, ASC=0x00, ASCQ=0x00).
* **reportzones** does not currently support reporting options.

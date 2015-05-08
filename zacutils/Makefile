# (c) 2015 Western Digital Technologies, Inc. All rights reserved.
# Makefile for ZAC Zone Management Tools.
#
# Type 'make' to create all binaries
# Or 'make reportzones' or 'make resetzones' for individual binaries
# Type 'make clean' to delete all temporaries.
#

CC = g++
CFLAGS = -O3 
OUT_DIR = .
LIBS =

default: reportzones resetzones

reportzones: $(OUT_DIR)/reportzones.c.o
	@echo -n 'Linking reportzones... '
	@$(CC) $(CFLAGS) -o reportzones $(OUT_DIR)/reportzones.c.o $(LIBS)
	@echo Done.

resetzones: $(OUT_DIR)/resetzones.c.o
	@echo -n 'Linking resetzones... '
	@$(CC) $(CFLAGS) -o resetzones $(OUT_DIR)/resetzones.c.o $(LIBS)
	@echo Done.


$(OUT_DIR)/reportzones.c.o: reportzones.c
	@echo -n 'Compiling reportzones.c... '
	@$(CC) $(CFLAGS) -o $(OUT_DIR)/reportzones.c.o -c reportzones.c
	@echo Done.

$(OUT_DIR)/resetzones.c.o: resetzones.c
	@echo -n 'Compiling resetzones.c... '
	@$(CC) $(CFLAGS) -o $(OUT_DIR)/resetzones.c.o -c resetzones.c
	@echo Done.

clean:
	@echo -n 'Removing all temporary binaries... '
	@rm -f reportzones resetzones $(OUT_DIR)/*.o
	@echo Done.

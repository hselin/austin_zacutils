# (c) 2015 Western Digital Technologies, Inc. All rights reserved.
# Makefile for ZAC Zone Management Tools.
#
# Type 'make' to create all binaries
# Or 'make reportzones' or 'make resetzones' for individual binaries
# Type 'make clean' to delete all temporaries.
#

CC = gcc
CXX = g++
CFLAGS = -std=gnu99
CXXFLAGS =
CPPFLAGS = -I. -O3 -pedantic
OUT_DIR = .
LIBS =

TARGETS = reportzones resetzones
DEPS = common.h reportzones.h resetzones.h

default: $(TARGETS)

%.o: %.c $(DEPS)
	@echo -n 'Compiling $<... '
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $(OUT_DIR)/$@ $<
	@echo 'Done.'

$(TARGETS): %: %.o common.o
	@echo -n 'Linking $<... '
	@$(CC) $(CPPFLAGS) $(CFLAGS) -o $(OUT_DIR)/$@ $^ $(LIBS)
	@echo 'Done.'

.PHONY: clean
clean:
	@echo -n 'Removing all temporary binaries... '
	@rm -f $(TARGETS)
	@rm -f $(OUT_DIR)/*.o
	@echo Done.

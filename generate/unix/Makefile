#
# Common make for acpica tools and utilities
#

#
# Note: This makefile is intended to be used from within the native
# ACPICA directory structure, from under generate/unix. It specifically
# places all object files in a generate/unix subdirectory, not within
# the various ACPICA source directories. This prevents collisions
# between different compilations of the same source file with different
# compile options, and prevents pollution of the source code.
#
include Makefile.config

#
# Get the OS machine architecture. Anything with a "64" in the returned
# string will be treated as a 64-bit OS. Otherwise, the default is 32-bit.
#
HARDWARE_NAME := $(shell uname -m)

#
# Main rule will only generate versions that are appropriate for the running
# OS, either 64-bit or 32-bit.
#
all:	${PROGS}
${PROGS}: FORCE
	@cd $@; \
	if [ $(findstring 64,$(HARDWARE_NAME)) ]; then \
		mkdir -p obj64; \
		make BITS=64; \
		echo "64-bit version of $@:"; \
		ls -al obj64/$@; \
	else \
		mkdir -p obj32; \
		make BITS=32; \
		echo "32-bit version of $@:"; \
		ls -al obj32/$@; \
	fi;

#
# Make 32-bit and 64-bit versions of all the tools
#
both:	32 64

#
# Make only 32-bit versions of all the tools
#
32:	FORCE
	@for toolname in ${PROGS}; do \
		(cd $$toolname; \
		pwd; \
		mkdir -p obj32; \
		make BITS=32; \
		echo "32-bit version of $$toolname:"; \
		ls -al obj32/$$toolname \
		); \
	done;

#
# Make only 64-bit versions of all the tools
#
64:	FORCE
	@for toolname in ${PROGS}; do \
		(cd $$toolname; \
		pwd; \
		mkdir -p obj64; \
		make BITS=64; \
		echo "64-bit version of $$toolname:"; \
		ls -al obj64/$$toolname \
		); \
	done;

clean:	FORCE
	@for toolname in ${PROGS}; do \
		(cd $$toolname; \
		pwd; \
		if [ -d "obj32" ] ; then \
			make BITS=32 clean; \
			rmdir obj32; \
		fi; \
		if [ -d "obj64" ] ; then \
			make BITS=64 clean; \
			rmdir obj64; \
		fi; \
		); \
	done;

#
# Install all tools, either 32-bit or 64-bit as appropriate for the host OS
#
install:	FORCE
	@for toolname in ${PROGS}; do \
		(cd $$toolname; \
		pwd; \
		if [ $(findstring 64,$(HARDWARE_NAME)) ]; then \
			make BITS=64 install; \
			echo "Installed 64-bit version of $$toolname"; \
		else \
			make BITS=32 install; \
			echo "Installed 32-bit version of $$toolname"; \
		fi; \
		); \
	done;

machine:	FORCE
	@echo "Machine architecture: $(HARDWARE_NAME), $(XBITS)";
	@echo "Findstring: $(findstring 64, $(HARDWARE_NAME))";

FORCE:


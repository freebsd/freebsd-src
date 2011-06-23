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


all:	${PROGS}
${PROGS}: FORCE
	@cd $@; make; ls -al $@

clean:	FORCE
	@for d in ${PROGS}; do \
		(cd $$d; \
		if [ $$? -ne 0 ]; then \
			echo "Bad element of PROGS: <$$d>"; \
		else \
			pwd; make clean; \
		fi); \
	done

install:	FORCE
	@for d in ${PROGS}; do \
		(cd $$d; \
		if [ $$? -ne 0 ]; then \
			echo "Bad element of PROGS: <$$d>"; \
		else \
			pwd; make install; \
		fi); \
	done

FORCE:

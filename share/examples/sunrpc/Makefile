#
# @(#)Makefile	2.1 88/08/02 4.0 RPCSRC
#
#
#   Build all demo services
#
MAKE = make
LIB=

SUBDIR= dir msg sort

all:	${SUBDIR}

clean cleanup:
	cd dir; $(MAKE) ${MFLAGS} cleanup
	cd msg; $(MAKE) ${MFLAGS} cleanup
	cd sort; $(MAKE) ${MFLAGS} cleanup

install:
	@echo "No installations done."

${SUBDIR}:	FRC
	cd $@; $(MAKE) ${MFLAGS} LIB=$(LIB)

FRC:

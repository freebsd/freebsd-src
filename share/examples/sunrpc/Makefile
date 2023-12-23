#   Build all demo services
#
#

PACKAGE=examples
FILESDIR=${SHAREDIR}/examples/sunrpc
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

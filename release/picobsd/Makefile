# $FreeBSD: src/release/picobsd/Makefile,v 1.2 2000/02/04 07:26:22 kris Exp $
#
# Top level Makefile for PicoBSD

# Please submit any changes to the freebsd-small mailing list for
# discussion prior to committing.

MAINTAINER=	freebsd-small

# install looks like a partially completed target, 
# but I don't want to finish this work.
# FIXME
# CONFIGS	=	custom dial isp net router
CONFIGS	=	custom

all clean tidy:	
	for i in ${CONFIGS}; do \
	  (cd $$i; \
	   echo '----- Building in' $$i; \
	    make $@); \
	done
	@if [ "$@" = "clean" ]; then \
	   rm -f *~; \
        fi

floppy floppy2:
	@echo Please build this target from one of the ${CONFIGS} subdirectories.

# Build one subdirectory
${CONFIGS}:
	cd $@; make clean all

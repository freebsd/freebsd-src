# Top level Makefile for PicoBSD
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
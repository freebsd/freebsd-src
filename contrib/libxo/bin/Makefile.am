#
# Copyright 2013, Juniper Networks, Inc.
# All rights reserved.
# This SOFTWARE is licensed under the LICENSE provided in the
# ../Copyright file. By downloading, installing, copying, or otherwise
# using the SOFTWARE, you agree to be bound by the terms of that
# LICENSE.

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = gt setup.sh

GT_INSTALL_DIR = ${prefix}/bin
GT_INSTALL_FILES = gt

install-data-hook:
	@echo "Installing gt ... "
	@-mkdir -p ${GT_INSTALL_DIR}
	@for file in ${GT_INSTALL_FILES} ; do \
		if [ -f $$file ]; then \
			rfile=$$file ; \
		else \
			rfile=${srcdir}/$$file ; \
		fi ; \
		mdir=${GT_INSTALL_DIR}/ ; \
		mkdir -p $$mdir ; \
		cp $$rfile $$mdir/ ; \
        done
	@${CHMOD} a+x ${GT_INSTALL_DIR}/gt

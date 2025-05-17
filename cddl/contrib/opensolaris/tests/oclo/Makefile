#
# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms of version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source.  A copy of the CDDL is also available via the Internet at
# http://www.illumos.org/license/CDDL.
#

#
# Copyright 2025 Oxide Computer Company
#

PROGS = \
	oclo \
	oclo_errors \
	ocloexec_verify

ROOTOPTDIR = $(ROOT)/opt/os-tests/tests
ROOTOPTOCLO = $(ROOTOPTDIR)/oclo
ROOTOPTPROGS = $(PROGS:%=$(ROOTOPTOCLO)/%)

include $(SRC)/cmd/Makefile.cmd
include $(SRC)/cmd/Makefile.cmd.64
include $(SRC)/cmd/Makefile.ctf

CSTD = $(CSTD_GNU17)
CTF_MODE = link
CPPFLAGS += -D_XOPEN_SOURCE=800 -D__EXTENSIONS__

oclo		:=	LDLIBS += -lsocket
oclo_errors	:=	LDLIBS += -lsocket

.KEEP_STATE:

all: $(PROGS)

install: $(ROOTOPTPROGS)

clean:
	$(RM) *.o

$(ROOTOPTPROGS): $(PROGS) $(ROOTOPTOCLO)

$(ROOTOPTDIR):
	$(INS.dir)

$(ROOTOPTOCLO): $(ROOTOPTDIR)
	$(INS.dir)

$(ROOTOPTOCLO)/%: %
	$(INS.file)

clobber: clean
	$(RM) $(PROGS)

FRC:

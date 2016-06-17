#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
#  Copyright (c) 2000-2003 Silicon Graphics, Inc.  All rights reserved.
#

TOPDIR=../../../..
HPATH           = $(TOPDIR)/include

LIB     = ../../lib/lib.a

OBJ=fpromasm.o main.o  fw-emu.o fpmem.o klgraph_init.o
obj-y=fprom
EXTRA_CFLAGS += -DSGI_SN2
EXTRA_AFLAGS += -DSGI_SN2
AFLAGS += -DSGI_SN2

fprom: $(OBJ)
	$(LD) -static -Tfprom.lds -o fprom $(OBJ) $(LIB)

.S.o:
	$(CC)  -D__ASSEMBLY__ $(AFLAGS) $(EXTRA_AFLAGS) $(AFLAGS_KERNEL) -c -o $*.o $<

clean:
	rm -f *.o fprom


include $(TOPDIR)/Rules.make


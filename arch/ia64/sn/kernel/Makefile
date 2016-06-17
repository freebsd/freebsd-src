# arch/ia64/sn/kernel/Makefile
#
# Copyright (C) 1999,2001-2003 Silicon Graphics, Inc.  All Rights Reserved.
#
# This program is free software; you can redistribute it and/or modify it 
# under the terms of version 2 of the GNU General Public License 
# as published by the Free Software Foundation.
# 
# This program is distributed in the hope that it would be useful, but 
# WITHOUT ANY WARRANTY; without even the implied warranty of 
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
# 
# Further, this software is distributed without any warranty that it is 
# free of the rightful claim of any third person regarding infringement 
# or the like.  Any license provided herein, whether implied or 
# otherwise, applies only to this software file.  Patent licenses, if 
# any, provided herein do not apply to combinations of this program with 
# other software, or any other product whatsoever.
# 
# You should have received a copy of the GNU General Public 
# License along with this program; if not, write the Free Software 
# Foundation, Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
# 
# Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pkwy, 
# Mountain View, CA  94043, or:
# 
# http://www.sgi.com 
# 
# For further information regarding this notice, see: 
# 
# http://oss.sgi.com/projects/GenInfo/NoticeExplan
#

EXTRA_CFLAGS    := -DLITTLE_ENDIAN

.S.s:
	$(CPP) $(AFLAGS) $(AFLAGS_KERNEL) -o $*.s $<
.S.o:
	$(CC) $(AFLAGS) $(AFLAGS_KERNEL) -c -o $*.o $<

O_TARGET        = sn.o

export-objs = sn_ksyms.o

subdir-y			= sn2

obj-y				= bte.o idle.o irq.o mca.o probe.o setup.o \
				  sn_ivt.o sn2/sn2.o sv.o

obj-$(CONFIG_IA64_GENERIC)      += machvec.o
obj-$(CONFIG_MODULES)           += sn_ksyms.o

include $(TOPDIR)/Rules.make

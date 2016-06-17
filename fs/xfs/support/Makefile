#
# Copyright (c) 2000-2002 Silicon Graphics, Inc.  All Rights Reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of version 2 of the GNU General Public License as
# published by the Free Software Foundation.
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
# You should have received a copy of the GNU General Public License along
# with this program; if not, write the Free Software Foundation, Inc., 59
# Temple Place - Suite 330, Boston MA 02111-1307, USA.
#
# Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
# Mountain View, CA  94043, or:
#
# http://www.sgi.com
#
# For further information regarding this notice, see:
#
# http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
#

EXTRA_CFLAGS += -I.. -I $(TOPDIR)/fs/xfs/linux

ifeq ($(CONFIG_XFS_DEBUG),y)
	EXTRA_CFLAGS += -DDEBUG
endif

O_TARGET			:= support_xfs.o
ifneq ($(MAKECMDGOALS),modules_install)
  obj-m				:= $(O_TARGET)
endif

obj-y				:= debug.o \
				   move.o \
				   qsort.o \
				   uuid.o

obj-$(CONFIG_XFS_TRACE)		+= ktrace.o

include $(TOPDIR)/Rules.make

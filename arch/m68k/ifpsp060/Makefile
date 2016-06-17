# Makefile for 680x0 Linux 68060 integer/floating point support package
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "README.legal" in the main directory of this archive
# for more details.

.S.o:
	$(CC) $(AFLAGS) -traditional -c -o $*.o $<

OS_TARGET := ifpsp.o

OS_OBJS	  := fskeleton.o iskeleton.o os.o

all:	$(OS_TARGET)

include $(TOPDIR)/Rules.make

$(OS_OBJS):

$(OS_TARGET): $(OS_OBJS)
	$(LD) -x -r -o $(OS_TARGET) $(OS_OBJS)

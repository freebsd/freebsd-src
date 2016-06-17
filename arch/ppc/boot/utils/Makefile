#
# arch/ppc/boot/utils/Makefile
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.

HOSTCFLAGS += -I$(TOPDIR)/arch/$(ARCH)/boot/include

all: dummy

# Simple programs with 1 file and no extra CFLAGS
UTILS = addnote hack-coff mkbugboot mkprep mknote mktree

$(UTILS):
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $@.c

clean:
	rm -f $(UTILS)

include $(TOPDIR)/Rules.make

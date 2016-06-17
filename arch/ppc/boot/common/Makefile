#
# arch/ppc/boot/common/Makefile
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Tom Rini	January 2001
#

USE_STANDARD_AS_RULE	:= true

coffcrt0.o:
	$(CC) $(AFLAGS) -DXCOFF -traditional -c -o coffcrt0.o crt0.S

include $(TOPDIR)/Rules.make

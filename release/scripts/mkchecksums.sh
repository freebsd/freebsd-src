#!/bin/sh
#
# mkchecksums.sh - generate interactive checksum-checking script.
# Author: Jordan Hubbard
#
# This script generates a cksum.sh script from a set of tarballs
# and should not be run by anyone but the release coordinator (there
# wouldn't be much point).
#
# $Id: mkchecksums.sh,v 1.1 1995/01/14 07:41:50 jkh Exp $
#

# Remove any previous attempts.
rm -rf CKSUMS do_cksum.sh

# First generate the CKSUMS file for the benefit of those who wish to
# use it in some other way.  If we find out that folks aren't even using
# it, we should consider eliminating it at some point.  The interactive
# stuff makes it somewhat superfluous.
cksum * > CKSUMS

# Now generate a script for actually verifying the checksums.
awk 'BEGIN {print "rval=0"} { printf("if [ -f %s ]; then if [ \"\`cksum %s%s%s\`\" != \"%s %s %s\" ]; then dialog --title \"Checksum Error\" --msgbox \"Checksum error detected on %s!\" -1 -1; rval=1; fi; fi\n", $3, "\047", $3, "\047", $1, $2, $3, $3);} END {print "exit $rval"}' < CKSUMS > do_cksum.sh
chmod +x do_cksum.sh

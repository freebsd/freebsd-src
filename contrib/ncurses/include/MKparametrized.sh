#!/bin/sh
# $Id: MKparametrized.sh,v 1.5 2000/10/01 00:57:24 tom Exp $
#
# MKparametrized.sh -- generate indirection vectors for various sort methods
#
# The output of this script is C source for an array specifying whether
# termcap strings should undergo parameter and padding translation.
#
CAPS="${1-Caps}"
cat <<EOF
/*
 * parametrized.h --- is a termcap capability parametrized?
 *
 * Note: this file is generated using MKparametrized.sh, do not edit by hand.
 * A value of -1 in the table means suppress both pad and % translations.
 * A value of 0 in the table means do pad but not % translations.
 * A value of 1 in the table means do both pad and % translations.
 */

static short const parametrized[] = {
EOF

# We detect whether % translations should be done by looking for #[0-9] in the
# description field.  We presently suppress padding translation only for the
# XENIX acs_* capabilities.  Maybe someday we'll dedicate a flag field for
# this, that would be cleaner....

${AWK-awk} <$CAPS '
$3 != "str"	{next;}
$1 ~ /^acs_/	{print "-1,\t/* ", $2, " */"; count++; next;}
$0 ~ /#[0-9]/	{print "1,\t/* ", $2, " */"; count++; next;}
		{print "0,\t/* ", $2, " */"; count++;}
END		{printf("} /* %d entries */;\n\n", count);}
'


#!/bin/sh

# Simplfied version of Linux scripts/basic/fixdep. We don't need
# CONFIG tracking etc for this usecase.


# Fixdep's interface is described:

# It is invoked as
#
#   fixdep <depfile> <target> <cmdline>
#
# and will read the dependency file <depfile>
#
# The transformed dependency snipped is written to stdout.
#
# It first generates a line
#
#   cmd_<target> = <cmdline>
#
# and then basically copies the .<target>.d file to stdout, in the
# process filtering out the dependency on autoconf.h and adding
# dependencies on include/config/my/option.h for every
# CONFIG_MY_OPTION encountered in any of the prequisites.

echo cmd_$2 = $3
cat $1

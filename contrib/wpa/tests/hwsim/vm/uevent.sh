#!/bin/sh

EPATH=$(sed 's/.*EPATH=\([^ ]*\) .*/\1/' /proc/cmdline)
PATH=/tmp/bin:$EPATH:$PATH

# assume this was a call for CRDA,
# if not then it won't find a COUNTRY
# environment variable and exit
exec crda

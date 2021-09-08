#!/bin/sh

. .github/configs $1

set -x
./configure ${CONFIGFLAGS}

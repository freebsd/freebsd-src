#!/bin/sh
#
# $FreeBSD: src/usr.bin/less/zless.sh,v 1.1.28.1 2010/12/21 17:10:29 kensmith Exp $
#

export LESSOPEN="|/usr/bin/lesspipe.sh %s"
exec /usr/bin/less "$@"

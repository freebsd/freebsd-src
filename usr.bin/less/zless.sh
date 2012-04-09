#!/bin/sh
#
# $FreeBSD: src/usr.bin/less/zless.sh,v 1.1.24.1.8.1 2012/03/03 06:15:13 kensmith Exp $
#

export LESSOPEN="|/usr/bin/lesspipe.sh %s"
exec /usr/bin/less "$@"

#!/bin/sh
#
# $FreeBSD: src/usr.bin/less/zless.sh,v 1.1.24.1.2.1 2009/10/25 01:10:29 kensmith Exp $
#

export LESSOPEN="|/usr/bin/lesspipe.sh %s"
exec /usr/bin/less "$@"

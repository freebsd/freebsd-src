#!/bin/sh
#
# $FreeBSD: src/usr.bin/less/zless.sh,v 1.1.26.1 2010/02/10 00:26:20 kensmith Exp $
#

export LESSOPEN="|/usr/bin/lesspipe.sh %s"
exec /usr/bin/less "$@"

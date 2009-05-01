#!/bin/sh
#
# $FreeBSD: src/usr.bin/less/zless.sh,v 1.1.22.1 2009/04/15 03:14:26 kensmith Exp $
#

export LESSOPEN="|/usr/bin/lesspipe.sh %s"
exec /usr/bin/less "$@"

#!/bin/sh
#
# $FreeBSD: src/usr.bin/less/zless.sh,v 1.1.24.1.4.1 2010/06/14 02:09:06 kensmith Exp $
#

export LESSOPEN="|/usr/bin/lesspipe.sh %s"
exec /usr/bin/less "$@"

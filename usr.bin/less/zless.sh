#!/bin/sh
#
# $FreeBSD: src/usr.bin/less/zless.sh,v 1.1.18.1 2008/10/02 02:57:24 kensmith Exp $
#

export LESSOPEN="|/usr/bin/lesspipe.sh %s"
exec /usr/bin/less "$@"

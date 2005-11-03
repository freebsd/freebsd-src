#!/bin/sh
#
# $FreeBSD: src/usr.bin/less/zless.sh,v 1.1 2005/05/17 11:14:11 des Exp $
#

export LESSOPEN="|/usr/bin/lesspipe.sh %s"
exec /usr/bin/less "$@"

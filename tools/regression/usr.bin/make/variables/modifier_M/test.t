#!/bin/sh

# $FreeBSD: src/tools/regression/usr.bin/make/variables/modifier_M/test.t,v 1.1.16.1 2008/10/02 02:57:24 kensmith Exp $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Variable expansion with M modifier"

eval_cmd $*

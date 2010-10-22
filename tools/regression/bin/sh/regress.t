#!/bin/sh
# $FreeBSD$

export SH="${SH:-sh}"

cd `dirname $0`

${SH} regress.sh

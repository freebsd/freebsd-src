#!/bin/sh
# $FreeBSD: src/tools/regression/atm/proto_sscop/RunTest.sh,v 1.1.30.1.4.1 2010/06/14 02:09:06 kensmith Exp $

. ../Funcs.sh

parse_options $*

DATA=$LOCALBASE/share/atmsupport/testsuite_sscop

$LOCALBASE/bin/ats_sscop $options $DATA/Funcs $DATA/S*

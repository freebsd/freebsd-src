#!/bin/sh
# $FreeBSD: src/tools/regression/atm/proto_sscop/RunTest.sh,v 1.1.34.1 2010/12/21 17:10:29 kensmith Exp $

. ../Funcs.sh

parse_options $*

DATA=$LOCALBASE/share/atmsupport/testsuite_sscop

$LOCALBASE/bin/ats_sscop $options $DATA/Funcs $DATA/S*

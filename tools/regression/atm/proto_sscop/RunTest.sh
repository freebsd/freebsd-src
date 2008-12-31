#!/bin/sh
# $FreeBSD: src/tools/regression/atm/proto_sscop/RunTest.sh,v 1.1.26.1 2008/11/25 02:59:29 kensmith Exp $

. ../Funcs.sh

parse_options $*

DATA=$LOCALBASE/share/atmsupport/testsuite_sscop

$LOCALBASE/bin/ats_sscop $options $DATA/Funcs $DATA/S*

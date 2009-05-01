#!/bin/sh
# $FreeBSD: src/tools/regression/atm/proto_sscop/RunTest.sh,v 1.1.28.1 2009/04/15 03:14:26 kensmith Exp $

. ../Funcs.sh

parse_options $*

DATA=$LOCALBASE/share/atmsupport/testsuite_sscop

$LOCALBASE/bin/ats_sscop $options $DATA/Funcs $DATA/S*

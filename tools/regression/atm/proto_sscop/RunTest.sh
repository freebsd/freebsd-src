#!/bin/sh
# $FreeBSD: src/tools/regression/atm/proto_sscop/RunTest.sh,v 1.1.24.1 2008/10/02 02:57:24 kensmith Exp $

. ../Funcs.sh

parse_options $*

DATA=$LOCALBASE/share/atmsupport/testsuite_sscop

$LOCALBASE/bin/ats_sscop $options $DATA/Funcs $DATA/S*

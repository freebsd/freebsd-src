# $FreeBSD: src/tools/regression/usr.bin/jot/regress.sh,v 1.4.12.1 2008/10/02 02:57:24 kensmith Exp $

echo 1..1

REGRESSION_START($1)

REGRESSION_TEST_ONE(`jot -w "%X" -s , 100 1 200')

REGRESSION_END()

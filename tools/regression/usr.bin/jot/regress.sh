# $FreeBSD: src/tools/regression/usr.bin/jot/regress.sh,v 1.4 2004/11/11 19:47:54 nik Exp $

echo 1..1

REGRESSION_START($1)

REGRESSION_TEST_ONE(`jot -w "%X" -s , 100 1 200')

REGRESSION_END()

# $FreeBSD: src/tools/regression/usr.bin/jot/regress.sh,v 1.3 2002/06/24 14:22:40 jmallett Exp $

REGRESSION_START($1)

REGRESSION_TEST_ONE(`jot -w "%X" -s , 100 1 200')

REGRESSION_END()

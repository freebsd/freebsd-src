# $FreeBSD$

REGRESSION_START($1)

REGRESSION_TEST(`G', `sed G < regress.in')
REGRESSION_TEST(`P', `sed P < regress.in')
REGRESSION_TEST(`psl', `sed \$!g\;P\;D < regress.in')
REGRESSION_TEST(`y', `sed y/\(\)/{}/ < regress.in')

REGRESSION_END()

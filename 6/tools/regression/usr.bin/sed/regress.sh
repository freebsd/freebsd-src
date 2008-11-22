# $FreeBSD$

REGRESSION_START($1)

echo '1..14'

REGRESSION_TEST(`G', `sed G < regress.in')
REGRESSION_TEST(`P', `sed P < regress.in')
REGRESSION_TEST(`psl', `sed \$!g\;P\;D < regress.in')
REGRESSION_TEST(`bcb', `sed s/X/$(jot -n -bx -s "" 2043)\\\\zz/ < regress.in')
REGRESSION_TEST(`y', `echo -n foo | sed y/o/O/')
REGRESSION_TEST(`sg', `echo foo | sed s/,*/,/g')
REGRESSION_TEST(`s3', `echo foo | sed s/,*/,/3')
REGRESSION_TEST(`s4', `echo foo | sed s/,*/,/4')
REGRESSION_TEST(`s5', `echo foo | sed s/,*/,/5')
REGRESSION_TEST(`c0', `sed ''`c\
foo
''`< regress.in')
REGRESSION_TEST(`c1', `sed ''`4,$c\
foo
''`< regress.in')
REGRESSION_TEST(`c2', `sed ''`3,9c\
foo
''`< regress.in')
REGRESSION_TEST(`c3', `sed ''`3,/no such string/c\
foo
''`< regress.in')
REGRESSION_TEST(`b2a', `sed ''`2,3b
1,2d''` < regress.in')

REGRESSION_END()

# $FreeBSD$

LC_ALL=C; export LC_ALL

echo 1..22

REGRESSION_START($1)

REGRESSION_TEST(`args', `m4 args.m4')
REGRESSION_TEST(`args2', `m4 args2.m4')
REGRESSION_TEST(`comments', `m4 comments.m4')
REGRESSION_TEST(`esyscmd', `m4 esyscmd.m4')
REGRESSION_TEST(`eval', `m4 eval.m4')
REGRESSION_TEST(`ff_after_dnl', `uudecode -o /dev/stdout ff_after_dnl.m4.uu | m4')
REGRESSION_TEST(`gnueval', `m4 -g gnueval.m4')
REGRESSION_TEST(`gnuformat', `m4 -g gnuformat.m4')
REGRESSION_TEST(`gnupatterns', `m4 -g gnupatterns.m4')
REGRESSION_TEST(`gnupatterns2', `m4 -g gnupatterns2.m4')
REGRESSION_TEST(`gnuprefix', `m4 -P gnuprefix.m4 2>&1')
REGRESSION_TEST(`gnusofterror', `m4 -g gnusofterror.m4')
REGRESSION_TEST(`gnutranslit2', `m4 -g translit2.m4')
REGRESSION_TEST(`includes', `m4 -I. includes.m4')
REGRESSION_TEST(`m4wrap3', `m4 m4wrap3.m4')
REGRESSION_TEST(`patterns', `m4 patterns.m4')
REGRESSION_TEST(`quotes', `m4 quotes.m4 2>&1')
REGRESSION_TEST(`strangequotes', `uudecode -o /dev/stdout strangequotes.m4.uu | m4')
REGRESSION_TEST(`redef', `m4 redef.m4')
REGRESSION_TEST(`translit', `m4 translit.m4')
REGRESSION_TEST(`translit2', `m4 translit2.m4')

REGRESSION_END()

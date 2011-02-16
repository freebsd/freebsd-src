/*
 * config/i386/freebsd64.h neglects to override the default bogus mcount
 * function name.  In order to avoid touching vendor source while gcc3.4
 * is in progress, try a minimal workaround.
 *
 * $FreeBSD: src/gnu/usr.bin/cc/cc_tools/freebsd64-fix.h,v 1.1.30.1 2010/12/21 17:10:29 kensmith Exp $
 */
#undef MCOUNT_NAME
#define MCOUNT_NAME ".mcount"

#!./perl

# $RCSfile: repeat.t,v $$Revision: 4.1 $$Date: 92/08/07 18:28:21 $

print "1..20\n";

# compile time

if ('-' x 5 eq '-----') {print "ok 1\n";} else {print "not ok 1\n";}
if ('-' x 1 eq '-') {print "ok 2\n";} else {print "not ok 2\n";}
if ('-' x 0 eq '') {print "ok 3\n";} else {print "not ok 3\n";}

if ('ab' x 3 eq 'ababab') {print "ok 4\n";} else {print "not ok 4\n";}

# run time

$a = '-';
if ($a x 5 eq '-----') {print "ok 5\n";} else {print "not ok 5\n";}
if ($a x 1 eq '-') {print "ok 6\n";} else {print "not ok 6\n";}
if ($a x 0 eq '') {print "ok 7\n";} else {print "not ok 7\n";}

$a = 'ab';
if ($a x 3 eq 'ababab') {print "ok 8\n";} else {print "not ok 8\n";}

$a = 'xyz';
$a x= 2;
if ($a eq 'xyzxyz') {print "ok 9\n";} else {print "not ok 9\n";}
$a x= 1;
if ($a eq 'xyzxyz') {print "ok 10\n";} else {print "not ok 10\n";}
$a x= 0;
if ($a eq '') {print "ok 11\n";} else {print "not ok 11\n";}

@x = (1,2,3);

print join('', @x x 4) eq '3333' ? "ok 12\n" : "not ok 12\n";
print join('', (@x) x 4) eq '123123123123' ? "ok 13\n" : "not ok 13\n";
print join('', (@x,()) x 4) eq '123123123123' ? "ok 14\n" : "not ok 14\n";
print join('', (@x,1) x 4) eq '1231123112311231' ? "ok 15\n" : "not ok 15\n";
print join(':', () x 4) eq '' ? "ok 16\n" : "not ok 16\n";
print join(':', (9) x 4) eq '9:9:9:9' ? "ok 17\n" : "not ok 17\n";
print join(':', (9,9) x 4) eq '9:9:9:9:9:9:9:9' ? "ok 18\n" : "not ok 18\n";
print join('', (split(//,"123")) x 2) eq '123123' ? "ok 19\n" : "not ok 19\n";

#
# The test #20 is actually testing for Digital C compiler optimizer bug.
#
# Dec C versions 5.* and 6.0 (used in Digital UNIX and VMS) used
# to produce (as of December 1998) broken code for util.c:repeatcpy()
# (a utility function for the 'x' operator) in the case *all* these
# four conditions held:
#
# (1) len == 1
# (2) "from" had the 8th bit on in its single character
# (3) count > 7 (the 'x' count > 16)
# (4) the highest optimization level was used in compilation
#     (which is the default when compiling Perl)
#
# The bug looked like this (. being the eight-bit character and ? being \xff):
#
# 16 ................
# 17 .........???????.
# 18 .........???????..
# 19 .........???????...
# 20 .........???????....
# 21 .........???????.....
# 22 .........???????......
# 23 .........???????.......
# 24 .........???????.???????
# 25 .........???????.???????.
#
# The bug could be (obscurely) avoided by changing "from" to
# be an unsigned char pointer.
#
# The bug was triggered in the "if (len == 1)" branch.  The fix
# was to introduce a new temporary variable.  In diff -u format:
#
#     register char *frombase = from;
# 
#     if (len == 1) {
#-       todo = *from;
#+       register char c = *from;
#        while (count-- > 0)
#-           *to++ = todo;
#+           *to++ = c;
#        return;
#     }
#
# This obscure bug was not found by the then test suite but instead
# by Mark.Martinec@nsc.ijs.si while trying to install Digest-MD5-2.00.
#
# jhi@iki.fi
#
print "\xdd" x 24 eq "\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd" ? "ok 20\n" : "not ok 20\n";

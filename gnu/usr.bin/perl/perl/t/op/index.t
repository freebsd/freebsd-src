#!./perl

# $Header: /pub/FreeBSD/FreeBSD-CVS/src/gnu/usr.bin/perl/perl/t/op/index.t,v 1.1.1.1 1994/09/10 06:27:42 gclarkii Exp $

print "1..20\n";


$foo = 'Now is the time for all good men to come to the aid of their country.';

$first = substr($foo,0,index($foo,'the'));
print ($first eq "Now is " ? "ok 1\n" : "not ok 1\n");

$last = substr($foo,rindex($foo,'the'),100);
print ($last eq "their country." ? "ok 2\n" : "not ok 2\n");

$last = substr($foo,index($foo,'Now'),2);
print ($last eq "No" ? "ok 3\n" : "not ok 3\n");

$last = substr($foo,rindex($foo,'Now'),2);
print ($last eq "No" ? "ok 4\n" : "not ok 4\n");

$last = substr($foo,index($foo,'.'),100);
print ($last eq "." ? "ok 5\n" : "not ok 5\n");

$last = substr($foo,rindex($foo,'.'),100);
print ($last eq "." ? "ok 6\n" : "not ok 6\n");

print index("ababa","a",-1) == 0 ? "ok 7\n" : "not ok 7\n";
print index("ababa","a",0) == 0 ? "ok 8\n" : "not ok 8\n";
print index("ababa","a",1) == 2 ? "ok 9\n" : "not ok 9\n";
print index("ababa","a",2) == 2 ? "ok 10\n" : "not ok 10\n";
print index("ababa","a",3) == 4 ? "ok 11\n" : "not ok 11\n";
print index("ababa","a",4) == 4 ? "ok 12\n" : "not ok 12\n";
print index("ababa","a",5) == -1 ? "ok 13\n" : "not ok 13\n";

print rindex("ababa","a",-1) == -1 ? "ok 14\n" : "not ok 14\n";
print rindex("ababa","a",0) == 0 ? "ok 15\n" : "not ok 15\n";
print rindex("ababa","a",1) == 0 ? "ok 16\n" : "not ok 16\n";
print rindex("ababa","a",2) == 2 ? "ok 17\n" : "not ok 17\n";
print rindex("ababa","a",3) == 2 ? "ok 18\n" : "not ok 18\n";
print rindex("ababa","a",4) == 4 ? "ok 19\n" : "not ok 19\n";
print rindex("ababa","a",5) == 4 ? "ok 20\n" : "not ok 20\n";

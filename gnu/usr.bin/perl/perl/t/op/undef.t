#!./perl

# $Header: /pub/FreeBSD/FreeBSD-CVS/src/gnu/usr.bin/perl/perl/t/op/undef.t,v 1.1.1.1 1994/09/10 06:27:46 gclarkii Exp $

print "1..21\n";

print defined($a) ? "not ok 1\n" : "ok 1\n";

$a = 1+1;
print defined($a) ? "ok 2\n" : "not ok 2\n";

undef $a;
print defined($a) ? "not ok 3\n" : "ok 3\n";

$a = "hi";
print defined($a) ? "ok 4\n" : "not ok 4\n";

$a = $b;
print defined($a) ? "not ok 5\n" : "ok 5\n";

@ary = ("1arg");
$a = pop(@ary);
print defined($a) ? "ok 6\n" : "not ok 6\n";
$a = pop(@ary);
print defined($a) ? "not ok 7\n" : "ok 7\n";

@ary = ("1arg");
$a = shift(@ary);
print defined($a) ? "ok 8\n" : "not ok 8\n";
$a = shift(@ary);
print defined($a) ? "not ok 9\n" : "ok 9\n";

$ary{'foo'} = 'hi';
print defined($ary{'foo'}) ? "ok 10\n" : "not ok 10\n";
print defined($ary{'bar'}) ? "not ok 11\n" : "ok 11\n";
undef $ary{'foo'};
print defined($ary{'foo'}) ? "not ok 12\n" : "ok 12\n";

print defined(@ary) ? "ok 13\n" : "not ok 13\n";
print defined(%ary) ? "ok 14\n" : "not ok 14\n";
undef @ary;
print defined(@ary) ? "not ok 15\n" : "ok 15\n";
undef %ary;
print defined(%ary) ? "not ok 16\n" : "ok 16\n";
@ary = (1);
print defined @ary ? "ok 17\n" : "not ok 17\n";
%ary = (1,1);
print defined %ary ? "ok 18\n" : "not ok 18\n";

sub foo { print "ok 19\n"; }

&foo || print "not ok 19\n";

print defined &foo ? "ok 20\n" : "not ok 20\n";
undef &foo;
print defined(&foo) ? "not ok 21\n" : "ok 21\n";

#!./perl

# $Header: /pub/FreeBSD/FreeBSD-CVS/src/gnu/usr.bin/perl/perl/t/op/glob.t,v 1.1.1.1 1994/09/10 06:27:42 gclarkii Exp $

print "1..4\n";

@ops = <op/*>;
$list = join(' ',@ops);

chop($otherway = `echo op/*`);

print $list eq $otherway ? "ok 1\n" : "not ok 1\n$list\n$otherway\n";

print $/ eq "\n" ? "ok 2\n" : "not ok 2\n";

while (<jskdfjskdfj* op/* jskdjfjkosvk*>) {
    $not = "not " unless $_ eq shift @ops;
    $not = "not at all " if $/ eq "\0";
}
print "${not}ok 3\n";

print $/ eq "\n" ? "ok 4\n" : "not ok 4\n";

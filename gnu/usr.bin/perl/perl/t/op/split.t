#!./perl

# $Header: /pub/FreeBSD/FreeBSD-CVS/src/gnu/usr.bin/perl/perl/t/op/split.t,v 1.1.1.1 1994/09/10 06:27:46 gclarkii Exp $

print "1..12\n";

$FS = ':';

$_ = 'a:b:c';

($a,$b,$c) = split($FS,$_);

if (join(';',$a,$b,$c) eq 'a;b;c') {print "ok 1\n";} else {print "not ok 1\n";}

@ary = split(/:b:/);
if (join("$_",@ary) eq 'aa:b:cc') {print "ok 2\n";} else {print "not ok 2\n";}

$_ = "abc\n";
@xyz = (@ary = split(//));
if (join(".",@ary) eq "a.b.c.\n") {print "ok 3\n";} else {print "not ok 3\n";}

$_ = "a:b:c::::";
@ary = split(/:/);
if (join(".",@ary) eq "a.b.c") {print "ok 4\n";} else {print "not ok 4\n";}

$_ = join(':',split(' ',"    a b\tc \t d "));
if ($_ eq 'a:b:c:d') {print "ok 5\n";} else {print "not ok 5 #$_#\n";}

$_ = join(':',split(/ */,"foo  bar bie\tdoll"));
if ($_ eq "f:o:o:b:a:r:b:i:e:\t:d:o:l:l")
	{print "ok 6\n";} else {print "not ok 6\n";}

$_ = join(':', 'foo', split(/ /,'a b  c'), 'bar');
if ($_ eq "foo:a:b::c:bar") {print "ok 7\n";} else {print "not ok 7 $_\n";}

# Can we say how many fields to split to?
$_ = join(':', split(' ','1 2 3 4 5 6', 3));
print $_ eq '1:2:3 4 5 6' ? "ok 8\n" : "not ok 8 $_\n";

# Can we do it as a variable?
$x = 4;
$_ = join(':', split(' ','1 2 3 4 5 6', $x));
print $_ eq '1:2:3:4 5 6' ? "ok 9\n" : "not ok 9 $_\n";

# Does the 999 suppress null field chopping?
$_ = join(':', split(/:/,'1:2:3:4:5:6:::', 999));
print $_ eq '1:2:3:4:5:6:::' ? "ok 10\n" : "not ok 10 $_\n";

# Does assignment to a list imply split to one more field than that?
$foo = `./perl -D1024 -e '(\$a,\$b) = split;' 2>&1`;
print $foo =~ /DEBUGGING/ || $foo =~ /num\(3\)/ ? "ok 11\n" : "not ok 11\n";

# Can we say how many fields to split to when assigning to a list?
($a,$b) = split(' ','1 2 3 4 5 6', 2);
$_ = join(':',$a,$b);
print $_ eq '1:2 3 4 5 6' ? "ok 12\n" : "not ok 12 $_\n";


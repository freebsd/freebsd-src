#!./perl

#
# test recursive functions.
#

print "1..23\n";

sub gcd ($$) {
    return gcd($_[0] - $_[1], $_[1]) if ($_[0] > $_[1]);
    return gcd($_[0], $_[1] - $_[0]) if ($_[0] < $_[1]);
    $_[0];
}

sub factorial ($) {
    $_[0] < 2 ? 1 : $_[0] * factorial($_[0] - 1);
}

sub fibonacci ($) {
    $_[0] < 2 ? 1 : fibonacci($_[0] - 2) + fibonacci($_[0] - 1);
}

# Highly recursive, highly aggressive.
# Kids, don't try this at home.
#
# For example ackermann(4,1) will take quite a long time.
# It will simply eat away your memory. Trust me.

sub ackermann ($$) {
    return $_[1] + 1               if ($_[0] == 0);
    return ackermann($_[0] - 1, 1) if ($_[1] == 0);
    ackermann($_[0] - 1, ackermann($_[0], $_[1] - 1));
}

# Highly recursive, highly boring.

sub takeuchi ($$$) {
    $_[1] < $_[0] ?
	takeuchi(takeuchi($_[0] - 1, $_[1], $_[2]),
		 takeuchi($_[1] - 1, $_[2], $_[0]),
		 takeuchi($_[2] - 1, $_[0], $_[1]))
	    : $_[2];
}

print 'not ' unless (($d = gcd(1147, 1271)) == 31);
print "ok 1\n";
print "# gcd(1147, 1271) = $d\n";

print 'not ' unless (($d = gcd(1908, 2016)) == 36);
print "ok 2\n";
print "# gcd(1908, 2016) = $d\n";

print 'not ' unless (($f = factorial(10)) == 3628800);
print "ok 3\n";
print "# factorial(10) = $f\n";

print 'not ' unless (($f = factorial(factorial(3))) == 720);
print "ok 4\n";
print "# factorial(factorial(3)) = $f\n";

print 'not ' unless (($f = fibonacci(10)) == 89);
print "ok 5\n";
print "# fibonacci(10) = $f\n";

print 'not ' unless (($f = fibonacci(fibonacci(7))) == 17711);
print "ok 6\n";
print "# fibonacci(fibonacci(7)) = $f\n";

$i = 7;

@ack = qw(1 2 3 4 2 3 4 5 3 5 7 9 5 13 29 61);

for $x (0..3) { 
    for $y (0..3) {
	$a = ackermann($x, $y);
	print 'not ' unless ($a == shift(@ack));
	print "ok ", $i++, "\n";
	print "# ackermann($x, $y) = $a\n";
    }
}

($x, $y, $z) = (18, 12, 6);

print 'not ' unless (($t = takeuchi($x, $y, $z)) == $z + 1);
print "ok ", $i++, "\n";
print "# takeuchi($x, $y, $z) = $t\n";

#./perl

BEGIN {
	eval { my $q = pack "q", 0 };
	if ($@) {
		print "1..0\n# Skip: no 64-bit types\n";
		exit(0);
	}
	chdir 't' if -d 't';
	@INC = '../lib';
}

# This could use many more tests.

# so that using > 0xfffffff constants and
# 32+ bit integers don't cause noise
no warnings qw(overflow portable);

print "1..55\n";

my $q = 12345678901;
my $r = 23456789012;
my $f = 0xffffffff;
my $x;
my $y;

$x = unpack "q", pack "q", $q;
print "not " unless $x == $q && $x > $f;
print "ok 1\n";


$x = sprintf("%lld", 12345678901);
print "not " unless $x eq $q && $x > $f;
print "ok 2\n";


$x = sprintf("%lld", $q);
print "not " unless $x == $q && $x eq $q && $x > $f;
print "ok 3\n";

$x = sprintf("%Ld", $q);
print "not " unless $x == $q && $x eq $q && $x > $f;
print "ok 4\n";

$x = sprintf("%qd", $q);
print "not " unless $x == $q && $x eq $q && $x > $f;
print "ok 5\n";


$x = sprintf("%llx", $q);
print "not " unless hex($x) == 0x2dfdc1c35 && hex($x) > $f;
print "ok 6\n";

$x = sprintf("%Lx", $q);
print "not " unless hex($x) == 0x2dfdc1c35 && hex($x) > $f;
print "ok 7\n";

$x = sprintf("%qx", $q);
print "not " unless hex($x) == 0x2dfdc1c35 && hex($x) > $f;
print "ok 8\n";


$x = sprintf("%llo", $q);
print "not " unless oct("0$x") == 0133767016065 && oct($x) > $f;
print "ok 9\n";

$x = sprintf("%Lo", $q);
print "not " unless oct("0$x") == 0133767016065 && oct($x) > $f;
print "ok 10\n";

$x = sprintf("%qo", $q);
print "not " unless oct("0$x") == 0133767016065 && oct($x) > $f;
print "ok 11\n";


$x = sprintf("%llb", $q);
print "not " unless oct("0b$x") == 0b1011011111110111000001110000110101 &&
                    oct("0b$x") > $f;
print "ok 12\n";

$x = sprintf("%Lb", $q);
print "not " unless oct("0b$x") == 0b1011011111110111000001110000110101 &&
                                   oct("0b$x") > $f;
print "ok 13\n";

$x = sprintf("%qb", $q);
print "not " unless oct("0b$x") == 0b1011011111110111000001110000110101 &&
                    oct("0b$x") > $f;
print "ok 14\n";


$x = sprintf("%llu", $q);
print "not " unless $x eq $q && $x > $f;
print "ok 15\n";

$x = sprintf("%Lu", $q);
print "not " unless $x == $q && $x eq $q && $x > $f;
print "ok 16\n";

$x = sprintf("%qu", $q);
print "not " unless $x == $q && $x eq $q && $x > $f;
print "ok 17\n";


$x = sprintf("%D", $q);
print "not " unless $x == $q && $x eq $q && $x > $f;
print "ok 18\n";

$x = sprintf("%U", $q);
print "not " unless $x == $q && $x eq $q && $x > $f;
print "ok 19\n";

$x = sprintf("%O", $q);
print "not " unless oct($x) == $q && oct($x) > $f;
print "ok 20\n";


$x = $q + $r;
print "not " unless $x == 35802467913 && $x > $f;
print "ok 21\n";

$x = $q - $r;
print "not " unless $x == -11111110111 && -$x > $f;
print "ok 22\n";

if ($^O ne 'unicos') {
    $x = $q * 1234567;
    print "not " unless $x == 15241567763770867 && $x > $f;
    print "ok 23\n";

    $x /= 1234567;
    print "not " unless $x == $q && $x > $f;
    print "ok 24\n";

    $x = 98765432109 % 12345678901;
    print "not " unless $x == 901;
    print "ok 25\n";
    
    # The following 12 tests adapted from op/inc.

    $a = 9223372036854775807;
    $c = $a++;
    print "not " unless $a == 9223372036854775808;
    print "ok 26\n";

    $a = 9223372036854775807;
    $c = ++$a;
    print "not "
	unless $a == 9223372036854775808 && $c == $a;
    print "ok 27\n";

    $a = 9223372036854775807;
    $c = $a + 1;
    print "not "
	unless $a == 9223372036854775807 && $c == 9223372036854775808;
    print "ok 28\n";

    $a = -9223372036854775808;
    $c = $a--;
    print "not "
	unless $a == -9223372036854775809 && $c == -9223372036854775808;
    print "ok 29\n";

    $a = -9223372036854775808;
    $c = --$a;
    print "not "
	unless $a == -9223372036854775809 && $c == $a;
    print "ok 30\n";

    $a = -9223372036854775808;
    $c = $a - 1;
    print "not "
	unless $a == -9223372036854775808 && $c == -9223372036854775809;
    print "ok 31\n";
    
    $a = 9223372036854775808;
    $a = -$a;
    $c = $a--;
    print "not "
	unless $a == -9223372036854775809 && $c == -9223372036854775808;
    print "ok 32\n";
    
    $a = 9223372036854775808;
    $a = -$a;
    $c = --$a;
    print "not "
	unless $a == -9223372036854775809 && $c == $a;
    print "ok 33\n";
    
    $a = 9223372036854775808;
    $a = -$a;
    $c = $a - 1;
    print "not "
	unless $a == -9223372036854775808 && $c == -9223372036854775809;
    print "ok 34\n";

    $a = 9223372036854775808;
    $b = -$a;
    $c = $b--;
    print "not "
	unless $b == -$a-1 && $c == -$a;
    print "ok 35\n";

    $a = 9223372036854775808;
    $b = -$a;
    $c = --$b;
    print "not "
	unless $b == -$a-1 && $c == $b;
    print "ok 36\n";

    $a = 9223372036854775808;
    $b = -$a;
    $b = $b - 1;
    print "not "
	unless $b == -(++$a);
    print "ok 37\n";

} else {
    # Unicos has imprecise doubles (14 decimal digits or so),
    # especially if operating near the UV/IV limits the low-order bits
    # become mangled even by simple arithmetic operations.
    for (23..37) {
	print "ok $_ # skipped: too imprecise numbers\n";
    }
}


$x = '';
print "not " unless (vec($x, 1, 64) = $q) == $q;
print "ok 38\n";

print "not " unless vec($x, 1, 64) == $q && vec($x, 1, 64) > $f;
print "ok 39\n";

print "not " unless vec($x, 0, 64) == 0 && vec($x, 2, 64) == 0;
print "ok 40\n";


print "not " unless ~0 == 0xffffffffffffffff;
print "ok 41\n";

print "not " unless (0xffffffff<<32) == 0xffffffff00000000;
print "ok 42\n";

print "not " unless ((0xffffffff)<<32)>>32 == 0xffffffff;
print "ok 43\n";

print "not " unless 1<<63 == 0x8000000000000000;
print "ok 44\n";

print "not " unless (sprintf "%#Vx", 1<<63) eq '0x8000000000000000';
print "ok 45\n";

print "not " unless (0x8000000000000000 | 1) == 0x8000000000000001;
print "ok 46\n";

print "not "
    unless (0xf000000000000000 & 0x8000000000000000) == 0x8000000000000000;
print "ok 47\n";

print "not "
    unless (0xf000000000000000 ^ 0xfffffffffffffff0) == 0x0ffffffffffffff0;
print "ok 48\n";


print "not "
    unless (sprintf "%b", ~0)   eq
           '1111111111111111111111111111111111111111111111111111111111111111';
print "ok 49\n";

print "not "
    unless (sprintf "%64b", ~0) eq
           '1111111111111111111111111111111111111111111111111111111111111111';
print "ok 50\n";

print "not " unless (sprintf "%d", ~0>>1) eq '9223372036854775807';
print "ok 51\n";

print "not " unless (sprintf "%u", ~0)    eq '18446744073709551615';
print "ok 52\n";

# If the 53..55 fail you have problems in the parser's string->int conversion,
# see toke.c:scan_num().

$q = -9223372036854775808;
print "# $q ne\n# -9223372036854775808\nnot " unless "$q" eq "-9223372036854775808";
print "ok 53\n";

$q =  9223372036854775807;
print "# $q ne\n# 9223372036854775807\nnot " unless "$q" eq "9223372036854775807";
print "ok 54\n";

$q = 18446744073709551615;
print "# $q ne\n# 18446744073709551615\nnot " unless "$q" eq "18446744073709551615";
print "ok 55\n";

# eof

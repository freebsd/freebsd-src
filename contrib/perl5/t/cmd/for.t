#!./perl

print "1..10\n";

for ($i = 0; $i <= 10; $i++) {
    $x[$i] = $i;
}
$y = $x[10];
print "#1	:$y: eq :10:\n";
$y = join(' ', @x);
print "#1	:$y: eq :0 1 2 3 4 5 6 7 8 9 10:\n";
if (join(' ', @x) eq '0 1 2 3 4 5 6 7 8 9 10') {
	print "ok 1\n";
} else {
	print "not ok 1\n";
}

$i = $c = 0;
for (;;) {
	$c++;
	last if $i++ > 10;
}
if ($c == 12) {print "ok 2\n";} else {print "not ok 2\n";}

$foo = 3210;
@ary = (1,2,3,4,5);
foreach $foo (@ary) {
	$foo *= 2;
}
if (join('',@ary) eq '246810') {print "ok 3\n";} else {print "not ok 3\n";}

for (@ary) {
    s/(.*)/ok $1\n/;
}

print $ary[1];

# test for internal scratch array generation
# this also tests that $foo was restored to 3210 after test 3
for (split(' ','a b c d e')) {
	$foo .= $_;
}
if ($foo eq '3210abcde') {print "ok 5\n";} else {print "not ok 5 $foo\n";}

foreach $foo (("ok 6\n","ok 7\n")) {
	print $foo;
}

sub foo {
    for $i (1..5) {
	return $i if $_[0] == $i;
    }
}

print foo(1) == 1 ? "ok" : "not ok", " 8\n";
print foo(2) == 2 ? "ok" : "not ok", " 9\n";
print foo(5) == 5 ? "ok" : "not ok", " 10\n";

#!./perl

# $RCSfile: sprintf.t,v $$Revision: 4.1 $$Date: 92/08/07 18:28:27 $

print "1..4\n";

$^W = 1;
$SIG{__WARN__} = sub {
    if ($_[0] =~ /^Invalid conversion/) {
	$w++;
    } else {
	warn @_;
    }
};

$w = 0;
$x = sprintf("%3s %-4s%%foo %.0d%5d %#x%c%3.1f","hi",123,0,456,0,ord('A'),3.0999);
if ($x eq ' hi 123 %foo   456 0A3.1' && $w == 0) {
    print "ok 1\n";
} else {
    print "not ok 1 '$x'\n";
}

for $i (2 .. 4) {
    $f = ('%6 .6s', '%6. 6s', '%6.6 s')[$i - 2];
    $w = 0;
    $x = sprintf($f, '');
    if ($x eq $f && $w == 1) {
	print "ok $i\n";
    } else {
	print "not ok $i '$x' '$f' '$w'\n";
    }
}

#!./perl

@FOO = ('s', 'N/A', 'a', 'NaN', -1, undef, 0, 1);

$expect = ($#FOO+2) * ($#FOO+1);
print "1..$expect\n";

my $ok = 0;
for my $i (0..$#FOO) {
    for my $j ($i..$#FOO) {
	$ok++;
	my $cmp = $FOO[$i] <=> $FOO[$j];
	if (!defined($cmp) ||
	    $cmp == -1 && $FOO[$i] < $FOO[$j] ||
	    $cmp == 0  && $FOO[$i] == $FOO[$j] ||
	    $cmp == 1  && $FOO[$i] > $FOO[$j])
	{
	    print "ok $ok\n";
	}
	else {
	    print "not ok $ok ($FOO[$i] <=> $FOO[$j]) gives: '$cmp'\n";
	}
	$ok++;
	$cmp = $FOO[$i] cmp $FOO[$j];
	if ($cmp == -1 && $FOO[$i] lt $FOO[$j] ||
	    $cmp == 0  && $FOO[$i] eq $FOO[$j] ||
	    $cmp == 1  && $FOO[$i] gt $FOO[$j])
	{
	    print "ok $ok\n";
	}
	else {
	    print "not ok $ok ($FOO[$i] cmp $FOO[$j]) gives '$cmp'\n";
	}
    }
}

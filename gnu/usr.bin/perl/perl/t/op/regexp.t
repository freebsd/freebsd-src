#!./perl

# $RCSfile: regexp.t,v $$Revision: 1.1.1.1 $$Date: 1993/08/23 21:30:03 $

open(TESTS,'op/re_tests') || open(TESTS,'t/op/re_tests')
    || die "Can't open re_tests";
while (<TESTS>) { }
$numtests = $.;
close(TESTS);

print "1..$numtests\n";
open(TESTS,'op/re_tests') || open(TESTS,'t/op/re_tests')
    || die "Can't open re_tests";
$| = 1;
while (<TESTS>) {
    ($pat, $subject, $result, $repl, $expect) = split(/[\t\n]/,$_);
    $input = join(':',$pat,$subject,$result,$repl,$expect);
    $pat = "'$pat'" unless $pat =~ /^'/;
    eval "\$match = (\$subject =~ m$pat); \$got = \"$repl\";";
    if ($result eq 'c') {
	if ($@ ne '') {print "ok $.\n";} else {print "not ok $.\n";}
    }
    elsif ($result eq 'n') {
	if (!$match) {print "ok $.\n";} else {print "not ok $. $input => $got\n";}
    }
    else {
	if ($match && $got eq $expect) {
	    print "ok $.\n";
	}
	else {
	    print "not ok $. $input => $got\n";
	}
    }
}
close(TESTS);

#!./perl

# The tests are in a separate file 't/op/re_tests'.
# Each line in that file is a separate test.
# There are five columns, separated by tabs.
#
# Column 1 contains the pattern, optionally enclosed in C<''>.
# Modifiers can be put after the closing C<'>.
#
# Column 2 contains the string to be matched.
#
# Column 3 contains the expected result:
# 	y	expect a match
# 	n	expect no match
# 	c	expect an error
#	B	test exposes a known bug in Perl, should be skipped
#	b	test exposes a known bug in Perl, should be skipped if noamp
#
# Columns 4 and 5 are used only if column 3 contains C<y> or C<c>.
#
# Column 4 contains a string, usually C<$&>.
#
# Column 5 contains the expected result of double-quote
# interpolating that string after the match, or start of error message.
#
# Column 6, if present, contains a reason why the test is skipped.
# This is printed with "skipped", for harness to pick up.
#
# \n in the tests are interpolated, as are variables of the form ${\w+}.
#
# If you want to add a regular expression test that can't be expressed
# in this format, don't add it here: put it in op/pat.t instead.

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

$iters = shift || 1;		# Poor man performance suite, 10000 is OK.

open(TESTS,'op/re_tests') || open(TESTS,'t/op/re_tests') ||
	die "Can't open re_tests";

while (<TESTS>) { }
$numtests = $.;
seek(TESTS,0,0);
$. = 0;

$bang = sprintf "\\%03o", ord "!"; # \41 would not be portable.
$ffff  = chr(0xff) x 2;
$nulnul = "\0" x 2;

$| = 1;
print "1..$numtests\n# $iters iterations\n";
TEST:
while (<TESTS>) {
    chomp;
    s/\\n/\n/g;
    ($pat, $subject, $result, $repl, $expect, $reason) = split(/\t/,$_,6);
    $input = join(':',$pat,$subject,$result,$repl,$expect);
    infty_subst(\$pat);
    infty_subst(\$expect);
    $pat = "'$pat'" unless $pat =~ /^[:']/;
    $pat =~ s/(\$\{\w+\})/$1/eeg;
    $pat =~ s/\\n/\n/g;
    $subject =~ s/(\$\{\w+\})/$1/eeg;
    $subject =~ s/\\n/\n/g;
    $expect =~ s/(\$\{\w+\})/$1/eeg;
    $expect =~ s/\\n/\n/g;
    $expect = $repl = '-' if $skip_amp and $input =~ /\$[&\`\']/;
    $skip = ($skip_amp ? ($result =~ s/B//i) : ($result =~ s/B//));
    # Certain tests don't work with utf8 (the re_test should be in UTF8)
    $skip = 1, $reason = 'utf8'
      if ($^H &= ~0x00000008) && $pat =~ /\[:\^(alnum|print|word|ascii|xdigit):\]/;
    $result =~ s/B//i unless $skip;
    for $study ('', 'study \$subject') {
 	$c = $iters;
 	eval "$study; \$match = (\$subject =~ m$pat) while \$c--; \$got = \"$repl\";";
	chomp( $err = $@ );
	if ($result eq 'c') {
	    if ($err !~ m!^\Q$expect!) { print "not ok $. (compile) $input => `$err'\n"; next TEST }
	    last;  # no need to study a syntax error
	}
	elsif ( $skip ) {
	    print "ok $. # skipped", length($reason) ? " $reason" : '', "\n";
	    next TEST;
	}
	elsif ($@) {
	    print "not ok $. $input => error `$err'\n"; next TEST;
	}
	elsif ($result eq 'n') {
	    if ($match) { print "not ok $. ($study) $input => false positive\n"; next TEST }
	}
	else {
	    if (!$match || $got ne $expect) {
 		print "not ok $. ($study) $input => `$got', match=$match\n";
		next TEST;
	    }
	}
    }
    print "ok $.\n";
}

close(TESTS);

sub infty_subst                             # Special-case substitution
{                                           #  of $reg_infty and friends
    my $tp = shift;
    $$tp =~ s/,\$reg_infty_m}/,$reg_infty_m}/o;
    $$tp =~ s/,\$reg_infty_p}/,$reg_infty_p}/o;
    $$tp =~ s/,\$reg_infty}/,$reg_infty}/o;
}

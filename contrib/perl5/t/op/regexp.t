#!./perl

# XXX known to leak scalars
$ENV{PERL_DESTRUCT_LEVEL} = 0 unless $ENV{PERL_DESTRUCT_LEVEL} > 3;

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
#
# Columns 4 and 5 are used only if column 3 contains C<y> or C<c>.
#
# Column 4 contains a string, usually C<$&>.
#
# Column 5 contains the expected result of double-quote
# interpolating that string after the match, or start of error message.
#
# \n in the tests are interpolated, as are variables of the form ${\w+}.
#
# If you want to add a regular expression test that can't be expressed
# in this format, don't add it here: put it in op/pat.t instead.

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib' if -d '../lib';
}

$iters = shift || 1;		# Poor man performance suite, 10000 is OK.

open(TESTS,'op/re_tests') || open(TESTS,'t/op/re_tests') ||
	die "Can't open re_tests";

while (<TESTS>) { }
$numtests = $.;
seek(TESTS,0,0);
$. = 0;

$bang = sprintf "\\%03o", ord "!"; # \41 would not be portable.

$| = 1;
print "1..$numtests\n# $iters iterations\n";
TEST:
while (<TESTS>) {
    chomp;
    s/\\n/\n/g;
    ($pat, $subject, $result, $repl, $expect) = split(/\t/,$_);
    $input = join(':',$pat,$subject,$result,$repl,$expect);
    infty_subst(\$pat);
    infty_subst(\$expect);
    $pat = "'$pat'" unless $pat =~ /^[:']/;
    $pat =~ s/\\n/\n/g;
    $pat =~ s/(\$\{\w+\})/$1/eeg;
    $subject =~ s/\\n/\n/g;
    $expect =~ s/\\n/\n/g;
    $expect = $repl = '-' if $skip_amp and $input =~ /\$[&\`\']/;
    for $study ("", "study \$subject") {
 	$c = $iters;
 	eval "$study; \$match = (\$subject =~ m$pat) while \$c--; \$got = \"$repl\";";
	chomp( $err = $@ );
	if ($result eq 'c') {
	    if ($err !~ m!^\Q$expect!) { print "not ok $. (compile) $input => `$err'\n"; next TEST }
	    last;  # no need to study a syntax error
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

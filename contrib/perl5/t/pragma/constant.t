#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use warnings;
use vars qw{ @warnings };
BEGIN {				# ...and save 'em for later
    $SIG{'__WARN__'} = sub { push @warnings, @_ }
}
END { print @warnings }

######################### We start with some black magic to print on failure.

BEGIN { $| = 1; print "1..73\n"; }
END {print "not ok 1\n" unless $loaded;}
use constant 1.01;
$loaded = 1;
#print "# Version: $constant::VERSION\n";
print "ok 1\n";

######################### End of black magic.

use strict;

sub test ($$;$) {
    my($num, $bool, $diag) = @_;
    if ($bool) {
	print "ok $num\n";
	return;
    }
    print "not ok $num\n";
    return unless defined $diag;
    $diag =~ s/\Z\n?/\n/;			# unchomp
    print map "# $num : $_", split m/^/m, $diag;
}

use constant PI		=> 4 * atan2 1, 1;

test 2, substr(PI, 0, 7) eq '3.14159';
test 3, defined PI;

sub deg2rad { PI * $_[0] / 180 }

my $ninety = deg2rad 90;

test 4, $ninety > 1.5707;
test 5, $ninety < 1.5708;

use constant UNDEF1	=> undef;	# the right way
use constant UNDEF2	=>	;	# the weird way
use constant 'UNDEF3'		;	# the 'short' way
use constant EMPTY	=> ( )  ;	# the right way for lists

test 6, not defined UNDEF1;
test 7, not defined UNDEF2;
test 8, not defined UNDEF3;
my @undef = UNDEF1;
test 9, @undef == 1;
test 10, not defined $undef[0];
@undef = UNDEF2;
test 11, @undef == 0;
@undef = UNDEF3;
test 12, @undef == 0;
@undef = EMPTY;
test 13, @undef == 0;

use constant COUNTDOWN	=> scalar reverse 1, 2, 3, 4, 5;
use constant COUNTLIST	=> reverse 1, 2, 3, 4, 5;
use constant COUNTLAST	=> (COUNTLIST)[-1];

test 14, COUNTDOWN eq '54321';
my @cl = COUNTLIST;
test 15, @cl == 5;
test 16, COUNTDOWN eq join '', @cl;
test 17, COUNTLAST == 1;
test 18, (COUNTLIST)[1] == 4;

use constant ABC	=> 'ABC';
test 19, "abc${\( ABC )}abc" eq "abcABCabc";

use constant DEF	=> 'D', 'E', chr ord 'F';
test 20, "d e f @{[ DEF ]} d e f" eq "d e f D E F d e f";

use constant SINGLE	=> "'";
use constant DOUBLE	=> '"';
use constant BACK	=> '\\';
my $tt = BACK . SINGLE . DOUBLE ;
test 21, $tt eq q(\\'");

use constant MESS	=> q('"'\\"'"\\);
test 22, MESS eq q('"'\\"'"\\);
test 23, length(MESS) == 8;

use constant TRAILING	=> '12 cats';
{
    no warnings 'numeric';
    test 24, TRAILING == 12;
}
test 25, TRAILING eq '12 cats';

use constant LEADING	=> " \t1234";
test 26, LEADING == 1234;
test 27, LEADING eq " \t1234";

use constant ZERO1	=> 0;
use constant ZERO2	=> 0.0;
use constant ZERO3	=> '0.0';
test 28, ZERO1 eq '0';
test 29, ZERO2 eq '0';
test 30, ZERO3 eq '0.0';

{
    package Other;
    use constant PI	=> 3.141;
}

test 31, (PI > 3.1415 and PI < 3.1416);
test 32, Other::PI == 3.141;

use constant E2BIG => $! = 7;
test 33, E2BIG == 7;
# This is something like "Arg list too long", but the actual message
# text may vary, so we can't test much better than this.
test 34, length(E2BIG) > 6;
test 35, index(E2BIG, " ") > 0;

test 36, @warnings == 0, join "\n", "unexpected warning", @warnings;
@warnings = ();		# just in case
undef &PI;
test 37, @warnings &&
    ($warnings[0] =~ /Constant sub.* undefined/),
    shift @warnings;

test 38, @warnings == 0, "unexpected warning";
test 39, 1;

use constant CSCALAR	=> \"ok 40\n";
use constant CHASH	=> { foo => "ok 41\n" };
use constant CARRAY	=> [ undef, "ok 42\n" ];
use constant CPHASH	=> [ { foo => 1 }, "ok 43\n" ];
use constant CCODE	=> sub { "ok $_[0]\n" };

print ${+CSCALAR};
print CHASH->{foo};
print CARRAY->[1];
print CPHASH->{foo};
eval q{ CPHASH->{bar} };
test 44, scalar($@ =~ /^No such pseudo-hash field/);
print CCODE->(45);
eval q{ CCODE->{foo} };
test 46, scalar($@ =~ /^Constant is not a HASH/);

# Allow leading underscore
use constant _PRIVATE => 47;
test 47, _PRIVATE == 47;

# Disallow doubled leading underscore
eval q{
    use constant __DISALLOWED => "Oops";
};
test 48, $@ =~ /begins with '__'/;

# Check on declared() and %declared. This sub should be EXACTLY the
# same as the one quoted in the docs!
sub declared ($) {
    use constant 1.01;              # don't omit this!
    my $name = shift;
    $name =~ s/^::/main::/;
    my $pkg = caller;
    my $full_name = $name =~ /::/ ? $name : "${pkg}::$name";
    $constant::declared{$full_name};
}

test 49, declared 'PI';
test 50, $constant::declared{'main::PI'};

test 51, !declared 'PIE';
test 52, !$constant::declared{'main::PIE'};

{
    package Other;
    use constant IN_OTHER_PACK => 42;
    ::test 53, ::declared 'IN_OTHER_PACK';
    ::test 54, $constant::declared{'Other::IN_OTHER_PACK'};
    ::test 55, ::declared 'main::PI';
    ::test 56, $constant::declared{'main::PI'};
}

test 57, declared 'Other::IN_OTHER_PACK';
test 58, $constant::declared{'Other::IN_OTHER_PACK'};

@warnings = ();
eval q{
    no warnings;
    use warnings 'constant';
    use constant 'BEGIN' => 1 ;
    use constant 'INIT' => 1 ;
    use constant 'CHECK' => 1 ;
    use constant 'END' => 1 ;
    use constant 'DESTROY' => 1 ;
    use constant 'AUTOLOAD' => 1 ;
    use constant 'STDIN' => 1 ;
    use constant 'STDOUT' => 1 ;
    use constant 'STDERR' => 1 ;
    use constant 'ARGV' => 1 ;
    use constant 'ARGVOUT' => 1 ;
    use constant 'ENV' => 1 ;
    use constant 'INC' => 1 ;
    use constant 'SIG' => 1 ;
};

test 59, @warnings == 14 ;
test 60, (shift @warnings) =~ /^Constant name 'BEGIN' is a Perl keyword at/;
test 61, (shift @warnings) =~ /^Constant name 'INIT' is a Perl keyword at/;
test 62, (shift @warnings) =~ /^Constant name 'CHECK' is a Perl keyword at/;
test 63, (shift @warnings) =~ /^Constant name 'END' is a Perl keyword at/;
test 64, (shift @warnings) =~ /^Constant name 'DESTROY' is a Perl keyword at/;
test 65, (shift @warnings) =~ /^Constant name 'AUTOLOAD' is a Perl keyword at/;
test 66, (shift @warnings) =~ /^Constant name 'STDIN' is forced into package main:: a/;
test 67, (shift @warnings) =~ /^Constant name 'STDOUT' is forced into package main:: at/;
test 68, (shift @warnings) =~ /^Constant name 'STDERR' is forced into package main:: at/;
test 69, (shift @warnings) =~ /^Constant name 'ARGV' is forced into package main:: at/;
test 70, (shift @warnings) =~ /^Constant name 'ARGVOUT' is forced into package main:: at/;
test 71, (shift @warnings) =~ /^Constant name 'ENV' is forced into package main:: at/;
test 72, (shift @warnings) =~ /^Constant name 'INC' is forced into package main:: at/;
test 73, (shift @warnings) =~ /^Constant name 'SIG' is forced into package main:: at/;
@warnings = ();

#!./perl

#
# Verify which OP= operators warn if their targets are undefined.
# Based on redef.t, contributed by Graham Barr <Graham.Barr@tiuk.ti.com>
#	-- Robin Barker <rmb@cise.npl.co.uk>
#

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use strict;
use warnings;

my $warn = "";
$SIG{q(__WARN__)} = sub { print $warn; $warn .= join("",@_) };

sub ok { print $_[1] ? "ok " : "not ok ", $_[0], "\n"; }

sub uninitialized { $warn =~ s/Use of uninitialized value[^\n]+\n//s; }
    
print "1..32\n";

{ my $x; $x ++;     ok  1, ! uninitialized; }
{ my $x; $x --;     ok  2, ! uninitialized; }
{ my $x; ++ $x;     ok  3, ! uninitialized; }
{ my $x; -- $x;	    ok  4, ! uninitialized; }

{ my $x; $x **= 1;  ok  5,  uninitialized; }

{ my $x; $x += 1;   ok  6, ! uninitialized; }
{ my $x; $x -= 1;   ok  7, ! uninitialized; }

{ my $x; $x .= 1;   ok  8, ! uninitialized; }

{ my $x; $x *= 1;   ok  9,  uninitialized; }
{ my $x; $x /= 1;   ok 10,  uninitialized; }
{ my $x; $x %= 1;   ok 11,  uninitialized; }

{ my $x; $x x= 1;   ok 12,  uninitialized; }

{ my $x; $x &= 1;   ok 13,  uninitialized; }
{ my $x; $x |= 1;   ok 14, ! uninitialized; }
{ my $x; $x ^= 1;   ok 15, ! uninitialized; }

{ my $x; $x &&= 1;  ok 16, ! uninitialized; }
{ my $x; $x ||= 1;  ok 17, ! uninitialized; }

{ my $x; $x <<= 1;  ok 18,  uninitialized; }
{ my $x; $x >>= 1;  ok 19,  uninitialized; }

{ my $x; $x &= "x"; ok 20,  uninitialized; }
{ my $x; $x |= "x"; ok 21, ! uninitialized; }
{ my $x; $x ^= "x"; ok 22, ! uninitialized; }

{ use integer; my $x; $x += 1; ok 23, ! uninitialized; }
{ use integer; my $x; $x -= 1; ok 24, ! uninitialized; }

{ use integer; my $x; $x *= 1; ok 25,  uninitialized; }
{ use integer; my $x; $x /= 1; ok 26,  uninitialized; }
{ use integer; my $x; $x %= 1; ok 27,  uninitialized; }

{ use integer; my $x; $x ++;   ok 28, ! uninitialized; }
{ use integer; my $x; $x --;   ok 29, ! uninitialized; }
{ use integer; my $x; ++ $x;   ok 30, ! uninitialized; }
{ use integer; my $x; -- $x;   ok 31, ! uninitialized; }

ok 32, $warn eq '';

# If we got any errors that we were not expecting, then print them
print map "#$_\n", split /\n/, $warn if length $warn;

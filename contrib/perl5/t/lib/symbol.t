#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..8\n";

BEGIN { $_ = 'foo'; }  # because Symbol used to clobber $_

use Symbol;

# First check $_ clobbering
print "not " if $_ ne 'foo';
print "ok 1\n";


# First test gensym()
$sym1 = gensym;
print "not " if ref($sym1) ne 'GLOB';
print "ok 2\n";

$sym2 = gensym;

print "not " if $sym1 eq $sym2;
print "ok 3\n";

ungensym $sym1;

$sym1 = $sym2 = undef;


# Test qualify()
package foo;

use Symbol qw(qualify);  # must import into this package too

qualify("x") eq "foo::x"          or print "not ";
print "ok 4\n";

qualify("x", "FOO") eq "FOO::x"   or print "not ";
print "ok 5\n";

qualify("BAR::x") eq "BAR::x"     or print "not ";
print "ok 6\n";

qualify("STDOUT") eq "main::STDOUT" or print "not ";
print "ok 7\n";

qualify("ARGV", "FOO") eq "main::ARGV" or print "not ";
print "ok 8\n";

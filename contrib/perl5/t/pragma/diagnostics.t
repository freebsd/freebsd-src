#!./perl

BEGIN {
    chdir '..' if -d '../pod' && -d '../t';
    @INC = 'lib';
}


######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)
use strict;
use warnings;

use vars qw($Test_Num $Total_tests);

my $loaded;
BEGIN { $| = 1; $Test_Num = 1 }
END {print "not ok $Test_Num\n" unless $loaded;}
print "1..$Total_tests\n";
BEGIN { require diagnostics; } # Don't want diagnostics' noise yet.
$loaded = 1;
ok($loaded, 'compile');
######################### End of black magic.

sub ok {
	my($test, $name) = shift;
	print "not " unless $test;
	print "ok $Test_Num";
	print " - $name" if defined $name;
	print "\n";
	$Test_Num++;
}


# Change this to your # of ok() calls + 1
BEGIN { $Total_tests = 1 }

#!./perl

#
# Verify that C<die> return the return code
#	-- Robin Barker <rmb@cise.npl.co.uk>
#

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib' if -e '../lib';
}
my $perl = -e '../perl' ? '../perl' : -e './perl' ? './perl' : 'perl';

use strict;

my %tests = (
	 1 => [   0,   0],
	 2 => [   0,   1], 
	 3 => [   0, 127], 
	 4 => [   0, 128], 
	 5 => [   0, 255], 
	 6 => [   0, 256], 
	 7 => [   0, 512], 
	 8 => [   1,   0],
	 9 => [   1,   1],
	10 => [   1, 256],
	11 => [ 128,   0],
	12 => [ 128,   1],
	13 => [ 128, 256],
	14 => [ 255,   0],
	15 => [ 255,   1],
	16 => [ 255, 256],
	# see if implicit close preserves $?
	17 => [  0,  512, '{ local *F; open F, q[TEST]; close F } die;'],
);

my $max = keys %tests;

print "1..$max\n";

foreach my $test (1 .. $max) {
    my($bang, $query, $code) = @{$tests{$test}};
    $code ||= 'die;';
    my $exit =
	($^O eq 'MSWin32'
	 ? system qq($perl -e "\$! = $bang; \$? = $query; $code" 2> nul)
	 : system qq($perl -e '\$! = $bang; \$? = $query; $code' 2> /dev/null));

    printf "# 0x%04x  0x%04x  0x%04x\n", $exit, $bang, $query;
    print "not " unless $exit == (($bang || ($query >> 8) || 255) << 8);
    print "ok $test\n";
}
    

#!perl

BEGIN {
    chdir( 't' ) if -d 't';
    unshift @INC, '../lib';
}

END {
    unlink 'tmon.out', 'err';
}

use Benchmark qw( timediff timestr );
use Getopt::Std 'getopts';
use Config '%Config';
getopts('vI:p:');

# -v   Verbose
# -I   Add to @INC
# -p   Name of perl binary

@tests = @ARGV ? @ARGV : sort <lib/dprof/*_t lib/dprof/*_v>;  # glob-sort, for OS/2

$path_sep = $Config{path_sep} || ':';
$perl5lib = $opt_I || join( $path_sep, @INC );
$perl = $opt_p || $^X;

if( $opt_v ){
	print "tests: @tests\n";
	print "perl: $perl\n";
	print "perl5lib: $perl5lib\n";
}
if( $perl =~ m|^\./| ){
	# turn ./perl into ../perl, because of chdir(t) above.
	$perl = ".$perl";
}
if( ! -f $perl ){ die "Where's Perl?" }

sub profile {
	my $test = shift;
	my @results;
	local $ENV{PERL5LIB} = $perl5lib;
	my $opt_d = '-d:DProf';

	my $t_start = new Benchmark;
	open( R, "$perl $opt_d $test |" ) || warn "$0: Can't run. $!\n";
	@results = <R>;
	close R;
	my $t_total = timediff( new Benchmark, $t_start );

	if( $opt_v ){
		print "\n";
		print @results
	}

	print timestr( $t_total, 'nop' ), "\n";
}


sub verify {
	my $test = shift;

	system $perl, '-I../lib', '-I./lib/dprof', $test,
		$opt_v?'-v':'', '-p', $perl;
}


$| = 1;
print "1..18\n";
while( @tests ){
	$test = shift @tests;
	if( $test =~ /_t$/i ){
		print "# $test" . '.' x (20 - length $test);
		profile $test;
	}
	else{
		verify $test;
	}
}

unlink("tmon.out");

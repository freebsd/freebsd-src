package V;

use Getopt::Std 'getopts';
getopts('vp:d:');

require Exporter;
@ISA = 'Exporter';

@EXPORT = qw( dprofpp $opt_v $results $expected report @results );
@EXPORT_OK = qw( notok ok $num );

$num = 0;
$results = $expected = '';
$perl = $opt_p || $^X;
$dpp = $opt_d || '../utils/dprofpp';
$dpp .= '.com' if $^O eq 'VMS';

print "\nperl: $perl\n" if $opt_v;
if( ! -f $perl ){ die "Where's Perl?" }
if( ! -f $dpp ) { 
    ($dpp = $^X) =~ s@(^.*)[/|\\].*@$1/dprofpp@;
    die "Where's dprofpp?" if( ! -f $dpp );
}

sub dprofpp {
	my $switches = shift;

        open( D, "$perl \"-I../lib\" $dpp \"$switches\" 2> err |" ) || warn "$0: Can't run. $!\n";
	@results = <D>;
	close D;

	open( D, "<err" ) || warn "$0: Can't open: $!\n";
	@err = <D>;
	close D;
	push( @results, @err ) if @err;

	$results = qq{@results};
	# ignore Loader (Dyna/Auto etc), leave newline
	$results =~ s/^\w+Loader::import//;
	$results =~ s/\n /\n/gm;
	$results;
}

sub report {
	$num = shift;
	my $sub = shift;
	my $x;

	$x = &$sub;
	$x ? &ok : &notok;
}

sub ok {
	print "ok $num\n";
}

sub notok {
	print "not ok $num\n";
	print "\nResult\n{$results}\n";
	print "Expected\n{$expected}\n";
}

1;

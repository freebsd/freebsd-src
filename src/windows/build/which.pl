#!perl -w

use strict;
use Config;
use File::Basename;
use Getopt::Long;

$0 = fileparse($0);

sub main
{
    Getopt::Long::Configure('bundling', 'no_auto_abbrev',
			    'no_getopt_compat', 'require_order',
			    'ignore_case', 'pass_through',
			    'prefix_pattern=(--|-|\+|\/)',
			   );
    my $OPT = {};
    GetOptions($OPT,
	       'help|h|?',
	       'all|a',
	       'quiet|q',
	       'debug|d',
	       'path:s',
	       );

    my $f = shift @ARGV;
    if ($OPT->{help} || !$f) {
	usage();
	exit(0) if $OPT->{help};
	exit(1);
    }

    my $p = $OPT->{path} || $ENV{PATH};
    my $s = $Config{path_sep};
    my @d = split(/$s/, $p);
    my @e = split(/$s/, lc($ENV{PATHEXT} || '.bat;.exe;.com'));
    my @f = ($f, map { $f.$_; } @e);
    my $found = 0;
    foreach my $d (@d) {
	print "(Searching $d)\n" if $OPT->{debug};
	foreach my $f (@f) {
	    my $df = $d.'\\'.$f; # cannot use $File::Spec->catfile due to UNC.
	    print "(Checking for $df)\n" if $OPT->{debug};
	    if (-f $df) {
		exit(0) if $OPT->{quiet};
		print "$df\n";
		exit(0) if !$OPT->{all};
		$found = 1;
	    }
	}
    }
    print "Could not find $f\n" if !$found && !$OPT->{quiet};
    exit($found?0:1);
}

sub usage
{
    print <<USAGE;
Usage: $0 [options] command
    command         find file executed by this command by looking at PATH
    -d, --debug     debug output
    -a, --all       find all such commands in PATH
    -q, --quiet     no output, exit with non-zero errorcode if not found
    --path PATHARG  search PATHARG instead of the PATH environment variable
    -?, -H, --help  help
USAGE
}

main();

# -*- perl -*-

# Crude template instantiation hack.
#
# The template named on the command line maps to a perl module t_$foo
# which defines certain methods including variable processing and
# output generation.  It can also suck in additional template modules
# for internal use.  One output file is generated, which typically
# contains structures and inline functions, and should be included by
# other files which will define, for example, the typedefname
# parameters supplied to this script.

# To do:
# Find a way to make dependency generation automatic.
# Make it less gross.

sub usage {
    print STDERR "usage: $0 TemplateName [-oOutputFile] PARM=value ...\n";
    print STDERR "  where acceptable PARM values depend on the template\n";
    exit(1);
}

my $orig_args = join(" ", @ARGV);
my $templatename = shift @ARGV || &usage;
my $outfile = shift @ARGV || &usage;
my $x;

eval "require t_$templatename;" || die;
eval "\$x = new t_$templatename;" || die;

sub getparms {
    my $arg;
    my $outfile;
    my %allowed_parms = ();

    foreach $arg (@ARGV) {
	my @words = split '=', $arg;
	if ($#words != 1) {
	    print STDERR "$0: $arg : #words = $#words\n";
	    &usage;
	}
	$x->setparm($words[0], $words[1]);
    }
}

sub generate {
    open OUTFILE, ">$outfile" || die;
    print OUTFILE "/*\n";
    print OUTFILE " * This file is generated, please don't edit it.\n";
    print OUTFILE " * script: $0\n";
    print OUTFILE " * args:   $orig_args\n";
    print OUTFILE " * The rest of this file is copied from a template, with\n";
    print OUTFILE " * substitutions.  See the template for copyright info.\n";
    print OUTFILE " */\n";
    $x->output(\*OUTFILE);
    close OUTFILE;
}

&getparms;
&generate;
exit (0);

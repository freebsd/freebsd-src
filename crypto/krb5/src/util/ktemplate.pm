# -*- perl -*-

sub usage {
    print STDERR "usage: $0 -oOutputFile PARM=value ...\n";
    print STDERR "  where acceptable PARM values are:\n";
    print STDERR "\t", join(' ', @parms), "\n";
    exit(1);
}

%parm = ();
sub run {
    my $arg;
    my $outfile;
    my %allowed_parms = ();

    foreach $arg (@parms) { $allowed_parms{$arg} = 1; }

    foreach $arg (@ARGV) {
	if ($arg =~ /^-o./) {
	    if (defined $outfile) {
		die "$0: Output file specified multiple times\n";
	    }
	    $outfile = substr($arg, 2);
	} else {
	    my @words = split '=', $arg;
	    if ($#words != 1) {
		print STDERR "$0: $arg : #words = $#words\n";
		&usage;
	    }
	    if (!defined $allowed_parms{$words[0]}) {
		print STDERR "$0: Unknown parameter $words[0]\n";
		&usage;
	    }
	    $parm{$words[0]} = $words[1];
	}
    }
    my $p;
    my $subst = "";
    #print "Keys defined: ", join(' ', keys %parm), "\n";
    foreach $p (@parms) {
	if (!defined $parm{$p}) {
	    die "$0: No value supplied for parameter $p\n";
	}
	# XXX More careful quoting of supplied value!
	$subst .= "\t\$a =~ s|<$p>|$parm{$p}|go;\n";
    }
    $subst = "sub do_substitution {\n"
	. "\tmy(\$a) = \@_;\n"
	. $subst
	. "\treturn \$a;\n"
	. "}\n"
	. "1;";
    eval $subst || die;
    if (defined $outfile) {
	open OUTFILE, ">$outfile" || die;
    } else {
	print STDERR "$0: No output file specified.\n";
	&usage;
    }
    print OUTFILE "/*\n";
    print OUTFILE " * This file is generated, please don't edit it.\n";
    print OUTFILE " * script: $0\n";
    print OUTFILE " * args:\n *\t", join("\n *\t", @ARGV), "\n";
    print OUTFILE " * The rest of this file is copied from a template, with\n";
    print OUTFILE " * substitutions.  See the template for copyright info.\n";
    print OUTFILE " */\n";
    while (<DATA>) {
	print OUTFILE &do_substitution($_);
    }
    close OUTFILE;
    exit (0);
}

1;

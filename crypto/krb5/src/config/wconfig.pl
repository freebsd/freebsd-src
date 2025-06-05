#! perl
$win_flag = "WIN32##";
@wflags = ();
$mit_specific = 0;
@ignore_list = ( "DOS#?#?" );

foreach $arg (@ARGV) {
    if ($arg =~ /^-/) { push @wflags, $arg; }
    if ("--mit" eq $arg) {
	$mit_specific = 1;
    } elsif ("--win16" eq $arg) {
	$win_flag = "WIN16##";
    } elsif ("--win32" eq $arg) {
	$win_flag = "WIN32##";
    } elsif ($arg =~ /^--enable-/) {
	my($a) = $arg . "##";
	$a =~ s/^--enable-//;
	$a =~ tr/a-z/A-Z/;
	push @ignore_list, $a;
    } elsif ($arg =~ /^--ignore=/) {
	my($a) = $arg;
	$a =~ s/--ignore=//;
	push @ignore_list, $a;
    } elsif ($arg =~ /^-/) {
	print STDERR "Invalid option '$arg'\n";
	exit 1;
    } else {
	if (! defined $dir) {
	    $dir = $arg;
	}
    }
}
push @ignore_list, $win_flag;
push @ignore_list, "MIT##" if $mit_specific;

if ($#wflags >= 0) { printf "WCONFIG_FLAGS=%s\n", join (" ", @wflags); }

# This has a couple variations from the old wconfig.c.
#
# The old script wouldn't treat the input strings as regular expressions.
# This one does, and actually it builds one regexp, so the strict order of
# checks done by wconfig.c no longer applies.
#
# And the old script would change "##DOS#" to "#", whereas this
# version (with the regexp given above) will accept and discard 0, 1
# or 2 "#" marks.
$sub = "sub do_subst { my (\$a) = shift; \$a =~ s/^##(" . join("|", @ignore_list) . ")//; return \$a; }";
#print STDERR $sub, "\n";
eval $sub;

sub process {
    my $fh = shift;
    while (<$fh>) {
	if (/^@/) {
	    # This branch isn't actually used as far as I can tell.
	    print "\n";
	    next;
	}
	# Do we want to do any autoconf-style @FOO@ substitutions?
	# s/@MAINT@/#/g;
	# Are there any options we might want to set at configure time?
	print &do_subst($_);
    }
}

if (defined $dir) {
    open AUX, "<$dir/win-pre.in" || die "Couldn't open win-pre.in: $!\n";
    &process(\*AUX);
    close AUX;
}
&process(\*STDIN);
if (defined $dir) {
    open AUX, "<$dir/win-post.in" || die "Couldn't open win-post.in: $!\n";
    &process(\*AUX);
    close AUX;
}
exit 0;

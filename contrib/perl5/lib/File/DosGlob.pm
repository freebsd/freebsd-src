#!perl -w

#
# Documentation at the __END__
#

package File::DosGlob;

sub doglob {
    my $cond = shift;
    my @retval = ();
    #print "doglob: ", join('|', @_), "\n";
  OUTER:
    for my $arg (@_) {
        local $_ = $arg;
	my @matched = ();
	my @globdirs = ();
	my $head = '.';
	my $sepchr = '/';
	next OUTER unless defined $_ and $_ ne '';
	# if arg is within quotes strip em and do no globbing
	if (/^"(.*)"$/) {
	    $_ = $1;
	    if ($cond eq 'd') { push(@retval, $_) if -d $_ }
	    else              { push(@retval, $_) if -e $_ }
	    next OUTER;
	}
	if (m|^(.*)([\\/])([^\\/]*)$|) {
	    my $tail;
	    ($head, $sepchr, $tail) = ($1,$2,$3);
	    #print "div: |$head|$sepchr|$tail|\n";
	    push (@retval, $_), next OUTER if $tail eq '';
	    if ($head =~ /[*?]/) {
		@globdirs = doglob('d', $head);
		push(@retval, doglob($cond, map {"$_$sepchr$tail"} @globdirs)),
		    next OUTER if @globdirs;
	    }
	    $head .= $sepchr if $head eq '' or $head =~ /^[A-Za-z]:$/;
	    $_ = $tail;
	}
	#
	# If file component has no wildcards, we can avoid opendir
	unless (/[*?]/) {
	    $head = '' if $head eq '.';
	    $head .= $sepchr unless $head eq '' or substr($head,-1) eq $sepchr;
	    $head .= $_;
	    if ($cond eq 'd') { push(@retval,$head) if -d $head }
	    else              { push(@retval,$head) if -e $head }
	    next OUTER;
	}
	opendir(D, $head) or next OUTER;
	my @leaves = readdir D;
	closedir D;
	$head = '' if $head eq '.';
	$head .= $sepchr unless $head eq '' or substr($head,-1) eq $sepchr;

	# escape regex metachars but not glob chars
	s:([].+^\-\${}[|]):\\$1:g;
	# and convert DOS-style wildcards to regex
	s/\*/.*/g;
	s/\?/.?/g;

	#print "regex: '$_', head: '$head'\n";
	my $matchsub = eval 'sub { $_[0] =~ m|^' . $_ . '$|io }';
	warn($@), next OUTER if $@;
      INNER:
	for my $e (@leaves) {
	    next INNER if $e eq '.' or $e eq '..';
	    next INNER if $cond eq 'd' and ! -d "$head$e";
	    push(@matched, "$head$e"), next INNER if &$matchsub($e);
	    #
	    # [DOS compatibility special case]
	    # Failed, add a trailing dot and try again, but only
	    # if name does not have a dot in it *and* pattern
	    # has a dot *and* name is shorter than 9 chars.
	    #
	    if (index($e,'.') == -1 and length($e) < 9
	        and index($_,'\\.') != -1) {
		push(@matched, "$head$e"), next INNER if &$matchsub("$e.");
	    }
	}
	push @retval, @matched if @matched;
    }
    return @retval;
}

#
# this can be used to override CORE::glob in a specific
# package by saying C<use File::DosGlob 'glob';> in that
# namespace.
#

# context (keyed by second cxix arg provided by core)
my %iter;
my %entries;

sub glob {
    my $pat = shift;
    my $cxix = shift;
    my @pat;

    # glob without args defaults to $_
    $pat = $_ unless defined $pat;

    # extract patterns
    if ($pat =~ /\s/) {
	require Text::ParseWords;
	@pat = Text::ParseWords::parse_line('\s+',0,$pat);
    }
    else {
	push @pat, $pat;
    }

    # assume global context if not provided one
    $cxix = '_G_' unless defined $cxix;
    $iter{$cxix} = 0 unless exists $iter{$cxix};

    # if we're just beginning, do it all first
    if ($iter{$cxix} == 0) {
	$entries{$cxix} = [doglob(1,@pat)];
    }

    # chuck it all out, quick or slow
    if (wantarray) {
	delete $iter{$cxix};
	return @{delete $entries{$cxix}};
    }
    else {
	if ($iter{$cxix} = scalar @{$entries{$cxix}}) {
	    return shift @{$entries{$cxix}};
	}
	else {
	    # return undef for EOL
	    delete $iter{$cxix};
	    delete $entries{$cxix};
	    return undef;
	}
    }
}

sub import {
    my $pkg = shift;
    return unless @_;
    my $sym = shift;
    my $callpkg = ($sym =~ s/^GLOBAL_// ? 'CORE::GLOBAL' : caller(0));
    *{$callpkg.'::'.$sym} = \&{$pkg.'::'.$sym} if $sym eq 'glob';
}

1;

__END__

=head1 NAME

File::DosGlob - DOS like globbing and then some

=head1 SYNOPSIS

    require 5.004;
    
    # override CORE::glob in current package
    use File::DosGlob 'glob';
    
    # override CORE::glob in ALL packages (use with extreme caution!)
    use File::DosGlob 'GLOBAL_glob';

    @perlfiles = glob  "..\\pe?l/*.p?";
    print <..\\pe?l/*.p?>;
    
    # from the command line (overrides only in main::)
    > perl -MFile::DosGlob=glob -e "print <../pe*/*p?>"

=head1 DESCRIPTION

A module that implements DOS-like globbing with a few enhancements.
It is largely compatible with perlglob.exe (the M$ setargv.obj
version) in all but one respect--it understands wildcards in
directory components.

For example, C<<..\\l*b\\file/*glob.p?>> will work as expected (in
that it will find something like '..\lib\File/DosGlob.pm' alright).
Note that all path components are case-insensitive, and that
backslashes and forward slashes are both accepted, and preserved.
You may have to double the backslashes if you are putting them in
literally, due to double-quotish parsing of the pattern by perl.

Spaces in the argument delimit distinct patterns, so
C<glob('*.exe *.dll')> globs all filenames that end in C<.exe>
or C<.dll>.  If you want to put in literal spaces in the glob
pattern, you can escape them with either double quotes, or backslashes.
e.g. C<glob('c:/"Program Files"/*/*.dll')>, or
C<glob('c:/Program\ Files/*/*.dll')>.  The argument is tokenized using
C<Text::ParseWords::parse_line()>, so see L<Text::ParseWords> for details
of the quoting rules used.

Extending it to csh patterns is left as an exercise to the reader.

=head1 EXPORTS (by request only)

glob()

=head1 BUGS

Should probably be built into the core, and needs to stop
pandering to DOS habits.  Needs a dose of optimizium too.

=head1 AUTHOR

Gurusamy Sarathy <gsar@umich.edu>

=head1 HISTORY

=over 4

=item *

Support for globally overriding glob() (GSAR 3-JUN-98)

=item *

Scalar context, independent iterator context fixes (GSAR 15-SEP-97)

=item *

A few dir-vs-file optimizations result in glob importation being
10 times faster than using perlglob.exe, and using perlglob.bat is
only twice as slow as perlglob.exe (GSAR 28-MAY-97)

=item *

Several cleanups prompted by lack of compatible perlglob.exe
under Borland (GSAR 27-MAY-97)

=item *

Initial version (GSAR 20-FEB-97)

=back

=head1 SEE ALSO

perl

perlglob.bat

Text::ParseWords

=cut


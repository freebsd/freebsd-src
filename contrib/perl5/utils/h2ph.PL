#!/usr/local/bin/perl
# $FreeBSD$

use Config;
use File::Basename qw(basename dirname);
use Cwd;

# List explicitly here the variables you want Configure to
# generate.  Metaconfig only looks for shell variables, so you
# have to mention them as if they were shell variables, not
# %Config entries.  Thus you write
#  $startperl
# to ensure Configure will look for $Config{startperl}.
# Wanted:  $archlibexp

# This forces PL files to create target in same directory as PL file.
# This is so that make depend always knows where to find PL derivatives.
$origdir = cwd;
chdir dirname($0);
$file = basename($0, '.PL');
$file .= '.com' if $^O eq 'VMS';

open OUT,">$file" or die "Can't create $file: $!";

print "Extracting $file (with variable substitutions)\n";

# In this section, perl variables will be expanded during extraction.
# You can use $Config{...} to use Configure variables.

print OUT <<"!GROK!THIS!";
$Config{startperl}
    eval 'exec $Config{perlpath} -S \$0 \${1+"\$@"}'
	if \$running_under_some_shell;
!GROK!THIS!

# In the following, perl variables are not expanded during extraction.

print OUT <<'!NO!SUBS!';

use strict;

use Config;
use File::Path qw(mkpath);
use Getopt::Std;

getopts('Dd:rlhaQ');
use vars qw($opt_D $opt_d $opt_r $opt_l $opt_h $opt_a $opt_Q);
die "-r and -a options are mutually exclusive\n" if ($opt_r and $opt_a);
my @inc_dirs = inc_dirs() if $opt_a;

my $Exit = 0;

my $Dest_dir = $opt_d || $Config{installarchlib};
die "Destination directory $Dest_dir doesn't exist or isn't a directory\n"
    unless -d $Dest_dir;

my @isatype = split(' ',<<END);
	char	uchar	u_char
	short	ushort	u_short
	int	uint	u_int
	long	ulong	u_long
	FILE	key_t	caddr_t
END

my %isatype;
@isatype{@isatype} = (1) x @isatype;
my $inif = 0;
my %Is_converted;

@ARGV = ('-') unless @ARGV;

build_preamble_if_necessary();

my ($t, $tab, %curargs, $new, $eval_index, $dir, $name, $args, $outfile);
my ($incl, $next);
while (defined (my $file = next_file())) {
    if (-l $file and -d $file) {
        link_if_possible($file) if ($opt_l);
        next;
    }

    # Recover from header files with unbalanced cpp directives
    $t = '';
    $tab = 0;

    # $eval_index goes into ``#line'' directives, to help locate syntax errors:
    $eval_index = 1;

    if ($file eq '-') {
	open(IN, "-");
	open(OUT, ">-");
    } else {
	($outfile = $file) =~ s/\.h$/.ph/ || next;
	print "$file -> $outfile\n" unless $opt_Q;
	if ($file =~ m|^(.*)/|) {
	    $dir = $1;
	    mkpath "$Dest_dir/$dir";
	}

	if ($opt_a) { # automagic mode:  locate header file in @inc_dirs
	    foreach (@inc_dirs) {
		chdir $_;
		last if -f $file;
	    }
	}

	open(IN,"$file") || (($Exit = 1),(warn "Can't open $file: $!\n"),next);
	open(OUT,">$Dest_dir/$outfile") || die "Can't create $outfile: $!\n";
    }

    print OUT "require '_h2ph_pre.ph';\n\n";
    while (<IN>) {
	chop;
	while (/\\$/) {
	    chop;
	    $_ .= <IN>;
	    chop;
	}
	print OUT "# $_\n" if $opt_D;

	if (s:/\*:\200:g) {
	    s:\*/:\201:g;
	    s/\200[^\201]*\201//g;	# delete single line comments
	    if (s/\200.*//) {		# begin multi-line comment?
		$_ .= '/*';
		$_ .= <IN>;
		redo;
	    }
	}
	if (s/^\s*\#\s*//) {
	    if (s/^define\s+(\w+)//) {
		$name = $1;
		$new = '';
		s/\s+$//;
		if (s/^\(([\w,\s]*)\)//) {
		    $args = $1;
    	    	    my $proto = '() ';
		    if ($args ne '') {
    	    	    	$proto = '';
			foreach my $arg (split(/,\s*/,$args)) {
			    $arg =~ s/^\s*([^\s].*[^\s])\s*$/$1/;
			    $curargs{$arg} = 1;
			}
			$args =~ s/\b(\w)/\$$1/g;
			$args = "local($args) = \@_;\n$t    ";
		    }
		    s/^\s+//;
		    expr();
		    $new =~ s/(["\\])/\\$1/g;       #"]);
		    $new = reindent($new);
		    $args = reindent($args);
		    if ($t ne '') {
			$new =~ s/(['\\])/\\$1/g;   #']);
			if ($opt_h) {
			    print OUT $t,
                            "eval \"\\n#line $eval_index $outfile\\n\" . 'sub $name $proto\{\n$t    ${args}eval q($new);\n$t}' unless defined(\&$name);\n";
                            $eval_index++;
			} else {
			    print OUT $t,
                            "eval 'sub $name $proto\{\n$t    ${args}eval q($new);\n$t}' unless defined(\&$name);\n";
			}
		    } else {
                      print OUT "unless(defined(\&$name)) {\n    sub $name $proto\{\n\t${args}eval q($new);\n    }\n}\n";
		    }
		    %curargs = ();
		} else {
		    s/^\s+//;
		    expr();
		    $new = 1 if $new eq '';
		    $new = reindent($new);
		    $args = reindent($args);
		    if ($t ne '') {
			$new =~ s/(['\\])/\\$1/g;        #']);

			if ($opt_h) {
			    print OUT $t,"eval \"\\n#line $eval_index $outfile\\n\" . 'sub $name () {",$new,";}' unless defined(\&$name);\n";
			    $eval_index++;
			} else {
			    print OUT $t,"eval 'sub $name () {",$new,";}' unless defined(\&$name);\n";
			}
		    } else {
		    	# Shunt around such directives as `#define FOO FOO':
		    	next if " \&$name" eq $new;

                      print OUT $t,"unless(defined(\&$name)) {\n    sub $name () {\t",$new,";}\n}\n";
		    }
		}
	    } elsif (/^(include|import)\s*[<"](.*)[>"]/) {
		($incl = $2) =~ s/\.h$/.ph/;
		print OUT $t,"require '$incl';\n";
	    } elsif(/^include_next\s*[<"](.*)[>"]/) {
		($incl = $1) =~ s/\.h$/.ph/;
		print OUT ($t,
			   "eval {\n");
                $tab += 4;
                $t = "\t" x ($tab / 8) . ' ' x ($tab % 8);
		print OUT ($t,
			   "my(\%INCD) = map { \$INC{\$_} => 1 } ",
			   "(grep { \$_ eq \"$incl\" } keys(\%INC));\n");
		print OUT ($t,
			   "my(\@REM) = map { \"\$_/$incl\" } ",
			   "(grep { not exists(\$INCD{\"\$_/$incl\"})",
			   "and -f \"\$_/$incl\" } \@INC);\n");
		print OUT ($t,
			   "require \"\$REM[0]\" if \@REM;\n");
                $tab -= 4;
                $t = "\t" x ($tab / 8) . ' ' x ($tab % 8);
                print OUT ($t,
			   "};\n");
		print OUT ($t,
			   "warn(\$\@) if \$\@;\n");
	    } elsif (/^ifdef\s+(\w+)/) {
		print OUT $t,"if(defined(&$1)) {\n";
		$tab += 4;
		$t = "\t" x ($tab / 8) . ' ' x ($tab % 8);
	    } elsif (/^ifndef\s+(\w+)/) {
		print OUT $t,"unless(defined(&$1)) {\n";
		$tab += 4;
		$t = "\t" x ($tab / 8) . ' ' x ($tab % 8);
	    } elsif (s/^if\s+//) {
		$new = '';
		$inif = 1;
		expr();
		$inif = 0;
		print OUT $t,"if($new) {\n";
		$tab += 4;
		$t = "\t" x ($tab / 8) . ' ' x ($tab % 8);
	    } elsif (s/^elif\s+//) {
		$new = '';
		$inif = 1;
		expr();
		$inif = 0;
		$tab -= 4;
		$t = "\t" x ($tab / 8) . ' ' x ($tab % 8);
		print OUT $t,"}\n elsif($new) {\n";
		$tab += 4;
		$t = "\t" x ($tab / 8) . ' ' x ($tab % 8);
	    } elsif (/^else/) {
		$tab -= 4;
		$t = "\t" x ($tab / 8) . ' ' x ($tab % 8);
		print OUT $t,"} else {\n";
		$tab += 4;
		$t = "\t" x ($tab / 8) . ' ' x ($tab % 8);
	    } elsif (/^endif/) {
		$tab -= 4;
		$t = "\t" x ($tab / 8) . ' ' x ($tab % 8);
		print OUT $t,"}\n";
	    } elsif(/^undef\s+(\w+)/) {
		print OUT $t, "undef(&$1) if defined(&$1);\n";
	    } elsif(/^error\s+(".*")/) {
		print OUT $t, "die($1);\n";
	    } elsif(/^error\s+(.*)/) {
		print OUT $t, "die(\"", quotemeta($1), "\");\n";
	    } elsif(/^warning\s+(.*)/) {
		print OUT $t, "warn(\"", quotemeta($1), "\");\n";
	    } elsif(/^ident\s+(.*)/) {
		print OUT $t, "# $1\n";
	    }
 	} elsif(/^\s*(typedef\s*)?enum\s*(\s+[a-zA-Z_]\w*\s*)?\{/) {
	    until(/\}.*?;/) {
		chomp($next = <IN>);
		$_ .= $next;
		print OUT "# $next\n" if $opt_D;
	    }
	    s@/\*.*?\*/@@g;
	    s/\s+/ /g;
	    /^\s?(typedef\s?)?enum\s?([a-zA-Z_]\w*)?\s?\{(.*)\}\s?([a-zA-Z_]\w*)?\s?;/;
	    (my $enum_subs = $3) =~ s/\s//g;
	    my @enum_subs = split(/,/, $enum_subs);
	    my $enum_val = -1;
	    foreach my $enum (@enum_subs) {
		my ($enum_name, $enum_value) = $enum =~ /^([a-zA-Z_]\w*)(=.+)?$/;
		$enum_value =~ s/^=//;
		$enum_val = (length($enum_value) ? $enum_value : $enum_val + 1);
		if ($opt_h) {
		    print OUT ($t,
			       "eval(\"\\n#line $eval_index $outfile\\n",
			       "sub $enum_name () \{ $enum_val; \}\") ",
			       "unless defined(\&$enum_name);\n");
		    ++ $eval_index;
		} else {
		    print OUT ($t,
			       "eval(\"sub $enum_name () \{ $enum_val; \}\") ",
			       "unless defined(\&$enum_name);\n");
		}
	    }
	}
    }
    print OUT "1;\n";

    $Is_converted{$file} = 1;
    queue_includes_from($file) if ($opt_a);
}

exit $Exit;


sub reindent($) {
    my($text) = shift;
    $text =~ s/\n/\n    /g;
    $text =~ s/        /\t/g;
    $text;
}


sub expr {
    my $joined_args;
    if(keys(%curargs)) {
	$joined_args = join('|', keys(%curargs));
    }
    while ($_ ne '') {
	s/^\&\&// && do { $new .= " &&"; next;}; # handle && operator
	s/^\&([\(a-z\)]+)/$1/i;	# hack for things that take the address of
	s/^(\s+)//		&& do {$new .= ' '; next;};
	s/^(0X[0-9A-F]+)[UL]*//i	&& do {$new .= lc($1); next;};
	s/^(-?\d+\.\d+E[-+]\d+)F?//i	&& do {$new .= $1; next;};
	s/^(\d+)\s*[LU]*//i	&& do {$new .= $1; next;};
	s/^("(\\"|[^"])*")//	&& do {$new .= $1; next;};
	s/^'((\\"|[^"])*)'//	&& do {
	    if ($curargs{$1}) {
		$new .= "ord('\$$1')";
	    } else {
		$new .= "ord('$1')";
	    }
	    next;
	};
        # replace "sizeof(foo)" with "{foo}"
        # also, remove * (C dereference operator) to avoid perl syntax
        # problems.  Where the %sizeof array comes from is anyone's
        # guess (c2ph?), but this at least avoids fatal syntax errors.
        # Behavior is undefined if sizeof() delimiters are unbalanced.
        # This code was modified to able to handle constructs like this:
        #   sizeof(*(p)), which appear in the HP-UX 10.01 header files.
        s/^sizeof\s*\(// && do {
            $new .= '$sizeof';
            my $lvl = 1;  # already saw one open paren
            # tack { on the front, and skip it in the loop
            $_ = "{" . "$_";
            my $index = 1;
            # find balanced closing paren
            while ($index <= length($_) && $lvl > 0) {
                $lvl++ if substr($_, $index, 1) eq "(";
                $lvl-- if substr($_, $index, 1) eq ")";
                $index++;
            }
            # tack } on the end, replacing )
            substr($_, $index - 1, 1) = "}";
            # remove pesky * operators within the sizeof argument
            substr($_, 0, $index - 1) =~ s/\*//g;
            next;
        };
	# Eliminate typedefs
	/\(([\w\s]+)[\*\s]*\)\s*[\w\(]/ && do {
	    foreach (split /\s+/, $1) {  # Make sure all the words are types,
		last unless ($isatype{$_} or $_ eq 'struct');
	    }
	    s/\([\w\s]+[\*\s]*\)// && next;      # then eliminate them.
	};
	# struct/union member, including arrays:
	s/^([_A-Z]\w*(\[[^\]]+\])?((\.|->)[_A-Z]\w*(\[[^\]]+\])?)+)//i && do {
	    my $id = $1;
	    $id =~ s/(\.|(->))([^\.\-]*)/->\{$3\}/g;
	    $id =~ s/\b([^\$])($joined_args)/$1\$$2/g if length($joined_args);
	    while($id =~ /\[\s*([^\$\&\d\]]+)\]/) {
		my($index) = $1;
		$index =~ s/\s//g;
		if(exists($curargs{$index})) {
		    $index = "\$$index";
		} else {
		    $index = "&$index";
		}
		$id =~ s/\[\s*([^\$\&\d\]]+)\]/[$index]/;
	    }
	    $new .= " (\$$id)";
	};
	s/^([_a-zA-Z]\w*)//	&& do {
	    my $id = $1;
	    if ($id eq 'struct') {
		s/^\s+(\w+)//;
		$id .= ' ' . $1;
		$isatype{$id} = 1;
	    } elsif ($id =~ /^((un)?signed)|(long)|(short)$/) {
		while (s/^\s+(\w+)//) { $id .= ' ' . $1; }
		$isatype{$id} = 1;
	    }
	    if ($curargs{$id}) {
		$new .= "\$$id";
		$new .= '->' if /^[\[\{]/;
	    } elsif ($id eq 'defined') {
		$new .= 'defined';
	    } elsif (/^\(/) {
		s/^\((\w),/("$1",/ if $id =~ /^_IO[WR]*$/i;	# cheat
		$new .= " &$id";
	    } elsif ($isatype{$id}) {
		if ($new =~ /{\s*$/) {
		    $new .= "'$id'";
		} elsif ($new =~ /\(\s*$/ && /^[\s*]*\)/) {
		    $new =~ s/\(\s*$//;
		    s/^[\s*]*\)//;
		} else {
		    $new .= q(').$id.q(');
		}
	    } else {
		if ($inif && $new !~ /defined\s*\($/) {
		    $new .= '(defined(&' . $id . ') ? &' . $id . ' : 0)';
		} elsif (/^\[/) {
		    $new .= " \$$id";
		} else {
		    $new .= ' &' . $id;
		}
	    }
	    next;
	};
	s/^(.)// && do { if ($1 ne '#') { $new .= $1; } next;};
    }
}


# Handle recursive subdirectories without getting a grotesquely big stack.
# Could this be implemented using File::Find?
sub next_file
{
    my $file;

    while (@ARGV) {
        $file = shift @ARGV;

        if ($file eq '-' or -f $file or -l $file) {
            return $file;
        } elsif (-d $file) {
            if ($opt_r) {
                expand_glob($file);
            } else {
                print STDERR "Skipping directory `$file'\n";
            }
        } elsif ($opt_a) {
            return $file;
        } else {
            print STDERR "Skipping `$file':  not a file or directory\n";
        }
    }

    return undef;
}


# Put all the files in $directory into @ARGV for processing.
sub expand_glob
{
    my ($directory)  = @_;

    $directory =~ s:/$::;

    opendir DIR, $directory;
        foreach (readdir DIR) {
            next if ($_ eq '.' or $_ eq '..');

            # expand_glob() is going to be called until $ARGV[0] isn't a
            # directory; so push directories, and unshift everything else.
            if (-d "$directory/$_") { push    @ARGV, "$directory/$_" }
            else                    { unshift @ARGV, "$directory/$_" }
        }
    closedir DIR;
}


# Given $file, a symbolic link to a directory in the C include directory,
# make an equivalent symbolic link in $Dest_dir, if we can figure out how.
# Otherwise, just duplicate the file or directory.
sub link_if_possible
{
    my ($dirlink)  = @_;
    my $target  = eval 'readlink($dirlink)';

    if ($target =~ m:^\.\./: or $target =~ m:^/:) {
        # The target of a parent or absolute link could leave the $Dest_dir
        # hierarchy, so let's put all of the contents of $dirlink (actually,
        # the contents of $target) into @ARGV; as a side effect down the
        # line, $dirlink will get created as an _actual_ directory.
        expand_glob($dirlink);
    } else {
        if (-l "$Dest_dir/$dirlink") {
            unlink "$Dest_dir/$dirlink" or
                print STDERR "Could not remove link $Dest_dir/$dirlink:  $!\n";
        }

        if (eval 'symlink($target, "$Dest_dir/$dirlink")') {
            print "Linking $target -> $Dest_dir/$dirlink\n";

            # Make sure that the link _links_ to something:
            if (! -e "$Dest_dir/$target") {
                mkpath("$Dest_dir/$target", 0755) or
                    print STDERR "Could not create $Dest_dir/$target/\n";
            }
        } else {
            print STDERR "Could not symlink $target -> $Dest_dir/$dirlink:  $!\n";
        }
    }
}


# Push all #included files in $file onto our stack, except for STDIN
# and files we've already processed.
sub queue_includes_from
{
    my ($file)    = @_;
    my $line;

    return if ($file eq "-");

    open HEADER, $file or return;
        while (defined($line = <HEADER>)) {
            while (/\\$/) { # Handle continuation lines
                chop $line;
                $line .= <HEADER>;
            }

            if ($line =~ /^#\s*include\s+<(.*?)>/) {
                push(@ARGV, $1) unless $Is_converted{$1};
            }
        }
    close HEADER;
}


# Determine include directories; $Config{usrinc} should be enough for (all
# non-GCC?) C compilers, but gcc uses an additional include directory.
sub inc_dirs
{
    my $from_gcc    = `$Config{cc} -v 2>&1`;
    $from_gcc       =~ s:^Reading specs from (.*?)/specs\b.*:$1/include:s;

    length($from_gcc) ? ($from_gcc, $Config{usrinc}) : ($Config{usrinc});
}


# Create "_h2ph_pre.ph", if it doesn't exist or was built by a different
# version of h2ph.
sub build_preamble_if_necessary
{
    # Increment $VERSION every time this function is modified:
    my $VERSION     = 2;
    my $preamble    = "$Dest_dir/_h2ph_pre.ph";

    # Can we skip building the preamble file?
    if (-r $preamble) {
        # Extract version number from first line of preamble:
        open  PREAMBLE, $preamble or die "Cannot open $preamble:  $!";
            my $line = <PREAMBLE>;
            $line =~ /(\b\d+\b)/;
        close PREAMBLE            or die "Cannot close $preamble:  $!";

        # Don't build preamble if a compatible preamble exists:
        return if $1 == $VERSION;
    }

    my (%define) = _extract_cc_defines();

    open  PREAMBLE, ">$preamble" or die "Cannot open $preamble:  $!";
        print PREAMBLE "# This file was created by h2ph version $VERSION\n";

        foreach (sort keys %define) {
            if ($opt_D) {
                print PREAMBLE "# $_=$define{$_}\n";
            }

            if ($define{$_} =~ /^\d+$/) {
                print PREAMBLE
                    "unless (defined &$_) { sub $_() { $define{$_} } }\n\n";
            } elsif ($define{$_} =~ /^\w+$/) {
                print PREAMBLE
                    "unless (defined &$_) { sub $_() { &$define{$_} } }\n\n";
            } else {
                print PREAMBLE
                    "unless (defined &$_) { sub $_() { \"",
                    quotemeta($define{$_}), "\" } }\n\n";
            }
        }
    close PREAMBLE               or die "Cannot close $preamble:  $!";
}


# %Config contains information on macros that are pre-defined by the
# system's compiler.  We need this information to make the .ph files
# function with perl as the .h files do with cc.
sub _extract_cc_defines
{
    my %define;
    my $allsymbols  = join " ",
        @Config{'ccsymbols', 'cppsymbols', 'cppccsymbols'};

    # Split compiler pre-definitions into `key=value' pairs:
    foreach (split /\s+/, $allsymbols) {
        /(.+?)=(.+)/ and $define{$1} = $2;

        if ($opt_D) {
            print STDERR "$_:  $1 -> $2\n";
        }
    }

    return %define;
}


1;

##############################################################################
__END__

=head1 NAME

h2ph - convert .h C header files to .ph Perl header files

=head1 SYNOPSIS

B<h2ph [-d destination directory] [-r | -a] [-l] [headerfiles]>

=head1 DESCRIPTION

I<h2ph>
converts any C header files specified to the corresponding Perl header file
format.
It is most easily run while in /usr/include:

	cd /usr/include; h2ph * sys/*

or

	cd /usr/include; h2ph -r -l .

The output files are placed in the hierarchy rooted at Perl's
architecture dependent library directory.  You can specify a different
hierarchy with a B<-d> switch.

If run with no arguments, filters standard input to standard output.

=head1 OPTIONS

=over 4

=item -d destination_dir

Put the resulting B<.ph> files beneath B<destination_dir>, instead of
beneath the default Perl library location (C<$Config{'installarchlib'}>).

=item -r

Run recursively; if any of B<headerfiles> are directories, then run I<h2ph>
on all files in those directories (and their subdirectories, etc.).  B<-r>
and B<-a> are mutually exclusive.

=item -a

Run automagically; convert B<headerfiles>, as well as any B<.h> files
which they include.  This option will search for B<.h> files in all
directories which your C compiler ordinarily uses.  B<-a> and B<-r> are
mutually exclusive.

=item -l

Symbolic links will be replicated in the destination directory.  If B<-l>
is not specified, then links are skipped over.

=item -h

Put ``hints'' in the .ph files which will help in locating problems with
I<h2ph>.  In those cases when you B<require> a B<.ph> file containing syntax
errors, instead of the cryptic

	[ some error condition ] at (eval mmm) line nnn

you will see the slightly more helpful

	[ some error condition ] at filename.ph line nnn

However, the B<.ph> files almost double in size when built using B<-h>.

=item -D

Include the code from the B<.h> file as a comment in the B<.ph> file.
This is primarily used for debugging I<h2ph>.

=item -Q

``Quiet'' mode; don't print out the names of the files being converted.

=back

=head1 ENVIRONMENT

No environment variables are used.

=head1 FILES

 /usr/include/*.h
 /usr/include/sys/*.h

etc.

=head1 AUTHOR

Larry Wall

=head1 SEE ALSO

perl(1)

=head1 DIAGNOSTICS

The usual warnings if it can't read or write the files involved.

=head1 BUGS

Doesn't construct the %sizeof array for you.

It doesn't handle all C constructs, but it does attempt to isolate
definitions inside evals so that you can get at the definitions
that it can translate.

It's only intended as a rough tool.
You may need to dicker with the files produced.

You have to run this program by hand; it's not run as part of the Perl
installation.

Doesn't handle complicated expressions built piecemeal, a la:

    enum {
        FIRST_VALUE,
        SECOND_VALUE,
    #ifdef ABC
        THIRD_VALUE
    #endif
    };

Doesn't necessarily locate all of your C compiler's internally-defined
symbols.

=cut

!NO!SUBS!

close OUT or die "Can't close $file: $!";
chmod 0755, $file or die "Can't reset permissions for $file: $!\n";
exec("$Config{'eunicefix'} $file") if $Config{'eunicefix'} ne ':';
chdir $origdir;

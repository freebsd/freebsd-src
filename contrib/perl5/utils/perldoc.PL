#!/usr/local/bin/perl

use Config;
use File::Basename qw(&basename &dirname);
use Cwd;

# List explicitly here the variables you want Configure to
# generate.  Metaconfig only looks for shell variables, so you
# have to mention them as if they were shell variables, not
# %Config entries.  Thus you write
#  $startperl
# to ensure Configure will look for $Config{startperl}.

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
	if 0;

use strict;
my \@pagers = ();
push \@pagers, "$Config{'pager'}" if -x "$Config{'pager'}";
!GROK!THIS!

# In the following, perl variables are not expanded during extraction.

print OUT <<'!NO!SUBS!';

#
# Perldoc revision #1 -- look up a piece of documentation in .pod format that
# is embedded in the perl installation tree.
#
# This is not to be confused with Tom Christianson's perlman, which is a
# man replacement, written in perl. This perldoc is strictly for reading
# the perl manuals, though it too is written in perl.

if(@ARGV<1) {
	my $me = $0;		# Editing $0 is unportable
	$me =~ s,.*/,,;
	die <<EOF;
Usage: $me [-h] [-r] [-i] [-v] [-t] [-u] [-m] [-l] [-F] [-X] PageName|ModuleName|ProgramName
       $me -f PerlFunc
       $me -q FAQKeywords

The -h option prints more help.  Also try "perldoc perldoc" to get
aquainted with the system.
EOF
}

use Getopt::Std;
use Config '%Config';

my @global_found = ();
my $global_target = "";

my $Is_VMS = $^O eq 'VMS';
my $Is_MSWin32 = $^O eq 'MSWin32';
my $Is_Dos = $^O eq 'dos';

sub usage{
    warn "@_\n" if @_;
    # Erase evidence of previous errors (if any), so exit status is simple.
    $! = 0;
    die <<EOF;
perldoc [options] PageName|ModuleName|ProgramName...
perldoc [options] -f BuiltinFunction
perldoc [options] -q FAQRegex

Options:
    -h   Display this help message
    -r   Recursive search (slow)
    -i   Ignore case 
    -t   Display pod using pod2text instead of pod2man and nroff
             (-t is the default on win32)
    -u	 Display unformatted pod text
    -m   Display module's file in its entirety
    -l   Display the module's file name
    -F   Arguments are file names, not modules
    -v	 Verbosely describe what's going on
    -X	 use index if present (looks for pod.idx at $Config{archlib})
    -q   Search the text of questions (not answers) in perlfaq[1-9]

PageName|ModuleName...
         is the name of a piece of documentation that you want to look at. You 
         may either give a descriptive name of the page (as in the case of
         `perlfunc') the name of a module, either like `Term::Info', 
         `Term/Info', the partial name of a module, like `info', or 
         `makemaker', or the name of a program, like `perldoc'.

BuiltinFunction
         is the name of a perl function.  Will extract documentation from
         `perlfunc'.

FAQRegex
         is a regex. Will search perlfaq[1-9] for and extract any
         questions that match.

Any switches in the PERLDOC environment variable will be used before the 
command line arguments.  The optional pod index file contains a list of
filenames, one per line.

EOF
}

if( defined $ENV{"PERLDOC"} ) {
    require Text::ParseWords;
    unshift(@ARGV, Text::ParseWords::shellwords($ENV{"PERLDOC"}));
}
!NO!SUBS!

my $getopts = "mhtluvriFf:Xq:";
print OUT <<"!GET!OPTS!";

use vars qw( @{[map "\$opt_$_", ($getopts =~ /\w/g)]} );

getopts("$getopts") || usage;
!GET!OPTS!

print OUT <<'!NO!SUBS!';

usage if $opt_h;

my $podidx;
if( $opt_X ) {
    $podidx = "$Config{'archlib'}/pod.idx";
    $podidx = "" unless -f $podidx && -r _ && -M _ <= 7;
}

if( (my $opts = do{ local $^W; $opt_t + $opt_u + $opt_m + $opt_l }) > 1) {
    usage("only one of -t, -u, -m or -l")
} elsif ($Is_MSWin32 || $Is_Dos) {
    $opt_t = 1 unless $opts
}

if ($opt_t) { require Pod::Text; import Pod::Text; }

my @pages;
if ($opt_f) {
   @pages = ("perlfunc");
} elsif ($opt_q) {
   @pages = ("perlfaq1" .. "perlfaq9");
} else {
   @pages = @ARGV;
}

# Does this look like a module or extension directory?
if (-f "Makefile.PL") {
	# Add ., lib and blib/* libs to @INC (if they exist)
	unshift(@INC, '.');
	unshift(@INC, 'lib') if -d 'lib';
	require ExtUtils::testlib;
}



sub containspod {
    my($file, $readit) = @_;
    return 1 if !$readit && $file =~ /\.pod$/i;
    local($_);
    open(TEST,"<$file");
    while(<TEST>) {
	if(/^=head/) {
	    close(TEST);
	    return 1;
	}
    }
    close(TEST);
    return 0;
}

sub minus_f_nocase {
     my($dir,$file) = @_;
     my $path = join('/',$dir,$file);
     return $path if -f $path and -r _;
     if (!$opt_i or $Is_VMS or $Is_MSWin32 or $Is_Dos or $^O eq 'os2') {
        # on a case-forgiving file system or if case is important 
	# that is it all we can do
	warn "Ignored $path: unreadable\n" if -f _;
	return '';
     }
     local *DIR;
     local($")="/";
     my @p = ($dir);
     my($p,$cip);
     foreach $p (split(/\//, $file)){
	my $try = "@p/$p";
	stat $try;
 	if (-d _){
 	    push @p, $p;
	    if ( $p eq $global_target) {
		my $tmp_path = join ('/', @p);
		my $path_f = 0;
		for (@global_found) {
		    $path_f = 1 if $_ eq $tmp_path;
		}
		push (@global_found, $tmp_path) unless $path_f;
		print STDERR "Found as @p but directory\n" if $opt_v;
	    }
 	} elsif (-f _ && -r _) {
 	    return $try;
 	} elsif (-f _) {
	    warn "Ignored $try: unreadable\n";
 	} else {
 	    my $found=0;
 	    my $lcp = lc $p;
 	    opendir DIR, "@p";
 	    while ($cip=readdir(DIR)) {
 		if (lc $cip eq $lcp){
 		    $found++;
 		    last;
 		}
 	    }
 	    closedir DIR;
 	    return "" unless $found;
 	    push @p, $cip;
 	    return "@p" if -f "@p" and -r _;
	    warn "Ignored @p: unreadable\n" if -f _;
 	}
     }
     return "";
}
 

sub check_file {
    my($dir,$file) = @_;
    if ($opt_m) {
	return minus_f_nocase($dir,$file);
    } else {
	my $path = minus_f_nocase($dir,$file);
        return $path if length $path and containspod($path);
    }
    return "";
}


sub searchfor {
    my($recurse,$s,@dirs) = @_;
    $s =~ s!::!/!g;
    $s = VMS::Filespec::unixify($s) if $Is_VMS;
    return $s if -f $s && containspod($s);
    printf STDERR "Looking for $s in @dirs\n" if $opt_v;
    my $ret;
    my $i;
    my $dir;
    $global_target = (split('/', $s))[-1];
    for ($i=0; $i<@dirs; $i++) {
	$dir = $dirs[$i];
	($dir = VMS::Filespec::unixpath($dir)) =~ s!/$!! if $Is_VMS;
	if (       ( $ret = check_file $dir,"$s.pod")
		or ( $ret = check_file $dir,"$s.pm")
		or ( $ret = check_file $dir,$s)
		or ( $Is_VMS and
		     $ret = check_file $dir,"$s.com")
		or ( $^O eq 'os2' and 
		     $ret = check_file $dir,"$s.cmd")
		or ( ($Is_MSWin32 or $Is_Dos or $^O eq 'os2') and
		     $ret = check_file $dir,"$s.bat")
		or ( $ret = check_file "$dir/pod","$s.pod")
		or ( $ret = check_file "$dir/pod",$s)
	) {
	    return $ret;
	}
	
	if ($recurse) {
	    opendir(D,$dir);
	    my @newdirs = map "$dir/$_", grep {
		not /^\.\.?$/ and
		not /^auto$/  and   # save time! don't search auto dirs
		-d  "$dir/$_"
	    } readdir D;
	    closedir(D);
	    next unless @newdirs;
	    @newdirs = map((s/.dir$//,$_)[1],@newdirs) if $Is_VMS;
	    print STDERR "Also looking in @newdirs\n" if $opt_v;
	    push(@dirs,@newdirs);
	}
    }
    return ();
}

my @found;
foreach (@pages) {
        if ($podidx && open(PODIDX, $podidx)) {
	    my $searchfor = $_;
	    local($_);
	    $searchfor =~ s,::,/,g;
	    print STDERR "Searching for '$searchfor' in $podidx\n" if $opt_v;
	    while (<PODIDX>) {
		chomp;
		push(@found, $_) if m,/$searchfor(?:\.(?:pod|pm))?$,i;
	    }
	    close(PODIDX);
	    next;
        }
	print STDERR "Searching for $_\n" if $opt_v;
	# We must look both in @INC for library modules and in PATH
	# for executables, like h2xs or perldoc itself.
	my @searchdirs = @INC;
	if ($opt_F) {
	  next unless -r;
	  push @found, $_ if $opt_m or containspod($_);
	  next;
	}
	unless ($opt_m) { 
	    if ($Is_VMS) {
		my($i,$trn);
		for ($i = 0; $trn = $ENV{'DCL$PATH'.$i}; $i++) {
		    push(@searchdirs,$trn);
		}
		push(@searchdirs,'perl_root:[lib.pod]')  # installed pods
	    } else {
	        push(@searchdirs, grep(-d, split($Config{path_sep}, 
						 $ENV{'PATH'})));
	    }
	}
	my @files = searchfor(0,$_,@searchdirs);
	if( @files ) {
		print STDERR "Found as @files\n" if $opt_v;
	} else {
		# no match, try recursive search
		
		@searchdirs = grep(!/^\.$/,@INC);
		
		@files= searchfor(1,$_,@searchdirs) if $opt_r;
		if( @files ) {
			print STDERR "Loosely found as @files\n" if $opt_v;
		} else {
			print STDERR "No documentation found for \"$_\".\n";
			if (@global_found) {
			    print STDERR "However, try\n";
			    for my $dir (@global_found) {
				opendir(DIR, $dir) or die "$!";
				while (my $file = readdir(DIR)) {
				    next if ($file =~ /^\./);
				    $file =~ s/\.(pm|pod)$//;
				    print STDERR "\tperldoc $_\::$file\n";
				}
				closedir DIR;
			    }
			}
		}
	}
	push(@found,@files);
}

if(!@found) {
	exit ($Is_VMS ? 98962 : 1);
}

if ($opt_l) {
    print join("\n", @found), "\n";
    exit;
}

my $lines = $ENV{LINES} || 24;

my $no_tty;
if( ! -t STDOUT ) { $no_tty = 1 }

my $tmp;
if ($Is_MSWin32) {
	$tmp = "$ENV{TEMP}\\perldoc1.$$";
	push @pagers, qw( more< less notepad );
	unshift @pagers, $ENV{PAGER}  if $ENV{PAGER};
} elsif ($Is_VMS) {
	$tmp = 'Sys$Scratch:perldoc.tmp1_'.$$;
	push @pagers, qw( most more less type/page );
} elsif ($Is_Dos) {
	$tmp = "$ENV{TEMP}/perldoc1.$$";
	$tmp =~ tr!\\/!//!s;
	push @pagers, qw( less.exe more.com< );
	unshift @pagers, $ENV{PAGER}  if $ENV{PAGER};
} else {
	if ($^O eq 'os2') {
	  require POSIX;
	  $tmp = POSIX::tmpnam();
	  unshift @pagers, 'less', 'cmd /c more <';
	} else {
	  $tmp = "/tmp/perldoc1.$$";	  
	}
	push @pagers, qw( more less pg view cat );
	unshift @pagers, $ENV{PAGER}  if $ENV{PAGER};
}
unshift @pagers, $ENV{PERLDOC_PAGER} if $ENV{PERLDOC_PAGER};

if ($opt_m) {
	foreach my $pager (@pagers) {
		system("$pager @found") or exit;
	}
	if ($Is_VMS) { eval 'use vmsish qw(status exit); exit $?' }
	exit 1;
} 

if ($opt_f) {
   my $perlfunc = shift @found;
   open(PFUNC, $perlfunc) or die "Can't open $perlfunc: $!";

   # Functions like -r, -e, etc. are listed under `-X'.
   my $search_string = ($opt_f =~ /^-[rwxoRWXOeszfdlpSbctugkTBMAC]$/) ? 'I<-X' : $opt_f ;

   # Skip introduction
   while (<PFUNC>) {
       last if /^=head2 Alphabetical Listing of Perl Functions/;
   }

   # Look for our function
   my $found = 0;
   my @pod;
   while (<PFUNC>) {
       if (/^=item\s+\Q$search_string\E\b/o)  {
	   $found = 1;
       } elsif (/^=item/) {
	   last if $found > 1;
       }
       next unless $found;
       push @pod, $_;
       ++$found if /^\w/;	# found descriptive text
   }
   if (@pod) {
       if ($opt_t) {
	   open(FORMATTER, "| pod2text") || die "Can't start filter";
	   print FORMATTER "=over 8\n\n";
	   print FORMATTER @pod;
	   print FORMATTER "=back\n";
	   close(FORMATTER);
       } elsif (@pod < $lines-2) {
	   print @pod;
       } else {
	   foreach my $pager (@pagers) {
		open (PAGER, "| $pager") or next;
		print PAGER @pod ;
		close(PAGER) or next;
		last;
	   }
       }
   } else {
       die "No documentation for perl function `$opt_f' found\n";
   }
   exit;
}

if ($opt_q) {
   local @ARGV = @found;	# I'm lazy, sue me.
   my $found = 0;
   my %found_in;
   my @pod;

   while (<>) {
      if (/^=head2\s+.*(?:$opt_q)/oi) {
	 $found = 1;
	 push @pod, "=head1 Found in $ARGV\n\n" unless $found_in{$ARGV}++;
      } elsif (/^=head2/) {
	 $found = 0;
      }
      next unless $found;
      push @pod, $_;
   }
   
   if (@pod) {
      if ($opt_t) {
	 open(FORMATTER, "| pod2text") || die "Can't start filter";
	 print FORMATTER "=over 8\n\n";
	 print FORMATTER @pod;
	 print FORMATTER "=back\n";
	 close(FORMATTER);
      } elsif (@pod < $lines-2) {
	 print @pod;
      } else {
	 foreach my $pager (@pagers) {
	    open (PAGER, "| $pager") or next;
	    print PAGER @pod ;
	    close(PAGER) or next;
	    last;
	 }
      }
   } else {
      die "No documentation for perl FAQ keyword `$opt_q' found\n";
   }
   exit;
}

foreach (@found) {

	my $err;
	if($opt_t) {
		open(TMP,">>$tmp");
		Pod::Text::pod2text($_,*TMP);
		close(TMP);
	} elsif(not $opt_u) {
		my $cmd = "pod2man --lax $_ | nroff -man";
		$cmd .= " | col -x" if $^O =~ /hpux/;
		my $rslt = `$cmd`;
		unless(($err = $?)) {
			open(TMP,">>$tmp");
			print TMP $rslt;
			close TMP;
		}
	}
	                                                
	if( $opt_u or $err or -z $tmp) {
		open(OUT,">>$tmp");
		open(IN,"<$_");
		my $cut = 1;
		while (<IN>) {
			$cut = $1 eq 'cut' if /^=(\w+)/;
			next if $cut;
			print OUT;
		}
		close(IN);
		close(OUT);
	}
}

if( $no_tty ) {
	open(TMP,"<$tmp");
	print while <TMP>;
	close(TMP);
} else {
	foreach my $pager (@pagers) {
		system("$pager $tmp") or last;
	}
}

1 while unlink($tmp); #Possibly pointless VMSism

exit 0;

__END__

=head1 NAME

perldoc - Look up Perl documentation in pod format.

=head1 SYNOPSIS

B<perldoc> [B<-h>] [B<-v>] [B<-t>] [B<-u>] [B<-m>] [B<-l>] [B<-F>]  [B<-X>] PageName|ModuleName|ProgramName

B<perldoc> B<-f> BuiltinFunction

B<perldoc> B<-q> FAQ Keyword

=head1 DESCRIPTION

I<perldoc> looks up a piece of documentation in .pod format that is embedded
in the perl installation tree or in a perl script, and displays it via
C<pod2man | nroff -man | $PAGER>. (In addition, if running under HP-UX,
C<col -x> will be used.) This is primarily used for the documentation for
the perl library modules.

Your system may also have man pages installed for those modules, in
which case you can probably just use the man(1) command.

=head1 OPTIONS

=over 5

=item B<-h> help

Prints out a brief help message.

=item B<-v> verbose

Describes search for the item in detail.

=item B<-t> text output

Display docs using plain text converter, instead of nroff. This may be faster,
but it won't look as nice.

=item B<-u> unformatted

Find docs only; skip reformatting by pod2*

=item B<-m> module

Display the entire module: both code and unformatted pod documentation.
This may be useful if the docs don't explain a function in the detail
you need, and you'd like to inspect the code directly; perldoc will find
the file for you and simply hand it off for display.

=item B<-l> file name only

Display the file name of the module found.

=item B<-F> file names

Consider arguments as file names, no search in directories will be performed.

=item B<-f> perlfunc

The B<-f> option followed by the name of a perl built in function will
extract the documentation of this function from L<perlfunc>.

=item B<-q> perlfaq

The B<-q> option takes a regular expression as an argument.  It will search
the question headings in perlfaq[1-9] and print the entries matching
the regular expression.

=item B<-X> use an index if present

The B<-X> option looks for a entry whose basename matches the name given on the
command line in the file C<$Config{archlib}/pod.idx>.  The pod.idx file should
contain fully qualified filenames, one per line.

=item B<PageName|ModuleName|ProgramName>

The item you want to look up.  Nested modules (such as C<File::Basename>)
are specified either as C<File::Basename> or C<File/Basename>.  You may also
give a descriptive name of a page, such as C<perlfunc>. You make also give a
partial or wrong-case name, such as "basename" for "File::Basename", but
this will be slower, if there is more then one page with the same partial
name, you will only get the first one.

=back

=head1 ENVIRONMENT

Any switches in the C<PERLDOC> environment variable will be used before the 
command line arguments.  C<perldoc> also searches directories
specified by the C<PERL5LIB> (or C<PERLLIB> if C<PERL5LIB> is not
defined) and C<PATH> environment variables.
(The latter is so that embedded pods for executables, such as
C<perldoc> itself, are available.)  C<perldoc> will use, in order of
preference, the pager defined in C<PERLDOC_PAGER>, C<MANPAGER>, or
C<PAGER> before trying to find a pager on its own.  (C<MANPAGER> is not
used if C<perldoc> was told to display plain text or unformatted pod.)

=head1 AUTHOR

Kenneth Albanowski <kjahds@kjahds.com>

Minor updates by Andy Dougherty <doughera@lafcol.lafayette.edu>

=cut

#
# Version 1.14: Wed Jul 15 01:50:20 EST 1998
#       Robin Barker <rmb1@cise.npl.co.uk>
#	-strict, -w cleanups
# Version 1.13: Fri Feb 27 16:20:50 EST 1997
#       Gurusamy Sarathy <gsar@umich.edu>
#	-doc tweaks for -F and -X options
# Version 1.12: Sat Apr 12 22:41:09 EST 1997
#       Gurusamy Sarathy <gsar@umich.edu>
#	-various fixes for win32
# Version 1.11: Tue Dec 26 09:54:33 EST 1995
#       Kenneth Albanowski <kjahds@kjahds.com>
#   -added Charles Bailey's further VMS patches, and -u switch
#   -added -t switch, with pod2text support
# 
# Version 1.10: Thu Nov  9 07:23:47 EST 1995
#		Kenneth Albanowski <kjahds@kjahds.com>
#	-added VMS support
#	-added better error recognition (on no found pages, just exit. On
#	 missing nroff/pod2man, just display raw pod.)
#	-added recursive/case-insensitive matching (thanks, Andreas). This
#	 slows things down a bit, unfortunately. Give a precise name, and
#	 it'll run faster.
#
# Version 1.01:	Tue May 30 14:47:34 EDT 1995
#		Andy Dougherty  <doughera@lafcol.lafayette.edu>
#   -added pod documentation.
#   -added PATH searching.
#   -added searching pod/ subdirectory (mainly to pick up perlfunc.pod
#    and friends.
#
#
# TODO:
#
#	Cache directories read during sloppy match
!NO!SUBS!

close OUT or die "Can't close $file: $!";
chmod 0755, $file or die "Can't reset permissions for $file: $!\n";
exec("$Config{'eunicefix'} $file") if $Config{'eunicefix'} ne ':';
chdir $origdir;

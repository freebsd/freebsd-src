case $CONFIGDOTSH in
'')
	if test -f config.sh; then TOP=.;
	elif test -f ../config.sh; then TOP=..;
	elif test -f ../../config.sh; then TOP=../..;
	elif test -f ../../../config.sh; then TOP=../../..;
	elif test -f ../../../../config.sh; then TOP=../../../..;
	else
		echo "Can't find config.sh."; exit 1
	fi
	. $TOP/config.sh
	;;
esac
: This forces SH files to create target in same directory as SH file.
: This is so that make depend always knows where to find SH derivatives.
case "$0" in
*/*) cd `expr X$0 : 'X\(.*\)/'` ;;
esac
echo "Extracting makeaperl (with variable substitutions)"
rm -f makeaperl
$spitshell >makeaperl <<!GROK!THIS!
$startperl
    eval 'exec $perlpath -S \$0 \${1+"\$@"}'
	if \$running_under_some_shell;
!GROK!THIS!

$spitshell >>makeaperl <<'!NO!SUBS!'

=head1 NAME

makeaperl - create a new perl binary from static extensions

=head1 SYNOPSIS

C<makeaperl -l library -m makefile -o target -t tempdir [object_files] [static_extensions] [search_directories]>

=head1 DESCRIPTION

This utility is designed to build new perl binaries from existing
extensions on the fly. Called without any arguments it produces a new
binary with the name C<perl> in the current directory. Intermediate
files are produced in C</tmp>, if that is writeable, else in the
current directory. The most important intermediate file is a Makefile,
that is used internally to call C<make>. The new perl binary will consist

The C<-l> switch lets you specify the name of a perl library to be
linked into the new binary. If you do not specify a library, makeaperl
writes targets for any C<libperl*.a> it finds in the search path. The
topmost target will be the one related to C<libperl.a>.

With the C<-m> switch you can provide a name for the Makefile that
will be written (default C</tmp/Makefile.$$>). Likewise specifies the
C<-o> switch a name for the perl binary (default C<perl>). The C<-t>
switch lets you determine, in which directory the intermediate files
should be stored.

All object files and static extensions following on the command line
will be linked into the target file. If there are any directories
specified on the command line, these directories are searched for
C<*.a> files, and all of the found ones will be linked in, too. If
there is no directory named, then the contents of $INC[0] are
searched.

If the command fails, there is currently no other mechanism to adjust
the behaviour of the program than to alter the generated Makefile and
run C<make> by hand.

=head1 AUTHORS
Tim Bunce <Tim.Bunce@ig.co.uk>, Andreas Koenig
<koenig@franz.ww.TU-Berlin.DE>; 

=head2 STATUS
First version, written 5 Feb 1995, is considered alpha.

=cut

use ExtUtils::MakeMaker;
use Getopt::Long;
use strict qw(subs refs);

$Version = 1.0;
$Verbose = 0;

sub usage{
    warn <<END;
$0 version $Version

$0: [options] [object_files] [static_extensions ...] [directories to search through]
 -l perllibrary     perl library to link from (the first libperl.a found)
 -m makefilename    name of the makefile to be written (/tmp/Makefile.\$\$)
 -o name            name for perl executable (perl)
 -t directory       directory where intermediate files reside (/tmp)
END
    exit 1;
}

if (-w "/tmp") {
    $opt_t = "/tmp";
} else {
    $opt_t = ".";
}
$opt_l = '';
$opt_m = "$opt_t/Makefile.$$";
$opt_o = 'perl';

$Getopt::Long::ignorecase=0;

GetOptions('t=s', 'l=s', 'm=s', 'o=s') || die &usage;

@dirs = grep -d $_, @ARGV;
@fils = grep -f $_, @ARGV;

@dirs = $INC[0] unless @dirs;

open MAKE, ">$opt_m";
MM->init_main();
MM->init_others();
print MAKE MM->makeaperl('MAKE'    => $opt_m,
			 'TARGET'  => $opt_o,
			 'TMP'     => $opt_t,
			 'LIBPERL' => $opt_l,
			 'DIRS'    => [@dirs], 
			 'STAT'    => [@fils], 
			 'INCL'    => [@dirs]
);
close MAKE;
(system "make -f $opt_m") == 0 or die "$0 failed: Please check file $opt_m and run make -f $opt_m\n";
!NO!SUBS!
chmod 755 makeaperl
$eunicefix makeaperl

package TestPodIncPlainText;

BEGIN {
   use File::Basename;
   use File::Spec;
   use Cwd qw(abs_path);
   push @INC, '..';
   my $THISDIR = abs_path(dirname $0);
   unshift @INC, $THISDIR;
   require "testcmp.pl";
   import TestCompare;
   my $PARENTDIR = dirname $THISDIR;
   push @INC, map { File::Spec->catfile($_, 'lib') } ($PARENTDIR, $THISDIR);
}

#use strict;
#use diagnostics;
use Carp;
use Exporter;
#use File::Compare;
#use Cwd qw(abs_path);

use vars qw($MYPKG @EXPORT @ISA);
$MYPKG = eval { (caller)[0] };
@EXPORT = qw(&testpodplaintext);
BEGIN {
    if ( $] >= 5.005_58 ) {
       require Pod::Text;
       @ISA = qw( Pod::Text );
    }
    else {
       require Pod::PlainText;
       @ISA = qw( Pod::PlainText );
    }
    require VMS::Filespec if $^O eq 'VMS';
}

## Hardcode settings for TERMCAP and COLUMNS so we can try to get
## reproducible results between environments
@ENV{qw(TERMCAP COLUMNS)} = ('co=76:do=^J', 76);

sub catfile(@) { File::Spec->catfile(@_); }

my $INSTDIR = abs_path(dirname $0);
if ($^O eq 'VMS') { # clean up directory spec
    $INSTDIR = VMS::Filespec::unixpath($INSTDIR);
    $INSTDIR =~ s#/$##;
    $INSTDIR =~ s#/000000/#/#;
}
$INSTDIR = (dirname $INSTDIR) if (basename($INSTDIR) eq 'pod');
$INSTDIR = (dirname $INSTDIR) if (basename($INSTDIR) eq 't');
my @PODINCDIRS = ( catfile($INSTDIR, 'lib', 'Pod'),
                   catfile($INSTDIR, 'scripts'),
                   catfile($INSTDIR, 'pod'),
                   catfile($INSTDIR, 't', 'pod')
                 );
print "PODINCDIRS = ",join(', ',@PODINCDIRS),"\n";

## Find the path to the file to =include
sub findinclude {
    my $self    = shift;
    my $incname = shift;

    ## See if its already found w/out any "searching;
    return  $incname if (-r $incname);

    ## Need to search for it. Look in the following directories ...
    ##   1. the directory containing this pod file
    my $thispoddir = dirname $self->input_file;
    ##   2. the parent directory of the above
    my $parentdir  = dirname $thispoddir;
    my @podincdirs = ($thispoddir, $parentdir, @PODINCDIRS);

    for (@podincdirs) {
       my $incfile = catfile($_, $incname);
       return $incfile  if (-r $incfile);
    }
    warn("*** Can't find =include file $incname in @podincdirs\n");
    return "";
}

sub command {
    my $self = shift;
    my ($cmd, $text, $line_num, $pod_para)  = @_;
    $cmd     = ''  unless (defined $cmd);
    local $_ = $text || '';
    my $out_fh  = $self->output_handle;

    ## Defer to the superclass for everything except '=include'
    return  $self->SUPER::command(@_) unless ($cmd eq "include");

    ## We have an '=include' command
    my $incdebug = 1; ## debugging
    my @incargs = split;
    if (@incargs == 0) {
        warn("*** No filename given for '=include'\n");
        return;
    }
    my $incfile  = $self->findinclude(shift @incargs)  or  return;
    my $incbase  = basename $incfile;
    print $out_fh "###### begin =include $incbase #####\n"  if ($incdebug);
    $self->parse_from_file( {-cutting => 1}, $incfile );
    print $out_fh "###### end =include $incbase #####\n"    if ($incdebug);
}

sub begin_input {
   $_[0]->{_INFILE} = VMS::Filespec::unixify($_[0]->{_INFILE}) if $^O eq 'VMS';
}

sub podinc2plaintext( $ $ ) {
    my ($infile, $outfile) = @_;
    local $_;
    my $text_parser = $MYPKG->new(quotes => "`'");
    $text_parser->parse_from_file($infile, $outfile);
}

sub testpodinc2plaintext( @ ) {
   my %args = @_;
   my $infile  = $args{'-In'}  || croak "No input file given!";
   my $outfile = $args{'-Out'} || croak "No output file given!";
   my $cmpfile = $args{'-Cmp'} || croak "No compare-result file given!";

   my $different = '';
   my $testname = basename $cmpfile, '.t', '.xr';

   unless (-e $cmpfile) {
      my $msg = "*** Can't find comparison file $cmpfile for testing $infile";
      warn  "$msg\n";
      return  $msg;
   }

   print "# Running testpodinc2plaintext for '$testname'...\n";
   ## Compare the output against the expected result
   podinc2plaintext($infile, $outfile);
   if ( testcmp($outfile, $cmpfile) ) {
       $different = "$outfile is different from $cmpfile";
   }
   else {
       unlink($outfile);
   }
   return  $different;
}

sub testpodplaintext( @ ) {
   my %opts = (ref $_[0] eq 'HASH') ? %{shift()} : ();
   my @testpods = @_;
   my ($testname, $testdir) = ("", "");
   my ($podfile, $cmpfile) = ("", "");
   my ($outfile, $errfile) = ("", "");
   my $passes = 0;
   my $failed = 0;
   local $_;

   print "1..", scalar @testpods, "\n"  unless ($opts{'-xrgen'});

   for $podfile (@testpods) {
      ($testname, $_) = fileparse($podfile);
      $testdir ||=  $_;
      $testname  =~ s/\.t$//;
      $cmpfile   =  $testdir . $testname . '.xr';
      $outfile   =  $testdir . $testname . '.OUT';

      if ($opts{'-xrgen'}) {
          if ($opts{'-force'} or ! -e $cmpfile) {
             ## Create the comparison file
             print "# Creating expected result for \"$testname\"" .
                   " pod2plaintext test ...\n";
             podinc2plaintext($podfile, $cmpfile);
          }
          else {
             print "# File $cmpfile already exists" .
                   " (use '-force' to regenerate it).\n";
          }
          next;
      }

      my $failmsg = testpodinc2plaintext
                        -In  => $podfile,
                        -Out => $outfile,
                        -Cmp => $cmpfile;
      if ($failmsg) {
          ++$failed;
          print "#\tFAILED. ($failmsg)\n";
	  print "not ok ", $failed+$passes, "\n";
      }
      else {
          ++$passes;
          unlink($outfile);
          print "#\tPASSED.\n";
	  print "ok ", $failed+$passes, "\n";
      }
   }
   return  $passes;
}

1;

package TestCompare;

use vars qw(@ISA @EXPORT $MYPKG);
#use strict;
#use diagnostics;
use Carp;
use Exporter;
use File::Basename;
use File::Spec;
use FileHandle;

@ISA = qw(Exporter);
@EXPORT = qw(&testcmp);
$MYPKG = eval { (caller)[0] };

##--------------------------------------------------------------------------

=head1 NAME

testcmp -- compare two files line-by-line

=head1 SYNOPSIS

    $is_diff = testcmp($file1, $file2);

or

    $is_diff = testcmp({-cmplines => \&mycmp}, $file1, $file2);

=head2 DESCRIPTION

Compare two text files line-by-line and return 0 if they are the
same, 1 if they differ. Each of $file1 and $file2 may be a filenames,
or a filehandles (in which case it must already be open for reading).

If the first argument is a hashref, then the B<-cmplines> key in the
hash may have a subroutine reference as its corresponding value.
The referenced user-defined subroutine should be a line-comparator
function that takes two pre-chomped text-lines as its arguments
(the first is from $file1 and the second is from $file2). It should
return 0 if it considers the two lines equivalent, and non-zero
otherwise.

=cut

##--------------------------------------------------------------------------

sub testcmp( $ $ ; $) {
   my %opts = ref($_[0]) eq 'HASH' ? %{shift()} : ();
   my ($file1, $file2) = @_;
   my ($fh1, $fh2) = ($file1, $file2);
   unless (ref $fh1) {
      $fh1 = FileHandle->new($file1, "r") or die "Can't open $file1: $!";
   }
   unless (ref $fh2) {
      $fh2 = FileHandle->new($file2, "r") or die "Can't open $file2: $!";
   }
  
   my $cmplines = $opts{'-cmplines'} || undef;
   my ($f1text, $f2text) = ("", "");
   my ($line, $diffs)    = (0, 0);
  
   while ( defined($f1text) and defined($f2text) ) {
      defined($f1text = <$fh1>)  and  chomp($f1text);
      defined($f2text = <$fh2>)  and  chomp($f2text);
      ++$line;
      last unless ( defined($f1text) and defined($f2text) );
      $diffs = (ref $cmplines) ? &$cmplines($f1text, $f2text)
                               : ($f1text ne $f2text);
      last if $diffs;
   }
   close($fh1) unless (ref $file1);
   close($fh2) unless (ref $file2);
  
   $diffs = 1  if (defined($f1text) or defined($f2text));
   if ( defined($f1text) and defined($f2text) ) {
      ## these two lines must be different
      warn "$file1 and $file2 differ at line $line\n";
   }
   elsif (defined($f1text)  and  (! defined($f1text))) {
      ## file1 must be shorter
      warn "$file1 is shorter than $file2\n";
   }
   elsif (defined $f2text) {
      ## file2 must be longer
      warn "$file1 is shorter than $file2\n";
   }
   return $diffs;
}

1;

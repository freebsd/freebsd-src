package ExtUtils::Command;

use 5.005_64;
use strict;
# use AutoLoader;
use Carp;
use File::Copy;
use File::Compare;
use File::Basename;
use File::Path qw(rmtree);
require Exporter;
our(@ISA, @EXPORT, $VERSION);
@ISA     = qw(Exporter);
@EXPORT  = qw(cp rm_f rm_rf mv cat eqtime mkpath touch test_f);
$VERSION = '1.01';

=head1 NAME

ExtUtils::Command - utilities to replace common UNIX commands in Makefiles etc.

=head1 SYNOPSIS

  perl -MExtUtils::Command -e cat files... > destination
  perl -MExtUtils::Command -e mv source... destination
  perl -MExtUtils::Command -e cp source... destination
  perl -MExtUtils::Command -e touch files...
  perl -MExtUtils::Command -e rm_f file...
  perl -MExtUtils::Command -e rm_rf directories...
  perl -MExtUtils::Command -e mkpath directories...
  perl -MExtUtils::Command -e eqtime source destination
  perl -MExtUtils::Command -e chmod mode files...
  perl -MExtUtils::Command -e test_f file

=head1 DESCRIPTION

The module is used in the Win32 port to replace common UNIX commands.
Most commands are wrappers on generic modules File::Path and File::Basename.

=over 4

=cut

sub expand_wildcards
{
 @ARGV = map(/[\*\?]/ ? glob($_) : $_,@ARGV);
}

=item cat 

Concatenates all files mentioned on command line to STDOUT.

=cut 

sub cat ()
{
 expand_wildcards();
 print while (<>);
}

=item eqtime src dst

Sets modified time of dst to that of src

=cut 

sub eqtime
{
 my ($src,$dst) = @ARGV;
 open(F,">$dst");
 close(F);
 utime((stat($src))[8,9],$dst);
}

=item rm_f files....

Removes directories - recursively (even if readonly)

=cut 

sub rm_rf
{
 rmtree([grep -e $_,expand_wildcards()],0,0);
}

=item rm_f files....

Removes files (even if readonly)

=cut 

sub rm_f
{
 foreach (expand_wildcards())
  {
   next unless -f $_;        
   next if unlink($_);
   chmod(0777,$_);           
   next if unlink($_);
   carp "Cannot delete $_:$!";
  }
}

=item touch files ...

Makes files exist, with current timestamp 

=cut 

sub touch
{
 expand_wildcards();
 my $t    = time;
 while (@ARGV)
  {
   my $file = shift(@ARGV);               
   open(FILE,">>$file") || die "Cannot write $file:$!";
   close(FILE);
   utime($t,$t,$file);
  }
}

=item mv source... destination

Moves source to destination.
Multiple sources are allowed if destination is an existing directory.

=cut 

sub mv
{
 my $dst = pop(@ARGV);
 expand_wildcards();
 croak("Too many arguments") if (@ARGV > 1 && ! -d $dst);
 while (@ARGV)
  {
   my $src = shift(@ARGV);               
   move($src,$dst);
  }
}

=item cp source... destination

Copies source to destination.
Multiple sources are allowed if destination is an existing directory.

=cut 

sub cp
{
 my $dst = pop(@ARGV);
 expand_wildcards();
 croak("Too many arguments") if (@ARGV > 1 && ! -d $dst);
 while (@ARGV)
  {
   my $src = shift(@ARGV);               
   copy($src,$dst);
  }
}

=item chmod mode files...

Sets UNIX like permissions 'mode' on all the files.

=cut 

sub chmod
{
 my $mode = shift(@ARGV);
 chmod($mode,expand_wildcards()) || die "Cannot chmod ".join(' ',$mode,@ARGV).":$!";
}

=item mkpath directory...

Creates directory, including any parent directories.

=cut 

sub mkpath
{
 File::Path::mkpath([expand_wildcards()],0,0777);
}

=item test_f file

Tests if a file exists

=cut 

sub test_f
{
 exit !-f shift(@ARGV);
}


1;
__END__ 

=back

=head1 BUGS

Should probably be Auto/Self loaded.

=head1 SEE ALSO 

ExtUtils::MakeMaker, ExtUtils::MM_Unix, ExtUtils::MM_Win32

=head1 AUTHOR

Nick Ing-Simmons <F<nick@ni-s.u-net.com>>.

=cut


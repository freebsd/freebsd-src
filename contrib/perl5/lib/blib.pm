package blib;

=head1 NAME

blib - Use MakeMaker's uninstalled version of a package

=head1 SYNOPSIS

 perl -Mblib script [args...]

 perl -Mblib=dir script [args...]

=head1 DESCRIPTION

Looks for MakeMaker-like I<'blib'> directory structure starting in 
I<dir> (or current directory) and working back up to five levels of '..'.

Intended for use on command line with B<-M> option as a way of testing
arbitary scripts against an uninstalled version of a package.

However it is possible to : 

 use blib; 
 or 
 use blib '..';

etc. if you really must.

=head1 BUGS

Pollutes global name space for development only task.

=head1 AUTHOR

Nick Ing-Simmons nik@tiuk.ti.com

=cut 

use Cwd;

use vars qw($VERSION);
$VERSION = '1.00';

sub import
{
 my $package = shift;
 my $dir = getcwd;
 if ($^O eq 'VMS') { ($dir = VMS::Filespec::unixify($dir)) =~ s-/$--; }
 if (@_)
  {
   $dir = shift;
   $dir =~ s/blib$//;
   $dir =~ s,/+$,,;
   $dir = '.' unless ($dir);
   die "$dir is not a directory\n" unless (-d $dir);
  }
 my $i   = 5;
 while ($i--)
  {
   my $blib = "${dir}/blib";
   if (-d $blib && -d "$blib/arch" && -d "$blib/lib")
    {
     unshift(@INC,"$blib/arch","$blib/lib");
     warn "Using $blib\n";
     return;
    }
   $dir .= "/..";
  }
 die "Cannot find blib even in $dir\n";
}

1;

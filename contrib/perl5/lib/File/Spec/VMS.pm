
package File::Spec::VMS;

use Carp qw( &carp );
use Config;
require Exporter;
use VMS::Filespec;
use File::Basename;

use File::Spec;
use vars qw($Revision);
$Revision = '5.3901 (6-Mar-1997)';

@ISA = qw(File::Spec::Unix);

Exporter::import('File::Spec', '$Verbose');

=head1 NAME

File::Spec::VMS - methods for VMS file specs

=head1 SYNOPSIS

 use File::Spec::VMS; # Done internally by File::Spec if needed

=head1 DESCRIPTION

See File::Spec::Unix for a documentation of the methods provided
there. This package overrides the implementation of these methods, not
the semantics.

=head2 Methods always loaded

=over

=item catdir

Concatenates a list of file specifications, and returns the result as a
VMS-syntax directory specification.

=cut

sub catdir {
    my($self,@dirs) = @_;
    my($dir) = pop @dirs;
    @dirs = grep($_,@dirs);
    my($rslt);
    if (@dirs) {
      my($path) = (@dirs == 1 ? $dirs[0] : $self->catdir(@dirs));
      my($spath,$sdir) = ($path,$dir);
      $spath =~ s/.dir$//; $sdir =~ s/.dir$//; 
      $sdir = $self->eliminate_macros($sdir) unless $sdir =~ /^[\w\-]+$/;
      $rslt = $self->fixpath($self->eliminate_macros($spath)."/$sdir",1);
    }
    else { 
      if ($dir =~ /^\$\([^\)]+\)$/) { $rslt = $dir; }
      else                          { $rslt = vmspath($dir); }
    }
    print "catdir(",join(',',@_[1..$#_]),") = |$rslt|\n" if $Verbose >= 3;
    $rslt;
}

=item catfile

Concatenates a list of file specifications, and returns the result as a
VMS-syntax directory specification.

=cut

sub catfile {
    my($self,@files) = @_;
    my($file) = pop @files;
    @files = grep($_,@files);
    my($rslt);
    if (@files) {
      my($path) = (@files == 1 ? $files[0] : $self->catdir(@files));
      my($spath) = $path;
      $spath =~ s/.dir$//;
      if ( $spath =~ /^[^\)\]\/:>]+\)$/ && basename($file) eq $file) { $rslt = "$spath$file"; }
      else {
          $rslt = $self->eliminate_macros($spath);
          $rslt = vmsify($rslt.($rslt ? '/' : '').unixify($file));
      }
    }
    else { $rslt = vmsify($file); }
    print "catfile(",join(',',@_[1..$#_]),") = |$rslt|\n" if $Verbose >= 3;
    $rslt;
}

=item curdir (override)

Returns a string representing of the current directory.

=cut

sub curdir {
    return '[]';
}

=item rootdir (override)

Returns a string representing of the root directory.

=cut

sub rootdir {
    return '';
}

=item updir (override)

Returns a string representing of the parent directory.

=cut

sub updir {
    return '[-]';
}

=item path (override)

Translate logical name DCL$PATH as a searchlist, rather than trying
to C<split> string value of C<$ENV{'PATH'}>.

=cut

sub path {
    my(@dirs,$dir,$i);
    while ($dir = $ENV{'DCL$PATH;' . $i++}) { push(@dirs,$dir); }
    @dirs;
}

=item file_name_is_absolute (override)

Checks for VMS directory spec as well as Unix separators.

=cut

sub file_name_is_absolute {
    my($self,$file) = @_;
    # If it's a logical name, expand it.
    $file = $ENV{$file} while $file =~ /^[\w\$\-]+$/ and $ENV{$file};
    $file =~ m!^/! or $file =~ m![<\[][^.\-\]>]! or $file =~ /:[^<\[]/;
}

1;
__END__


package File::Spec::Unix;

use Exporter ();
use Config;
use File::Basename qw(basename dirname fileparse);
use DirHandle;
use strict;
use vars qw(@ISA $Is_Mac $Is_OS2 $Is_VMS $Is_Win32);
use File::Spec;

Exporter::import('File::Spec', '$Verbose');

$Is_OS2 = $^O eq 'os2';
$Is_Mac = $^O eq 'MacOS';
$Is_Win32 = $^O eq 'MSWin32';

if ($Is_VMS = $^O eq 'VMS') {
    require VMS::Filespec;
    import VMS::Filespec qw( &vmsify );
}

=head1 NAME

File::Spec::Unix - methods used by File::Spec

=head1 SYNOPSIS

C<require File::Spec::Unix;>

=head1 DESCRIPTION

Methods for manipulating file specifications.

=head1 METHODS

=over 2

=item canonpath

No physical check on the filesystem, but a logical cleanup of a
path. On UNIX eliminated successive slashes and successive "/.".

=cut

sub canonpath {
    my($self,$path) = @_;
    $path =~ s|/+|/|g ;                            # xx////xx  -> xx/xx
    $path =~ s|(/\.)+/|/|g ;                       # xx/././xx -> xx/xx
    $path =~ s|^(\./)+|| unless $path eq "./";     # ./xx      -> xx
    $path =~ s|/$|| unless $path eq "/";           # xx/       -> xx
    $path;
}

=item catdir

Concatenate two or more directory names to form a complete path ending
with a directory. But remove the trailing slash from the resulting
string, because it doesn't look good, isn't necessary and confuses
OS2. Of course, if this is the root directory, don't cut off the
trailing slash :-)

=cut

# ';

sub catdir {
    shift;
    my @args = @_;
    for (@args) {
	# append a slash to each argument unless it has one there
	$_ .= "/" if $_ eq '' or substr($_,-1) ne "/";
    }
    my $result = join('', @args);
    # remove a trailing slash unless we are root
    substr($result,-1) = ""
	if length($result) > 1 && substr($result,-1) eq "/";
    $result;
}

=item catfile

Concatenate one or more directory names and a filename to form a
complete path ending with a filename

=cut

sub catfile {
    my $self = shift @_;
    my $file = pop @_;
    return $file unless @_;
    my $dir = $self->catdir(@_);
    for ($dir) {
	$_ .= "/" unless substr($_,length($_)-1,1) eq "/";
    }
    return $dir.$file;
}

=item curdir

Returns a string representing of the current directory.  "." on UNIX.

=cut

sub curdir {
    return "." ;
}

=item rootdir

Returns a string representing of the root directory.  "/" on UNIX.

=cut

sub rootdir {
    return "/";
}

=item updir

Returns a string representing of the parent directory.  ".." on UNIX.

=cut

sub updir {
    return "..";
}

=item no_upwards

Given a list of file names, strip out those that refer to a parent
directory. (Does not strip symlinks, only '.', '..', and equivalents.)

=cut

sub no_upwards {
    my($self) = shift;
    return grep(!/^\.{1,2}$/, @_);
}

=item file_name_is_absolute

Takes as argument a path and returns true, if it is an absolute path.

=cut

sub file_name_is_absolute {
    my($self,$file) = @_;
    $file =~ m:^/: ;
}

=item path

Takes no argument, returns the environment variable PATH as an array.

=cut

sub path {
    my($self) = @_;
    my $path_sep = ":";
    my $path = $ENV{PATH};
    my @path = split $path_sep, $path;
    foreach(@path) { $_ = '.' if $_ eq '' }
    @path;
}

=item join

join is the same as catfile.

=cut

sub join {
	my($self) = shift @_;
	$self->catfile(@_);
}

=item nativename

TBW.

=cut

sub nativename {
	my($self,$name) = shift @_;
	$name;
}

=back

=head1 SEE ALSO

L<File::Spec>

=cut

1;
__END__

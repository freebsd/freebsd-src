package File::Spec::Unix;

use strict;
use vars qw($VERSION);

$VERSION = '1.2';

use Cwd;

=head1 NAME

File::Spec::Unix - methods used by File::Spec

=head1 SYNOPSIS

 require File::Spec::Unix; # Done automatically by File::Spec

=head1 DESCRIPTION

Methods for manipulating file specifications.

=head1 METHODS

=over 2

=item canonpath

No physical check on the filesystem, but a logical cleanup of a
path. On UNIX eliminated successive slashes and successive "/.".

    $cpath = File::Spec->canonpath( $path ) ;

=cut

sub canonpath {
    my ($self,$path) = @_;
    $path =~ s|/+|/|g unless($^O eq 'cygwin');     # xx////xx  -> xx/xx
    $path =~ s|(/\.)+/|/|g;                        # xx/././xx -> xx/xx
    $path =~ s|^(\./)+||s unless $path eq "./";    # ./xx      -> xx
    $path =~ s|^/(\.\./)+|/|s;                     # /../../xx -> xx
    $path =~ s|/\Z(?!\n)|| unless $path eq "/";          # xx/       -> xx
    return $path;
}

=item catdir

Concatenate two or more directory names to form a complete path ending
with a directory. But remove the trailing slash from the resulting
string, because it doesn't look good, isn't necessary and confuses
OS2. Of course, if this is the root directory, don't cut off the
trailing slash :-)

=cut

sub catdir {
    my $self = shift;
    my @args = @_;
    foreach (@args) {
	# append a slash to each argument unless it has one there
	$_ .= "/" if $_ eq '' || substr($_,-1) ne "/";
    }
    return $self->canonpath(join('', @args));
}

=item catfile

Concatenate one or more directory names and a filename to form a
complete path ending with a filename

=cut

sub catfile {
    my $self = shift;
    my $file = pop @_;
    return $file unless @_;
    my $dir = $self->catdir(@_);
    $dir .= "/" unless substr($dir,-1) eq "/";
    return $dir.$file;
}

=item curdir

Returns a string representation of the current directory.  "." on UNIX.

=cut

sub curdir {
    return ".";
}

=item devnull

Returns a string representation of the null device. "/dev/null" on UNIX.

=cut

sub devnull {
    return "/dev/null";
}

=item rootdir

Returns a string representation of the root directory.  "/" on UNIX.

=cut

sub rootdir {
    return "/";
}

=item tmpdir

Returns a string representation of the first writable directory
from the following list or "" if none are writable:

    $ENV{TMPDIR}
    /tmp

=cut

my $tmpdir;
sub tmpdir {
    return $tmpdir if defined $tmpdir;
    foreach ($ENV{TMPDIR}, "/tmp") {
	next unless defined && -d && -w _;
	$tmpdir = $_;
	last;
    }
    $tmpdir = '' unless defined $tmpdir;
    return $tmpdir;
}

=item updir

Returns a string representation of the parent directory.  ".." on UNIX.

=cut

sub updir {
    return "..";
}

=item no_upwards

Given a list of file names, strip out those that refer to a parent
directory. (Does not strip symlinks, only '.', '..', and equivalents.)

=cut

sub no_upwards {
    my $self = shift;
    return grep(!/^\.{1,2}\Z(?!\n)/s, @_);
}

=item case_tolerant

Returns a true or false value indicating, respectively, that alphabetic
is not or is significant when comparing file specifications.

=cut

sub case_tolerant {
    return 0;
}

=item file_name_is_absolute

Takes as argument a path and returns true if it is an absolute path.

This does not consult the local filesystem on Unix, Win32, or OS/2.  It
does sometimes on MacOS (see L<File::Spec::MacOS/file_name_is_absolute>).
It does consult the working environment for VMS (see
L<File::Spec::VMS/file_name_is_absolute>).

=cut

sub file_name_is_absolute {
    my ($self,$file) = @_;
    return scalar($file =~ m:^/:s);
}

=item path

Takes no argument, returns the environment variable PATH as an array.

=cut

sub path {
    my @path = split(':', $ENV{PATH});
    foreach (@path) { $_ = '.' if $_ eq '' }
    return @path;
}

=item join

join is the same as catfile.

=cut

sub join {
    my $self = shift;
    return $self->catfile(@_);
}

=item splitpath

    ($volume,$directories,$file) = File::Spec->splitpath( $path );
    ($volume,$directories,$file) = File::Spec->splitpath( $path, $no_file );

Splits a path in to volume, directory, and filename portions. On systems
with no concept of volume, returns undef for volume. 

For systems with no syntax differentiating filenames from directories, 
assumes that the last file is a path unless $no_file is true or a 
trailing separator or /. or /.. is present. On Unix this means that $no_file
true makes this return ( '', $path, '' ).

The directory portion may or may not be returned with a trailing '/'.

The results can be passed to L</catpath()> to get back a path equivalent to
(usually identical to) the original path.

=cut

sub splitpath {
    my ($self,$path, $nofile) = @_;

    my ($volume,$directory,$file) = ('','','');

    if ( $nofile ) {
        $directory = $path;
    }
    else {
        $path =~ m|^ ( (?: .* / (?: \.\.?\Z(?!\n) )? )? ) ([^/]*) |xs;
        $directory = $1;
        $file      = $2;
    }

    return ($volume,$directory,$file);
}


=item splitdir

The opposite of L</catdir()>.

    @dirs = File::Spec->splitdir( $directories );

$directories must be only the directory portion of the path on systems 
that have the concept of a volume or that have path syntax that differentiates
files from directories.

Unlike just splitting the directories on the separator, empty
directory names (C<''>) can be returned, because these are significant
on some OSs (e.g. MacOS).

On Unix,

    File::Spec->splitdir( "/a/b//c/" );

Yields:

    ( '', 'a', 'b', '', 'c', '' )

=cut

sub splitdir {
    my ($self,$directories) = @_ ;
    #
    # split() likes to forget about trailing null fields, so here we
    # check to be sure that there will not be any before handling the
    # simple case.
    #
    if ( $directories !~ m|/\Z(?!\n)| ) {
        return split( m|/|, $directories );
    }
    else {
        #
        # since there was a trailing separator, add a file name to the end, 
        # then do the split, then replace it with ''.
        #
        my( @directories )= split( m|/|, "${directories}dummy" ) ;
        $directories[ $#directories ]= '' ;
        return @directories ;
    }
}


=item catpath

Takes volume, directory and file portions and returns an entire path. Under
Unix, $volume is ignored, and directory and file are catenated.  A '/' is
inserted if need be.  On other OSs, $volume is significant.

=cut

sub catpath {
    my ($self,$volume,$directory,$file) = @_;

    if ( $directory ne ''                && 
         $file ne ''                     && 
         substr( $directory, -1 ) ne '/' && 
         substr( $file, 0, 1 ) ne '/' 
    ) {
        $directory .= "/$file" ;
    }
    else {
        $directory .= $file ;
    }

    return $directory ;
}

=item abs2rel

Takes a destination path and an optional base path returns a relative path
from the base path to the destination path:

    $rel_path = File::Spec->abs2rel( $path ) ;
    $rel_path = File::Spec->abs2rel( $path, $base ) ;

If $base is not present or '', then L<cwd()> is used. If $base is relative, 
then it is converted to absolute form using L</rel2abs()>. This means that it
is taken to be relative to L<cwd()>.

On systems with the concept of a volume, this assumes that both paths 
are on the $destination volume, and ignores the $base volume. 

On systems that have a grammar that indicates filenames, this ignores the 
$base filename as well. Otherwise all path components are assumed to be
directories.

If $path is relative, it is converted to absolute form using L</rel2abs()>.
This means that it is taken to be relative to L<cwd()>.

No checks against the filesystem are made on most systems.  On MacOS,
the filesystem may be consulted (see
L<File::Spec::MacOS/file_name_is_absolute>).  On VMS, there is
interaction with the working environment, as logicals and
macros are expanded.

Based on code written by Shigio Yamaguchi.

=cut

sub abs2rel {
    my($self,$path,$base) = @_;

    # Clean up $path
    if ( ! $self->file_name_is_absolute( $path ) ) {
        $path = $self->rel2abs( $path ) ;
    }
    else {
        $path = $self->canonpath( $path ) ;
    }

    # Figure out the effective $base and clean it up.
    if ( !defined( $base ) || $base eq '' ) {
        $base = cwd() ;
    }
    elsif ( ! $self->file_name_is_absolute( $base ) ) {
        $base = $self->rel2abs( $base ) ;
    }
    else {
        $base = $self->canonpath( $base ) ;
    }

    # Now, remove all leading components that are the same
    my @pathchunks = $self->splitdir( $path);
    my @basechunks = $self->splitdir( $base);

    while (@pathchunks && @basechunks && $pathchunks[0] eq $basechunks[0]) {
        shift @pathchunks ;
        shift @basechunks ;
    }

    $path = CORE::join( '/', @pathchunks );
    $base = CORE::join( '/', @basechunks );

    # $base now contains the directories the resulting relative path 
    # must ascend out of before it can descend to $path_directory.  So, 
    # replace all names with $parentDir
    $base =~ s|[^/]+|..|g ;

    # Glue the two together, using a separator if necessary, and preventing an
    # empty result.
    if ( $path ne '' && $base ne '' ) {
        $path = "$base/$path" ;
    } else {
        $path = "$base$path" ;
    }

    return $self->canonpath( $path ) ;
}

=item rel2abs

Converts a relative path to an absolute path. 

    $abs_path = File::Spec->rel2abs( $path ) ;
    $abs_path = File::Spec->rel2abs( $path, $base ) ;

If $base is not present or '', then L<cwd()> is used. If $base is relative, 
then it is converted to absolute form using L</rel2abs()>. This means that it
is taken to be relative to L<cwd()>.

On systems with the concept of a volume, this assumes that both paths 
are on the $base volume, and ignores the $path volume. 

On systems that have a grammar that indicates filenames, this ignores the 
$base filename as well. Otherwise all path components are assumed to be
directories.

If $path is absolute, it is cleaned up and returned using L</canonpath()>.

No checks against the filesystem are made on most systems.  On MacOS,
the filesystem may be consulted (see
L<File::Spec::MacOS/file_name_is_absolute>).  On VMS, there is
interaction with the working environment, as logicals and
macros are expanded.

Based on code written by Shigio Yamaguchi.

=cut

sub rel2abs {
    my ($self,$path,$base ) = @_;

    # Clean up $path
    if ( ! $self->file_name_is_absolute( $path ) ) {
        # Figure out the effective $base and clean it up.
        if ( !defined( $base ) || $base eq '' ) {
            $base = cwd() ;
        }
        elsif ( ! $self->file_name_is_absolute( $base ) ) {
            $base = $self->rel2abs( $base ) ;
        }
        else {
            $base = $self->canonpath( $base ) ;
        }

        # Glom them together
        $path = $self->catdir( $base, $path ) ;
    }

    return $self->canonpath( $path ) ;
}


=back

=head1 SEE ALSO

L<File::Spec>

=cut

1;

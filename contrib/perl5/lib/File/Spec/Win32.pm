package File::Spec::Win32;

use strict;
use Cwd;
use vars qw(@ISA $VERSION);
require File::Spec::Unix;

$VERSION = '1.2';

@ISA = qw(File::Spec::Unix);

=head1 NAME

File::Spec::Win32 - methods for Win32 file specs

=head1 SYNOPSIS

 require File::Spec::Win32; # Done internally by File::Spec if needed

=head1 DESCRIPTION

See File::Spec::Unix for a documentation of the methods provided
there. This package overrides the implementation of these methods, not
the semantics.

=over

=item devnull

Returns a string representation of the null device.

=cut

sub devnull {
    return "nul";
}

=item tmpdir

Returns a string representation of the first existing directory
from the following list:

    $ENV{TMPDIR}
    $ENV{TEMP}
    $ENV{TMP}
    C:/temp
    /tmp
    /

=cut

my $tmpdir;
sub tmpdir {
    return $tmpdir if defined $tmpdir;
    my $self = shift;
    foreach (@ENV{qw(TMPDIR TEMP TMP)}, qw(C:/temp /tmp /)) {
	next unless defined && -d;
	$tmpdir = $_;
	last;
    }
    $tmpdir = '' unless defined $tmpdir;
    $tmpdir = $self->canonpath($tmpdir);
    return $tmpdir;
}

sub case_tolerant {
    return 1;
}

sub file_name_is_absolute {
    my ($self,$file) = @_;
    return scalar($file =~ m{^([a-z]:)?[\\/]}is);
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
    $dir .= "\\" unless substr($dir,-1) eq "\\";
    return $dir.$file;
}

sub path {
    my $path = $ENV{'PATH'} || $ENV{'Path'} || $ENV{'path'};
    my @path = split(';',$path);
    foreach (@path) { $_ = '.' if $_ eq '' }
    return @path;
}

=item canonpath

No physical check on the filesystem, but a logical cleanup of a
path. On UNIX eliminated successive slashes and successive "/.".

=cut

sub canonpath {
    my ($self,$path) = @_;
    $path =~ s/^([a-z]:)/\u$1/s;
    $path =~ s|/|\\|g;
    $path =~ s|([^\\])\\+|$1\\|g;                  # xx////xx  -> xx/xx
    $path =~ s|(\\\.)+\\|\\|g;                     # xx/././xx -> xx/xx
    $path =~ s|^(\.\\)+||s unless $path eq ".\\";  # ./xx      -> xx
    $path =~ s|\\\Z(?!\n)||
             unless $path =~ m#^([A-Z]:)?\\\Z(?!\n)#s;   # xx/       -> xx
    return $path;
}

=item splitpath

    ($volume,$directories,$file) = File::Spec->splitpath( $path );
    ($volume,$directories,$file) = File::Spec->splitpath( $path, $no_file );

Splits a path in to volume, directory, and filename portions. Assumes that 
the last file is a path unless the path ends in '\\', '\\.', '\\..'
or $no_file is true.  On Win32 this means that $no_file true makes this return 
( $volume, $path, undef ).

Separators accepted are \ and /.

Volumes can be drive letters or UNC sharenames (\\server\share).

The results can be passed to L</catpath> to get back a path equivalent to
(usually identical to) the original path.

=cut

sub splitpath {
    my ($self,$path, $nofile) = @_;
    my ($volume,$directory,$file) = ('','','');
    if ( $nofile ) {
        $path =~ 
            m{^( (?:[a-zA-Z]:|(?:\\\\|//)[^\\/]+[\\/][^\\/]+)? ) 
                 (.*)
             }xs;
        $volume    = $1;
        $directory = $2;
    }
    else {
        $path =~ 
            m{^ ( (?: [a-zA-Z]: |
                      (?:\\\\|//)[^\\/]+[\\/][^\\/]+
                  )?
                )
                ( (?:.*[\\\\/](?:\.\.?\Z(?!\n))?)? )
                (.*)
             }xs;
        $volume    = $1;
        $directory = $2;
        $file      = $3;
    }

    return ($volume,$directory,$file);
}


=item splitdir

The opposite of L</catdir()>.

    @dirs = File::Spec->splitdir( $directories );

$directories must be only the directory portion of the path on systems 
that have the concept of a volume or that have path syntax that differentiates
files from directories.

Unlike just splitting the directories on the separator, leading empty and 
trailing directory entries can be returned, because these are significant
on some OSs. So,

    File::Spec->splitdir( "/a/b/c" );

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
    if ( $directories !~ m|[\\/]\Z(?!\n)| ) {
        return split( m|[\\/]|, $directories );
    }
    else {
        #
        # since there was a trailing separator, add a file name to the end, 
        # then do the split, then replace it with ''.
        #
        my( @directories )= split( m|[\\/]|, "${directories}dummy" ) ;
        $directories[ $#directories ]= '' ;
        return @directories ;
    }
}


=item catpath

Takes volume, directory and file portions and returns an entire path. Under
Unix, $volume is ignored, and this is just like catfile(). On other OSs,
the $volume become significant.

=cut

sub catpath {
    my ($self,$volume,$directory,$file) = @_;

    # If it's UNC, make sure the glue separator is there, reusing
    # whatever separator is first in the $volume
    $volume .= $1
        if ( $volume =~ m@^([\\/])[\\/][^\\/]+[\\/][^\\/]+\Z(?!\n)@s &&
             $directory =~ m@^[^\\/]@s
           ) ;

    $volume .= $directory ;

    # If the volume is not just A:, make sure the glue separator is 
    # there, reusing whatever separator is first in the $volume if possible.
    if ( $volume !~ m@^[a-zA-Z]:\Z(?!\n)@s &&
         $volume =~ m@[^\\/]\Z(?!\n)@      &&
         $file   =~ m@[^\\/]@
       ) {
        $volume =~ m@([\\/])@ ;
        my $sep = $1 ? $1 : '\\' ;
        $volume .= $sep ;
    }

    $volume .= $file ;

    return $volume ;
}


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
    if ( ! $self->file_name_is_absolute( $base ) ) {
        $base = $self->rel2abs( $base ) ;
    }
    elsif ( !defined( $base ) || $base eq '' ) {
        $base = cwd() ;
    }
    else {
        $base = $self->canonpath( $base ) ;
    }

    # Split up paths
    my ( $path_volume, $path_directories, $path_file ) =
        $self->splitpath( $path, 1 ) ;

    my $base_directories = ($self->splitpath( $base, 1 ))[1] ;

    # Now, remove all leading components that are the same
    my @pathchunks = $self->splitdir( $path_directories );
    my @basechunks = $self->splitdir( $base_directories );

    while ( @pathchunks && 
            @basechunks && 
            lc( $pathchunks[0] ) eq lc( $basechunks[0] ) 
          ) {
        shift @pathchunks ;
        shift @basechunks ;
    }

    # No need to catdir, we know these are well formed.
    $path_directories = CORE::join( '\\', @pathchunks );
    $base_directories = CORE::join( '\\', @basechunks );

    # $base_directories now contains the directories the resulting relative
    # path must ascend out of before it can descend to $path_directory.  So, 
    # replace all names with $parentDir

    #FA Need to replace between backslashes...
    $base_directories =~ s|[^\\]+|..|g ;

    # Glue the two together, using a separator if necessary, and preventing an
    # empty result.

    #FA Must check that new directories are not empty.
    if ( $path_directories ne '' && $base_directories ne '' ) {
        $path_directories = "$base_directories\\$path_directories" ;
    } else {
        $path_directories = "$base_directories$path_directories" ;
    }

    # It makes no sense to add a relative path to a UNC volume
    $path_volume = '' unless $path_volume =~ m{^[A-Z]:}is ;

    return $self->canonpath( 
        $self->catpath($path_volume, $path_directories, $path_file ) 
    ) ;
}


sub rel2abs {
    my ($self,$path,$base ) = @_;

    if ( ! $self->file_name_is_absolute( $path ) ) {

        if ( !defined( $base ) || $base eq '' ) {
            $base = cwd() ;
        }
        elsif ( ! $self->file_name_is_absolute( $base ) ) {
            $base = $self->rel2abs( $base ) ;
        }
        else {
            $base = $self->canonpath( $base ) ;
        }

        my ( $path_directories, $path_file ) =
            ($self->splitpath( $path, 1 ))[1,2] ;

        my ( $base_volume, $base_directories ) =
            $self->splitpath( $base, 1 ) ;

        $path = $self->catpath( 
            $base_volume, 
            $self->catdir( $base_directories, $path_directories ), 
            $path_file
        ) ;
    }

    return $self->canonpath( $path ) ;
}

=back

=head1 SEE ALSO

L<File::Spec>

=cut

1;

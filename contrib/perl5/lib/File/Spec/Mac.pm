package File::Spec::Mac;

use strict;
use vars qw(@ISA $VERSION);
require File::Spec::Unix;

$VERSION = '1.2';

@ISA = qw(File::Spec::Unix);

=head1 NAME

File::Spec::Mac - File::Spec for MacOS

=head1 SYNOPSIS

 require File::Spec::Mac; # Done internally by File::Spec if needed

=head1 DESCRIPTION

Methods for manipulating file specifications.

=head1 METHODS

=over 2

=item canonpath

On MacOS, there's nothing to be done.  Returns what it's given.

=cut

sub canonpath {
    my ($self,$path) = @_;
    return $path;
}

=item catdir

Concatenate two or more directory names to form a complete path ending with 
a directory.  Put a trailing : on the end of the complete path if there 
isn't one, because that's what's done in MacPerl's environment.

The fundamental requirement of this routine is that

	  File::Spec->catdir(split(":",$path)) eq $path

But because of the nature of Macintosh paths, some additional 
possibilities are allowed to make using this routine give reasonable results 
for some common situations.  Here are the rules that are used.  Each 
argument has its trailing ":" removed.  Each argument, except the first,
has its leading ":" removed.  They are then joined together by a ":".

So

	  File::Spec->catdir("a","b") = "a:b:"
	  File::Spec->catdir("a:",":b") = "a:b:"
	  File::Spec->catdir("a:","b") = "a:b:"
	  File::Spec->catdir("a",":b") = "a:b"
	  File::Spec->catdir("a","","b") = "a::b"

etc.

To get a relative path (one beginning with :), begin the first argument with :
or put a "" as the first argument.

If you don't want to worry about these rules, never allow a ":" on the ends 
of any of the arguments except at the beginning of the first.

Under MacPerl, there is an additional ambiguity.  Does the user intend that

	  File::Spec->catfile("LWP","Protocol","http.pm")

be relative or absolute?  There's no way of telling except by checking for the
existence of LWP: or :LWP, and even there he may mean a dismounted volume or
a relative path in a different directory (like in @INC).   So those checks
aren't done here. This routine will treat this as absolute.

=cut

sub catdir {
    shift;
    my @args = @_;
    my $result = shift @args;
    $result =~ s/:\Z(?!\n)//;
    foreach (@args) {
	s/:\Z(?!\n)//;
	s/^://s;
	$result .= ":$_";
    }
    return "$result:";
}

=item catfile

Concatenate one or more directory names and a filename to form a
complete path ending with a filename.  Since this uses catdir, the
same caveats apply.  Note that the leading : is removed from the filename,
so that 

	  File::Spec->catfile($ENV{HOME},"file");

and

	  File::Spec->catfile($ENV{HOME},":file");

give the same answer, as one might expect.

=cut

sub catfile {
    my $self = shift;
    my $file = pop @_;
    return $file unless @_;
    my $dir = $self->catdir(@_);
    $file =~ s/^://s;
    return $dir.$file;
}

=item curdir

Returns a string representing the current directory.

=cut

sub curdir {
    return ":";
}

=item devnull

Returns a string representing the null device.

=cut

sub devnull {
    return "Dev:Null";
}

=item rootdir

Returns a string representing the root directory.  Under MacPerl,
returns the name of the startup volume, since that's the closest in
concept, although other volumes aren't rooted there.

=cut

sub rootdir {
#
#  There's no real root directory on MacOS.  The name of the startup
#  volume is returned, since that's the closest in concept.
#
    require Mac::Files;
    my $system =  Mac::Files::FindFolder(&Mac::Files::kOnSystemDisk,
					 &Mac::Files::kSystemFolderType);
    $system =~ s/:.*\Z(?!\n)/:/s;
    return $system;
}

=item tmpdir

Returns a string representation of the first existing directory
from the following list or '' if none exist:

    $ENV{TMPDIR}

=cut

my $tmpdir;
sub tmpdir {
    return $tmpdir if defined $tmpdir;
    $tmpdir = $ENV{TMPDIR} if -d $ENV{TMPDIR};
    $tmpdir = '' unless defined $tmpdir;
    return $tmpdir;
}

=item updir

Returns a string representing the parent directory.

=cut

sub updir {
    return "::";
}

=item file_name_is_absolute

Takes as argument a path and returns true, if it is an absolute path.  In 
the case where a name can be either relative or absolute (for example, a 
folder named "HD" in the current working directory on a drive named "HD"), 
relative wins.  Use ":" in the appropriate place in the path if you want to
distinguish unambiguously.

As a special case, the file name '' is always considered to be absolute.

=cut

sub file_name_is_absolute {
    my ($self,$file) = @_;
    if ($file =~ /:/) {
	return ($file !~ m/^:/s);
    } elsif ( $file eq '' ) {
        return 1 ;
    } else {
	return (! -e ":$file");
    }
}

=item path

Returns the null list for the MacPerl application, since the concept is 
usually meaningless under MacOS. But if you're using the MacPerl tool under 
MPW, it gives back $ENV{Commands} suitably split, as is done in 
:lib:ExtUtils:MM_Mac.pm.

=cut

sub path {
#
#  The concept is meaningless under the MacPerl application.
#  Under MPW, it has a meaning.
#
    return unless exists $ENV{Commands};
    return split(/,/, $ENV{Commands});
}

=item splitpath

=cut

sub splitpath {
    my ($self,$path, $nofile) = @_;

    my ($volume,$directory,$file) = ('','','');

    if ( $nofile ) {
        ( $volume, $directory ) = $path =~ m@((?:[^:]+(?::|\Z(?!\n)))?)(.*)@s;
    }
    else {
        $path =~ 
            m@^( (?: [^:]+: )? ) 
                ( (?: .*: )? )
                ( .* )
             @xs;
        $volume    = $1;
        $directory = $2;
        $file      = $3;
    }

    # Make sure non-empty volumes and directories end in ':'
    $volume    .= ':' if $volume    =~ m@[^:]\Z(?!\n)@ ;
    $directory .= ':' if $directory =~ m@[^:]\Z(?!\n)@ ;
    return ($volume,$directory,$file);
}


=item splitdir

=cut

sub splitdir {
    my ($self,$directories) = @_ ;
    #
    # split() likes to forget about trailing null fields, so here we
    # check to be sure that there will not be any before handling the
    # simple case.
    #
    if ( $directories !~ m@:\Z(?!\n)@ ) {
        return split( m@:@, $directories );
    }
    else {
        #
        # since there was a trailing separator, add a file name to the end, 
        # then do the split, then replace it with ''.
        #
        my( @directories )= split( m@:@, "${directories}dummy" ) ;
        $directories[ $#directories ]= '' ;
        return @directories ;
    }
}


=item catpath

=cut

sub catpath {
    my $self = shift ;

    my $result = shift ;
    $result =~ s@^([^/])@/$1@s ;

    my $segment ;
    for $segment ( @_ ) {
        if ( $result =~ m@[^/]\Z(?!\n)@ && $segment =~ m@^[^/]@s ) {
            $result .= "/$segment" ;
        }
        elsif ( $result =~ m@/\Z(?!\n)@ && $segment =~ m@^/@s ) {
            $result  =~ s@/+\Z(?!\n)@/@;
            $segment =~ s@^/+@@s;
            $result  .= "$segment" ;
        }
        else {
            $result  .= $segment ;
        }
    }

    return $result ;
}

=item abs2rel

See L<File::Spec::Unix/abs2rel> for general documentation.

Unlike C<File::Spec::Unix->abs2rel()>, this function will make
checks against the local filesystem if necessary.  See
L</file_name_is_absolute> for details.

=cut

sub abs2rel {
    my($self,$path,$base) = @_;

    # Clean up $path
    if ( ! $self->file_name_is_absolute( $path ) ) {
        $path = $self->rel2abs( $path ) ;
    }

    # Figure out the effective $base and clean it up.
    if ( !defined( $base ) || $base eq '' ) {
        $base = cwd() ;
    }
    elsif ( ! $self->file_name_is_absolute( $base ) ) {
        $base = $self->rel2abs( $base ) ;
    }

    # Now, remove all leading components that are the same
    my @pathchunks = $self->splitdir( $path );
    my @basechunks = $self->splitdir( $base );

    while (@pathchunks && @basechunks && $pathchunks[0] eq $basechunks[0]) {
        shift @pathchunks ;
        shift @basechunks ;
    }

    $path = join( ':', @pathchunks );

    # @basechunks now contains the number of directories to climb out of.
    $base = ':' x @basechunks ;

    return "$base:$path" ;
}

=item rel2abs

See L<File::Spec::Unix/rel2abs> for general documentation.

Unlike C<File::Spec::Unix->rel2abs()>, this function will make
checks against the local filesystem if necessary.  See
L</file_name_is_absolute> for details.

=cut

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

        $path = $self->canonpath("$base$path") ;
    }

    return $path ;
}


=back

=head1 SEE ALSO

L<File::Spec>

=cut

1;

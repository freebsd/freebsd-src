package File::Spec::VMS;

use strict;
use vars qw(@ISA $VERSION);
require File::Spec::Unix;

$VERSION = '1.1';

@ISA = qw(File::Spec::Unix);

use Cwd;
use File::Basename;
use VMS::Filespec;

=head1 NAME

File::Spec::VMS - methods for VMS file specs

=head1 SYNOPSIS

 require File::Spec::VMS; # Done internally by File::Spec if needed

=head1 DESCRIPTION

See File::Spec::Unix for a documentation of the methods provided
there. This package overrides the implementation of these methods, not
the semantics.

=over

=item eliminate_macros

Expands MM[KS]/Make macros in a text string, using the contents of
identically named elements of C<%$self>, and returns the result
as a file specification in Unix syntax.

=cut

sub eliminate_macros {
    my($self,$path) = @_;
    return '' unless $path;
    $self = {} unless ref $self;

    if ($path =~ /\s/) {
      return join ' ', map { $self->eliminate_macros($_) } split /\s+/, $path;
    }

    my($npath) = unixify($path);
    my($complex) = 0;
    my($head,$macro,$tail);

    # perform m##g in scalar context so it acts as an iterator
    while ($npath =~ m#(.*?)\$\((\S+?)\)(.*)#gs) { 
        if ($self->{$2}) {
            ($head,$macro,$tail) = ($1,$2,$3);
            if (ref $self->{$macro}) {
                if (ref $self->{$macro} eq 'ARRAY') {
                    $macro = join ' ', @{$self->{$macro}};
                }
                else {
                    print "Note: can't expand macro \$($macro) containing ",ref($self->{$macro}),
                          "\n\t(using MMK-specific deferred substitutuon; MMS will break)\n";
                    $macro = "\cB$macro\cB";
                    $complex = 1;
                }
            }
            else { ($macro = unixify($self->{$macro})) =~ s#/\Z(?!\n)##; }
            $npath = "$head$macro$tail";
        }
    }
    if ($complex) { $npath =~ s#\cB(.*?)\cB#\${$1}#gs; }
    $npath;
}

=item fixpath

Catchall routine to clean up problem MM[SK]/Make macros.  Expands macros
in any directory specification, in order to avoid juxtaposing two
VMS-syntax directories when MM[SK] is run.  Also expands expressions which
are all macro, so that we can tell how long the expansion is, and avoid
overrunning DCL's command buffer when MM[KS] is running.

If optional second argument has a TRUE value, then the return string is
a VMS-syntax directory specification, if it is FALSE, the return string
is a VMS-syntax file specification, and if it is not specified, fixpath()
checks to see whether it matches the name of a directory in the current
default directory, and returns a directory or file specification accordingly.

=cut

sub fixpath {
    my($self,$path,$force_path) = @_;
    return '' unless $path;
    $self = bless {} unless ref $self;
    my($fixedpath,$prefix,$name);

    if ($path =~ /\s/) {
      return join ' ',
             map { $self->fixpath($_,$force_path) }
	     split /\s+/, $path;
    }

    if ($path =~ m#^\$\([^\)]+\)\Z(?!\n)#s || $path =~ m#[/:>\]]#) { 
        if ($force_path or $path =~ /(?:DIR\)|\])\Z(?!\n)/) {
            $fixedpath = vmspath($self->eliminate_macros($path));
        }
        else {
            $fixedpath = vmsify($self->eliminate_macros($path));
        }
    }
    elsif ((($prefix,$name) = ($path =~ m#^\$\(([^\)]+)\)(.+)#s)) && $self->{$prefix}) {
        my($vmspre) = $self->eliminate_macros("\$($prefix)");
        # is it a dir or just a name?
        $vmspre = ($vmspre =~ m|/| or $prefix =~ /DIR\Z(?!\n)/) ? vmspath($vmspre) : '';
        $fixedpath = ($vmspre ? $vmspre : $self->{$prefix}) . $name;
        $fixedpath = vmspath($fixedpath) if $force_path;
    }
    else {
        $fixedpath = $path;
        $fixedpath = vmspath($fixedpath) if $force_path;
    }
    # No hints, so we try to guess
    if (!defined($force_path) and $fixedpath !~ /[:>(.\]]/) {
        $fixedpath = vmspath($fixedpath) if -d $fixedpath;
    }

    # Trim off root dirname if it's had other dirs inserted in front of it.
    $fixedpath =~ s/\.000000([\]>])/$1/;
    # Special case for VMS absolute directory specs: these will have had device
    # prepended during trip through Unix syntax in eliminate_macros(), since
    # Unix syntax has no way to express "absolute from the top of this device's
    # directory tree".
    if ($path =~ /^[\[>][^.\-]/) { $fixedpath =~ s/^[^\[<]+//; }
    $fixedpath;
}

=back

=head2 Methods always loaded

=over

=item canonpath (override)

Removes redundant portions of file specifications according to VMS syntax.

=cut

sub canonpath {
    my($self,$path) = @_;

    if ($path =~ m|/|) { # Fake Unix
      my $pathify = $path =~ m|/\Z(?!\n)|;
      $path = $self->SUPER::canonpath($path);
      if ($pathify) { return vmspath($path); }
      else          { return vmsify($path);  }
    }
    else {
      $path =~ s-\]\[--g;  $path =~ s/><//g;            # foo.][bar       ==> foo.bar
      $path =~ s/([\[<])000000\./$1/;                   # [000000.foo     ==> foo
      1 while $path =~ s{([\[<-])\.-}{$1-};             # [.-.-           ==> [--
      $path =~ s/\.[^\[<\.]+\.-([\]\>])/$1/;            # bar.foo.-]      ==> bar]
      $path =~ s/([\[<])(-+)/$1 . "\cx" x length($2)/e; # encode leading '-'s
      $path =~ s/([\[<\.])([^\[<\.\cx]+)\.-\.?/$1/g;    # bar.-.foo       ==> foo
      $path =~ s/([\[<])(\cx+)/$1 . '-' x length($2)/e; # then decode
      return $path;
    }
}

=item catdir

Concatenates a list of file specifications, and returns the result as a
VMS-syntax directory specification.  No check is made for "impossible"
cases (e.g. elements other than the first being absolute filespecs).

=cut

sub catdir {
    my ($self,@dirs) = @_;
    my $dir = pop @dirs;
    @dirs = grep($_,@dirs);
    my $rslt;
    if (@dirs) {
	my $path = (@dirs == 1 ? $dirs[0] : $self->catdir(@dirs));
	my ($spath,$sdir) = ($path,$dir);
	$spath =~ s/\.dir\Z(?!\n)//; $sdir =~ s/\.dir\Z(?!\n)//; 
	$sdir = $self->eliminate_macros($sdir) unless $sdir =~ /^[\w\-]+\Z(?!\n)/s;
	$rslt = $self->fixpath($self->eliminate_macros($spath)."/$sdir",1);

	# Special case for VMS absolute directory specs: these will have had device
	# prepended during trip through Unix syntax in eliminate_macros(), since
	# Unix syntax has no way to express "absolute from the top of this device's
	# directory tree".
	if ($spath =~ /^[\[<][^.\-]/s) { $rslt =~ s/^[^\[<]+//s; }
    }
    else {
	if    (not defined $dir or not length $dir) { $rslt = ''; }
	elsif ($dir =~ /^\$\([^\)]+\)\Z(?!\n)/s)          { $rslt = $dir; }
	else                                        { $rslt = vmspath($dir); }
    }
    return $self->canonpath($rslt);
}

=item catfile

Concatenates a list of file specifications, and returns the result as a
VMS-syntax file specification.

=cut

sub catfile {
    my ($self,@files) = @_;
    my $file = pop @files;
    @files = grep($_,@files);
    my $rslt;
    if (@files) {
	my $path = (@files == 1 ? $files[0] : $self->catdir(@files));
	my $spath = $path;
	$spath =~ s/\.dir\Z(?!\n)//;
	if ($spath =~ /^[^\)\]\/:>]+\)\Z(?!\n)/s && basename($file) eq $file) {
	    $rslt = "$spath$file";
	}
	else {
	    $rslt = $self->eliminate_macros($spath);
	    $rslt = vmsify($rslt.($rslt ? '/' : '').unixify($file));
	}
    }
    else { $rslt = (defined($file) && length($file)) ? vmsify($file) : ''; }
    return $self->canonpath($rslt);
}


=item curdir (override)

Returns a string representation of the current directory: '[]'

=cut

sub curdir {
    return '[]';
}

=item devnull (override)

Returns a string representation of the null device: '_NLA0:'

=cut

sub devnull {
    return "_NLA0:";
}

=item rootdir (override)

Returns a string representation of the root directory: 'SYS$DISK:[000000]'

=cut

sub rootdir {
    return 'SYS$DISK:[000000]';
}

=item tmpdir (override)

Returns a string representation of the first writable directory
from the following list or '' if none are writable:

    sys$scratch:
    $ENV{TMPDIR}

=cut

my $tmpdir;
sub tmpdir {
    return $tmpdir if defined $tmpdir;
    foreach ('sys$scratch:', $ENV{TMPDIR}) {
	next unless defined && -d && -w _;
	$tmpdir = $_;
	last;
    }
    $tmpdir = '' unless defined $tmpdir;
    return $tmpdir;
}

=item updir (override)

Returns a string representation of the parent directory: '[-]'

=cut

sub updir {
    return '[-]';
}

=item case_tolerant (override)

VMS file specification syntax is case-tolerant.

=cut

sub case_tolerant {
    return 1;
}

=item path (override)

Translate logical name DCL$PATH as a searchlist, rather than trying
to C<split> string value of C<$ENV{'PATH'}>.

=cut

sub path {
    my (@dirs,$dir,$i);
    while ($dir = $ENV{'DCL$PATH;' . $i++}) { push(@dirs,$dir); }
    return @dirs;
}

=item file_name_is_absolute (override)

Checks for VMS directory spec as well as Unix separators.

=cut

sub file_name_is_absolute {
    my ($self,$file) = @_;
    # If it's a logical name, expand it.
    $file = $ENV{$file} while $file =~ /^[\w\$\-]+\Z(?!\n)/s && $ENV{$file};
    return scalar($file =~ m!^/!s             ||
		  $file =~ m![<\[][^.\-\]>]!  ||
		  $file =~ /:[^<\[]/);
}

=item splitpath (override)

Splits using VMS syntax.

=cut

sub splitpath {
    my($self,$path) = @_;
    my($dev,$dir,$file) = ('','','');

    vmsify($path) =~ /(.+:)?([\[<].*[\]>])?(.*)/s;
    return ($1 || '',$2 || '',$3);
}

=item splitdir (override)

Split dirspec using VMS syntax.

=cut

sub splitdir {
    my($self,$dirspec) = @_;
    $dirspec =~ s/\]\[//g;  $dirspec =~ s/\-\-/-.-/g;
    $dirspec = "[$dirspec]" unless $dirspec =~ /[\[<]/; # make legal
    my(@dirs) = split('\.', vmspath($dirspec));
    $dirs[0] =~ s/^[\[<]//s;  $dirs[-1] =~ s/[\]>]\Z(?!\n)//s;
    @dirs;
}


=item catpath (override)

Construct a complete filespec using VMS syntax

=cut

sub catpath {
    my($self,$dev,$dir,$file) = @_;
    if ($dev =~ m|^/+([^/]+)|) { $dev = "$1:"; }
    else { $dev .= ':' unless $dev eq '' or $dev =~ /:\Z(?!\n)/; }
    if (length($dev) or length($dir)) {
      $dir = "[$dir]" unless $dir =~ /[\[<\/]/;
      $dir = vmspath($dir);
    }
    "$dev$dir$file";
}

=item abs2rel (override)

Use VMS syntax when converting filespecs.

=cut

sub abs2rel {
    my $self = shift;

    return vmspath(File::Spec::Unix::abs2rel( $self, @_ ))
        if ( join( '', @_ ) =~ m{/} ) ;

    my($path,$base) = @_;

    # Note: we use '/' to glue things together here, then let canonpath()
    # clean them up at the end.

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

    # Split up paths
    my ( $path_directories, $path_file ) =
        ($self->splitpath( $path, 1 ))[1,2] ;

    $path_directories = $1
        if $path_directories =~ /^\[(.*)\]\Z(?!\n)/s ;

    my $base_directories = ($self->splitpath( $base, 1 ))[1] ;

    $base_directories = $1
        if $base_directories =~ /^\[(.*)\]\Z(?!\n)/s ;

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

    # @basechunks now contains the directories to climb out of,
    # @pathchunks now has the directories to descend in to.
    $path_directories = '-.' x @basechunks . join( '.', @pathchunks ) ;
    $path_directories =~ s{\.\Z(?!\n)}{} ;
    return $self->canonpath( $self->catpath( '', $path_directories, $path_file ) ) ;
}


=item rel2abs (override)

Use VMS syntax when converting filespecs.

=cut

sub rel2abs {
    my $self = shift ;
    return vmspath(File::Spec::Unix::rel2abs( $self, @_ ))
        if ( join( '', @_ ) =~ m{/} ) ;

    my ($path,$base ) = @_;
    # Clean up and split up $path
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

        # Split up paths
        my ( $path_directories, $path_file ) =
            ($self->splitpath( $path ))[1,2] ;

        my ( $base_volume, $base_directories ) =
            $self->splitpath( $base ) ;

        $path_directories = '' if $path_directories eq '[]' ||
                                  $path_directories eq '<>';
        my $sep = '' ;
        $sep = '.'
            if ( $base_directories =~ m{[^.\]>]\Z(?!\n)} &&
                 $path_directories =~ m{^[^.\[<]}s
            ) ;
        $base_directories = "$base_directories$sep$path_directories";
        $base_directories =~ s{\.?[\]>][\[<]\.?}{.};

        $path = $self->catpath( $base_volume, $base_directories, $path_file );
   }

    return $self->canonpath( $path ) ;
}


=back

=head1 SEE ALSO

L<File::Spec>

=cut

1;

package File::Path;

=head1 NAME

File::Path - create or remove a series of directories

=head1 SYNOPSIS

C<use File::Path>

C<mkpath(['/foo/bar/baz', 'blurfl/quux'], 1, 0711);>

C<rmtree(['foo/bar/baz', 'blurfl/quux'], 1, 1);>

=head1 DESCRIPTION

The C<mkpath> function provides a convenient way to create directories, even
if your C<mkdir> kernel call won't create more than one level of directory at
a time.  C<mkpath> takes three arguments:

=over 4

=item *

the name of the path to create, or a reference
to a list of paths to create,

=item *

a boolean value, which if TRUE will cause C<mkpath>
to print the name of each directory as it is created
(defaults to FALSE), and

=item *

the numeric mode to use when creating the directories
(defaults to 0777)

=back

It returns a list of all directories (including intermediates, determined
using the Unix '/' separator) created.

Similarly, the C<rmtree> function provides a convenient way to delete a
subtree from the directory structure, much like the Unix command C<rm -r>.
C<rmtree> takes three arguments:

=over 4

=item *

the root of the subtree to delete, or a reference to
a list of roots.  All of the files and directories
below each root, as well as the roots themselves,
will be deleted.

=item *

a boolean value, which if TRUE will cause C<rmtree> to
print a message each time it examines a file, giving the
name of the file, and indicating whether it's using C<rmdir>
or C<unlink> to remove it, or that it's skipping it.
(defaults to FALSE)

=item *

a boolean value, which if TRUE will cause C<rmtree> to
skip any files to which you do not have delete access
(if running under VMS) or write access (if running
under another OS).  This will change in the future when
a criterion for 'delete permission' under OSs other
than VMS is settled.  (defaults to FALSE)

=back

It returns the number of files successfully deleted.  Symlinks are
treated as ordinary files.

B<NOTE:> If the third parameter is not TRUE, C<rmtree> is B<unsecure>
in the face of failure or interruption.  Files and directories which
were not deleted may be left with permissions reset to allow world
read and write access.  Note also that the occurrence of errors in
rmtree can be determined I<only> by trapping diagnostic messages
using C<$SIG{__WARN__}>; it is not apparent from the return value.
Therefore, you must be extremely careful about using C<rmtree($foo,$bar,0>
in situations where security is an issue.

=head1 AUTHORS

Tim Bunce <F<Tim.Bunce@ig.co.uk>> and
Charles Bailey <F<bailey@newman.upenn.edu>>

=head1 REVISION

Current $VERSION is 1.0401.

=cut

use Carp;
use File::Basename ();
use DirHandle ();
use Exporter ();
use strict;

use vars qw( $VERSION @ISA @EXPORT );
$VERSION = "1.0401";
@ISA = qw( Exporter );
@EXPORT = qw( mkpath rmtree );

my $Is_VMS = $^O eq 'VMS';

# These OSes complain if you want to remove a file that you have no
# write permission to:
my $force_writeable = ($^O eq 'os2' || $^O eq 'dos' || $^O eq 'MSWin32'
		       || $^O eq 'amigaos');

sub mkpath {
    my($paths, $verbose, $mode) = @_;
    # $paths   -- either a path string or ref to list of paths
    # $verbose -- optional print "mkdir $path" for each directory created
    # $mode    -- optional permissions, defaults to 0777
    local($")="/";
    $mode = 0777 unless defined($mode);
    $paths = [$paths] unless ref $paths;
    my(@created,$path);
    foreach $path (@$paths) {
	$path .= '/' if $^O eq 'os2' and $path =~ /^\w:$/; # feature of CRT 
	next if -d $path;
	# Logic wants Unix paths, so go with the flow.
	$path = VMS::Filespec::unixify($path) if $Is_VMS;
	my $parent = File::Basename::dirname($path);
	# Allow for creation of new logical filesystems under VMS
	if (not $Is_VMS or $parent !~ m:/[^/]+/000000/?:) {
	    push(@created,mkpath($parent, $verbose, $mode)) unless (-d $parent);
	}
	print "mkdir $path\n" if $verbose;
	unless (mkdir($path,$mode)) {
	  my $e = $!;
	  # allow for another process to have created it meanwhile
	  croak "mkdir $path: $e" unless -d $path;
	}
	push(@created, $path);
    }
    @created;
}

sub rmtree {
    my($roots, $verbose, $safe) = @_;
    my(@files);
    my($count) = 0;
    $roots = [$roots] unless ref $roots;
    $verbose ||= 0;
    $safe ||= 0;

    my($root);
    foreach $root (@{$roots}) {
	$root =~ s#/$##;
	(undef, undef, my $rp) = lstat $root or next;
	$rp &= 07777;	# don't forget setuid, setgid, sticky bits
	if ( -d _ ) {
	    # notabene: 0777 is for making readable in the first place,
	    # it's also intended to change it to writable in case we have
	    # to recurse in which case we are better than rm -rf for 
	    # subtrees with strange permissions
	    chmod(0777, ($Is_VMS ? VMS::Filespec::fileify($root) : $root))
	      or carp "Can't make directory $root read+writeable: $!"
		unless $safe;

	    my $d = DirHandle->new($root)
	      or carp "Can't read $root: $!";
	    @files = $d->read;
	    $d->close;

	    # Deleting large numbers of files from VMS Files-11 filesystems
	    # is faster if done in reverse ASCIIbetical order 
	    @files = reverse @files if $Is_VMS;
	    ($root = VMS::Filespec::unixify($root)) =~ s#\.dir$## if $Is_VMS;
	    @files = map("$root/$_", grep $_!~/^\.{1,2}$/,@files);
	    $count += rmtree(\@files,$verbose,$safe);
	    if ($safe &&
		($Is_VMS ? !&VMS::Filespec::candelete($root) : !-w $root)) {
		print "skipped $root\n" if $verbose;
		next;
	    }
	    chmod 0777, $root
	      or carp "Can't make directory $root writeable: $!"
		if $force_writeable;
	    print "rmdir $root\n" if $verbose;
	    if (rmdir $root) {
		++$count;
	    }
	    else {
		carp "Can't remove directory $root: $!";
		chmod($rp, ($Is_VMS ? VMS::Filespec::fileify($root) : $root))
		    or carp("and can't restore permissions to "
		            . sprintf("0%o",$rp) . "\n");
	    }
	}
	else { 
	    if ($safe &&
		($Is_VMS ? !&VMS::Filespec::candelete($root) : !-w $root)) {
		print "skipped $root\n" if $verbose;
		next;
	    }
	    chmod 0666, $root
	      or carp "Can't make file $root writeable: $!"
		if $force_writeable;
	    print "unlink $root\n" if $verbose;
	    # delete all versions under VMS
	    for (;;) {
		unless (unlink $root) {
		    carp "Can't unlink file $root: $!";
		    if ($force_writeable) {
			chmod $rp, $root
			    or carp("and can't restore permissions to "
			            . sprintf("0%o",$rp) . "\n");
		    }
		    last;
		}
		++$count;
		last unless $Is_VMS && lstat $root;
	    }
	}
    }

    $count;
}

1;

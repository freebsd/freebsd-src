package File::Spec::Mac;

use Exporter ();
use Config;
use strict;
use File::Spec;
use vars qw(@ISA $VERSION $Is_Mac);

$VERSION = '1.0';

@ISA = qw(File::Spec::Unix);
$Is_Mac = $^O eq 'MacOS';

Exporter::import('File::Spec', '$Verbose');


=head1 NAME

File::Spec::Mac - File::Spec for MacOS

=head1 SYNOPSIS

C<require File::Spec::Mac;>

=head1 DESCRIPTION

Methods for manipulating file specifications.

=head1 METHODS

=over 2

=item canonpath

On MacOS, there's nothing to be done.  Returns what it's given.

=cut

sub canonpath {
    my($self,$path) = @_;
    $path;
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

# ';

sub catdir {
    shift;
    my @args = @_;
	$args[0] =~ s/:$//;
	my $result = shift @args;
	for (@args) {
		s/:$//;
		s/^://;
		$result .= ":$_";
    }
    $result .= ":";
	$result;
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
    my $self = shift @_;
    my $file = pop @_;
    return $file unless @_;
    my $dir = $self->catdir(@_);
	$file =~ s/^://;
    return $dir.$file;
}

=item curdir

Returns a string representing of the current directory.

=cut

sub curdir {
    return ":" ;
}

=item rootdir

Returns a string representing the root directory.  Under MacPerl,
returns the name of the startup volume, since that's the closest in
concept, although other volumes aren't rooted there.  On any other
platform returns '', since there's no common way to indicate "root
directory" across all Macs.

=cut

sub rootdir {
#
#  There's no real root directory on MacOS.  If you're using MacPerl,
#  the name of the startup volume is returned, since that's the closest in
#  concept.  On other platforms, simply return '', because nothing better
#  can be done.
#
	if($Is_Mac) {
        require Mac::Files;
		my $system =  Mac::Files::FindFolder(&Mac::Files::kOnSystemDisk,
		        &Mac::Files::kSystemFolderType);
		$system =~ s/:.*$/:/;
		return $system;
	} else {
		return '';
    }
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

=cut

sub file_name_is_absolute {
    my($self,$file) = @_;
	if ($file =~ /:/) {
		return ($file !~ m/^:/);
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
    my($self) = @_;
	my @path;
	if(exists $ENV{Commands}) {
		@path = split /,/,$ENV{Commands};
	} else {
	    @path = ();
	}
    @path;
}

=back

=head1 SEE ALSO

L<File::Spec>

=cut

1;
__END__


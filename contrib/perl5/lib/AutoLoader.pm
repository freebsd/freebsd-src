package AutoLoader;

use 5.005_64;
our(@EXPORT, @EXPORT_OK, $VERSION);

my $is_dosish;
my $is_epoc;
my $is_vms;
my $is_macos;

BEGIN {
    require Exporter;
    @EXPORT = @EXPORT = ();
    @EXPORT_OK = @EXPORT_OK = qw(AUTOLOAD);
    $is_dosish = $^O eq 'dos' || $^O eq 'os2' || $^O eq 'MSWin32';
    $is_epoc = $^O eq 'epoc';
    $is_vms = $^O eq 'VMS';
    $is_macos = $^O eq 'MacOS';
    $VERSION = '5.58';
}

AUTOLOAD {
    my $sub = $AUTOLOAD;
    my $filename;
    # Braces used to preserve $1 et al.
    {
	# Try to find the autoloaded file from the package-qualified
	# name of the sub. e.g., if the sub needed is
	# Getopt::Long::GetOptions(), then $INC{Getopt/Long.pm} is
	# something like '/usr/lib/perl5/Getopt/Long.pm', and the
	# autoload file is '/usr/lib/perl5/auto/Getopt/Long/GetOptions.al'.
	#
	# However, if @INC is a relative path, this might not work.  If,
	# for example, @INC = ('lib'), then $INC{Getopt/Long.pm} is
	# 'lib/Getopt/Long.pm', and we want to require
	# 'auto/Getopt/Long/GetOptions.al' (without the leading 'lib').
	# In this case, we simple prepend the 'auto/' and let the
	# C<require> take care of the searching for us.

	my ($pkg,$func) = ($sub =~ /(.*)::([^:]+)$/);
	$pkg =~ s#::#/#g;
	if (defined($filename = $INC{"$pkg.pm"})) {
	    if ($is_macos) {
		$pkg =~ tr#/#:#;
		$filename =~ s#^(.*)$pkg\.pm\z#$1auto:$pkg:$func.al#s;
	    } else {
		$filename =~ s#^(.*)$pkg\.pm\z#$1auto/$pkg/$func.al#s;
	    }

	    # if the file exists, then make sure that it is a
	    # a fully anchored path (i.e either '/usr/lib/auto/foo/bar.al',
	    # or './lib/auto/foo/bar.al'.  This avoids C<require> searching
	    # (and failing) to find the 'lib/auto/foo/bar.al' because it
	    # looked for 'lib/lib/auto/foo/bar.al', given @INC = ('lib').

	    if (-r $filename) {
		unless ($filename =~ m|^/|s) {
		    if ($is_dosish) {
			unless ($filename =~ m{^([a-z]:)?[\\/]}is) {
			     $filename = "./$filename";
			}
		    }
		    elsif ($is_epoc) {
			unless ($filename =~ m{^([a-z?]:)?[\\/]}is) {
			     $filename = "./$filename";
			}
		    }elsif ($is_vms) {
			# XXX todo by VMSmiths
			$filename = "./$filename";
		    }
		    elsif (!$is_macos) {
			$filename = "./$filename";
		    }
		}
	    }
	    else {
		$filename = undef;
	    }
	}
	unless (defined $filename) {
	    # let C<require> do the searching
	    $filename = "auto/$sub.al";
	    $filename =~ s#::#/#g;
	}
    }
    my $save = $@;
    eval { local $SIG{__DIE__}; require $filename };
    if ($@) {
	if (substr($sub,-9) eq '::DESTROY') {
	    *$sub = sub {};
	} else {
	    # The load might just have failed because the filename was too
	    # long for some old SVR3 systems which treat long names as errors.
	    # If we can succesfully truncate a long name then it's worth a go.
	    # There is a slight risk that we could pick up the wrong file here
	    # but autosplit should have warned about that when splitting.
	    if ($filename =~ s/(\w{12,})\.al$/substr($1,0,11).".al"/e){
		eval { local $SIG{__DIE__}; require $filename };
	    }
	    if ($@){
		$@ =~ s/ at .*\n//;
		my $error = $@;
		require Carp;
		Carp::croak($error);
	    }
	}
    }
    $@ = $save;
    goto &$sub;
}

sub import {
    my $pkg = shift;
    my $callpkg = caller;

    #
    # Export symbols, but not by accident of inheritance.
    #

    if ($pkg eq 'AutoLoader') {
      local $Exporter::ExportLevel = 1;
      Exporter::import $pkg, @_;
    }

    #
    # Try to find the autosplit index file.  Eg., if the call package
    # is POSIX, then $INC{POSIX.pm} is something like
    # '/usr/local/lib/perl5/POSIX.pm', and the autosplit index file is in
    # '/usr/local/lib/perl5/auto/POSIX/autosplit.ix', so we require that.
    #
    # However, if @INC is a relative path, this might not work.  If,
    # for example, @INC = ('lib'), then
    # $INC{POSIX.pm} is 'lib/POSIX.pm', and we want to require
    # 'auto/POSIX/autosplit.ix' (without the leading 'lib').
    #

    (my $calldir = $callpkg) =~ s#::#/#g;
    my $path = $INC{$calldir . '.pm'};
    if (defined($path)) {
	# Try absolute path name.
	$path =~ s#^(.*)$calldir\.pm$#$1auto/$calldir/autosplit.ix#;
	eval { require $path; };
	# If that failed, try relative path with normal @INC searching.
	if ($@) {
	    $path ="auto/$calldir/autosplit.ix";
	    eval { require $path; };
	}
	if ($@) {
	    my $error = $@;
	    require Carp;
	    Carp::carp($error);
	}
    } 
}

sub unimport {
  my $callpkg = caller;
  eval "package $callpkg; sub AUTOLOAD;";
}

1;

__END__

=head1 NAME

AutoLoader - load subroutines only on demand

=head1 SYNOPSIS

    package Foo;
    use AutoLoader 'AUTOLOAD';   # import the default AUTOLOAD subroutine

    package Bar;
    use AutoLoader;              # don't import AUTOLOAD, define our own
    sub AUTOLOAD {
        ...
        $AutoLoader::AUTOLOAD = "...";
        goto &AutoLoader::AUTOLOAD;
    }

=head1 DESCRIPTION

The B<AutoLoader> module works with the B<AutoSplit> module and the
C<__END__> token to defer the loading of some subroutines until they are
used rather than loading them all at once.

To use B<AutoLoader>, the author of a module has to place the
definitions of subroutines to be autoloaded after an C<__END__> token.
(See L<perldata>.)  The B<AutoSplit> module can then be run manually to
extract the definitions into individual files F<auto/funcname.al>.

B<AutoLoader> implements an AUTOLOAD subroutine.  When an undefined
subroutine in is called in a client module of B<AutoLoader>,
B<AutoLoader>'s AUTOLOAD subroutine attempts to locate the subroutine in a
file with a name related to the location of the file from which the
client module was read.  As an example, if F<POSIX.pm> is located in
F</usr/local/lib/perl5/POSIX.pm>, B<AutoLoader> will look for perl
subroutines B<POSIX> in F</usr/local/lib/perl5/auto/POSIX/*.al>, where
the C<.al> file has the same name as the subroutine, sans package.  If
such a file exists, AUTOLOAD will read and evaluate it,
thus (presumably) defining the needed subroutine.  AUTOLOAD will then
C<goto> the newly defined subroutine.

Once this process completes for a given function, it is defined, so
future calls to the subroutine will bypass the AUTOLOAD mechanism.

=head2 Subroutine Stubs

In order for object method lookup and/or prototype checking to operate
correctly even when methods have not yet been defined it is necessary to
"forward declare" each subroutine (as in C<sub NAME;>).  See
L<perlsub/"SYNOPSIS">.  Such forward declaration creates "subroutine
stubs", which are place holders with no code.

The AutoSplit and B<AutoLoader> modules automate the creation of forward
declarations.  The AutoSplit module creates an 'index' file containing
forward declarations of all the AutoSplit subroutines.  When the
AutoLoader module is 'use'd it loads these declarations into its callers
package.

Because of this mechanism it is important that B<AutoLoader> is always
C<use>d and not C<require>d.

=head2 Using B<AutoLoader>'s AUTOLOAD Subroutine

In order to use B<AutoLoader>'s AUTOLOAD subroutine you I<must>
explicitly import it:

    use AutoLoader 'AUTOLOAD';

=head2 Overriding B<AutoLoader>'s AUTOLOAD Subroutine

Some modules, mainly extensions, provide their own AUTOLOAD subroutines.
They typically need to check for some special cases (such as constants)
and then fallback to B<AutoLoader>'s AUTOLOAD for the rest.

Such modules should I<not> import B<AutoLoader>'s AUTOLOAD subroutine.
Instead, they should define their own AUTOLOAD subroutines along these
lines:

    use AutoLoader;
    use Carp;

    sub AUTOLOAD {
        my $sub = $AUTOLOAD;
        (my $constname = $sub) =~ s/.*:://;
        my $val = constant($constname, @_ ? $_[0] : 0);
        if ($! != 0) {
            if ($! =~ /Invalid/ || $!{EINVAL}) {
                $AutoLoader::AUTOLOAD = $sub;
                goto &AutoLoader::AUTOLOAD;
            }
            else {
                croak "Your vendor has not defined constant $constname";
            }
        }
        *$sub = sub { $val }; # same as: eval "sub $sub { $val }";
        goto &$sub;
    }

If any module's own AUTOLOAD subroutine has no need to fallback to the
AutoLoader's AUTOLOAD subroutine (because it doesn't have any AutoSplit
subroutines), then that module should not use B<AutoLoader> at all.

=head2 Package Lexicals

Package lexicals declared with C<my> in the main block of a package
using B<AutoLoader> will not be visible to auto-loaded subroutines, due to
the fact that the given scope ends at the C<__END__> marker.  A module
using such variables as package globals will not work properly under the
B<AutoLoader>.

The C<vars> pragma (see L<perlmod/"vars">) may be used in such
situations as an alternative to explicitly qualifying all globals with
the package namespace.  Variables pre-declared with this pragma will be
visible to any autoloaded routines (but will not be invisible outside
the package, unfortunately).

=head2 Not Using AutoLoader

You can stop using AutoLoader by simply

	no AutoLoader;

=head2 B<AutoLoader> vs. B<SelfLoader>

The B<AutoLoader> is similar in purpose to B<SelfLoader>: both delay the
loading of subroutines.

B<SelfLoader> uses the C<__DATA__> marker rather than C<__END__>.
While this avoids the use of a hierarchy of disk files and the
associated open/close for each routine loaded, B<SelfLoader> suffers a
startup speed disadvantage in the one-time parsing of the lines after
C<__DATA__>, after which routines are cached.  B<SelfLoader> can also
handle multiple packages in a file.

B<AutoLoader> only reads code as it is requested, and in many cases
should be faster, but requires a mechanism like B<AutoSplit> be used to
create the individual files.  L<ExtUtils::MakeMaker> will invoke
B<AutoSplit> automatically if B<AutoLoader> is used in a module source
file.

=head1 CAVEATS

AutoLoaders prior to Perl 5.002 had a slightly different interface.  Any
old modules which use B<AutoLoader> should be changed to the new calling
style.  Typically this just means changing a require to a use, adding
the explicit C<'AUTOLOAD'> import if needed, and removing B<AutoLoader>
from C<@ISA>.

On systems with restrictions on file name length, the file corresponding
to a subroutine may have a shorter name that the routine itself.  This
can lead to conflicting file names.  The I<AutoSplit> package warns of
these potential conflicts when used to split a module.

AutoLoader may fail to find the autosplit files (or even find the wrong
ones) in cases where C<@INC> contains relative paths, B<and> the program
does C<chdir>.

=head1 SEE ALSO

L<SelfLoader> - an autoloader that doesn't use external files.

=cut

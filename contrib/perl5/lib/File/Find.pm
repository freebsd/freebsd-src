package File::Find;
require 5.000;
require Exporter;
require Cwd;

=head1 NAME

find - traverse a file tree

finddepth - traverse a directory structure depth-first

=head1 SYNOPSIS

    use File::Find;
    find(\&wanted, '/foo','/bar');
    sub wanted { ... }

    use File::Find;
    finddepth(\&wanted, '/foo','/bar');
    sub wanted { ... }

=head1 DESCRIPTION

The first argument to find() is either a hash reference describing the
operations to be performed for each file, a code reference, or a string
that contains a subroutine name.  If it is a hash reference, then the
value for the key C<wanted> should be a code reference.  This code
reference is called I<the wanted() function> below.

Currently the only other supported key for the above hash is
C<bydepth>, in presense of which the walk over directories is
performed depth-first.  Entry point finddepth() is a shortcut for
specifying C<{ bydepth => 1}> in the first argument of find().

The wanted() function does whatever verifications you want.
$File::Find::dir contains the current directory name, and $_ the
current filename within that directory.  $File::Find::name contains
C<"$File::Find::dir/$_">.  You are chdir()'d to $File::Find::dir when
the function is called.  The function may set $File::Find::prune to
prune the tree.

File::Find assumes that you don't alter the $_ variable.  If you do then
make sure you return it to its original value before exiting your function.

This library is useful for the C<find2perl> tool, which when fed,

    find2perl / -name .nfs\* -mtime +7 \
	-exec rm -f {} \; -o -fstype nfs -prune

produces something like:

    sub wanted {
        /^\.nfs.*$/ &&
        (($dev,$ino,$mode,$nlink,$uid,$gid) = lstat($_)) &&
        int(-M _) > 7 &&
        unlink($_)
        ||
        ($nlink || (($dev,$ino,$mode,$nlink,$uid,$gid) = lstat($_))) &&
        $dev < 0 &&
        ($File::Find::prune = 1);
    }

Set the variable $File::Find::dont_use_nlink if you're using AFS,
since AFS cheats.

C<finddepth> is just like C<find>, except that it does a depth-first
search.

Here's another interesting wanted function.  It will find all symlinks
that don't resolve:

    sub wanted {
	-l && !-e && print "bogus link: $File::Find::name\n";
    }

=head1 BUGS

There is no way to make find or finddepth follow symlinks.

=cut

@ISA = qw(Exporter);
@EXPORT = qw(find finddepth);


sub find_opt {
    my $wanted = shift;
    my $bydepth = $wanted->{bydepth};
    my $cwd = $bydepth ? Cwd::fastcwd() : Cwd::cwd();
    # Localize these rather than lexicalizing them for backwards
    # compatibility.
    local($topdir,$topdev,$topino,$topmode,$topnlink);
    foreach $topdir (@_) {
	(($topdev,$topino,$topmode,$topnlink) =
	  ($Is_VMS ? stat($topdir) : lstat($topdir)))
	  || (warn("Can't stat $topdir: $!\n"), next);
	if (-d _) {
	    if (chdir($topdir)) {
		$prune = 0;
		unless ($bydepth) {
		  ($dir,$_) = ($topdir,'.');
		  $name = $topdir;
		  $wanted->{wanted}->();
		}
		next if $prune;
		my $fixtopdir = $topdir;
		$fixtopdir =~ s,/$,, ;
		$fixtopdir =~ s/\.dir$// if $Is_VMS;
		&finddir($wanted,$fixtopdir,$topnlink, $bydepth);
		if ($bydepth) {
		  ($dir,$_) = ($fixtopdir,'.');
		  $name = $fixtopdir;
		  $wanted->{wanted}->();
		}
	    }
	    else {
		warn "Can't cd to $topdir: $!\n";
	    }
	}
	else {
	    require File::Basename;
	    unless (($_,$dir) = File::Basename::fileparse($topdir)) {
		($dir,$_) = ('.', $topdir);
	    }
	    if (chdir($dir)) {
		$name = $topdir;
		$wanted->{wanted}->();
	    }
	    else {
		warn "Can't cd to $dir: $!\n";
	    }
	}
	chdir $cwd;
    }
}

sub finddir {
    my($wanted, $nlink, $bydepth);
    local($dir, $name);
    ($wanted, $dir, $nlink, $bydepth) = @_;

    my($dev, $ino, $mode, $subcount);

    # Get the list of files in the current directory.
    opendir(DIR,'.') || (warn("Can't open $dir: $!\n"), $bydepth || return);
    my(@filenames) = readdir(DIR);
    closedir(DIR);

    if ($nlink == 2 && !$dont_use_nlink) {  # This dir has no subdirectories.
	for (@filenames) {
	    next if $_ eq '.';
	    next if $_ eq '..';
	    $name = "$dir/$_";
	    $nlink = 0;
	    $wanted->{wanted}->();
	}
    }
    else {		      # This dir has subdirectories.
	$subcount = $nlink - 2;
	for (@filenames) {
	    next if $_ eq '.';
	    next if $_ eq '..';
	    $nlink = 0;
	    $prune = 0 unless $bydepth;
	    $name = "$dir/$_";
	    $wanted->{wanted}->() unless $bydepth;
	    if ($subcount > 0 || $dont_use_nlink) {    # Seen all the subdirs?

		# Get link count and check for directoriness.

		($dev,$ino,$mode,$nlink) = ($Is_VMS ? stat($_) : lstat($_));
		    # unless ($nlink || $dont_use_nlink);

		if (-d _) {

		    # It really is a directory, so do it recursively.

		    --$subcount;
		    next if $prune;
		    # Untaint $_, so that we can do a chdir
		    $_ = $1 if /^(.*)/;
		    if (chdir $_) {
			$name =~ s/\.dir$// if $Is_VMS;
			&finddir($wanted,$name,$nlink, $bydepth);
			chdir '..';
		    }
		    else {
			warn "Can't cd to $_: $!\n";
		    }
		}
	    }
	    $wanted->{wanted}->() if $bydepth;
	}
    }
}

sub wrap_wanted {
  my $wanted = shift;
  ref($wanted) eq 'HASH' ? $wanted : { wanted => $wanted };
}

sub find {
  my $wanted = shift;
  find_opt(wrap_wanted($wanted), @_);
}

sub finddepth {
  my $wanted = wrap_wanted(shift);
  $wanted->{bydepth} = 1;
  find_opt($wanted, @_);
}

# These are hard-coded for now, but may move to hint files.
if ($^O eq 'VMS') {
  $Is_VMS = 1;
  $dont_use_nlink = 1;
}

$dont_use_nlink = 1
    if $^O eq 'os2' || $^O eq 'dos' || $^O eq 'amigaos' || $^O eq 'MSWin32';

# Set dont_use_nlink in your hint file if your system's stat doesn't
# report the number of links in a directory as an indication
# of the number of files.
# See, e.g. hints/machten.sh for MachTen 2.2.
unless ($dont_use_nlink) {
  require Config;
  $dont_use_nlink = 1 if ($Config::Config{'dont_use_nlink'});
}

1;


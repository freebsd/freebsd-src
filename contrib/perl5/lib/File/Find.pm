package File::Find;
use 5.005_64;
require Exporter;
require Cwd;

=head1 NAME

find - traverse a file tree

finddepth - traverse a directory structure depth-first

=head1 SYNOPSIS

    use File::Find;
    find(\&wanted, '/foo', '/bar');
    sub wanted { ... }

    use File::Find;
    finddepth(\&wanted, '/foo', '/bar');
    sub wanted { ... }

    use File::Find;
    find({ wanted => \&process, follow => 1 }, '.');

=head1 DESCRIPTION

The first argument to find() is either a hash reference describing the
operations to be performed for each file, or a code reference.

Here are the possible keys for the hash:

=over 3

=item C<wanted>

The value should be a code reference.  This code reference is called
I<the wanted() function> below.

=item C<bydepth>

Reports the name of a directory only AFTER all its entries
have been reported.  Entry point finddepth() is a shortcut for
specifying C<{ bydepth => 1 }> in the first argument of find().

=item C<preprocess>

The value should be a code reference.  This code reference is used to
preprocess a directory; it is called after readdir() but before the loop that
calls the wanted() function.  It is called with a list of strings and is
expected to return a list of strings.  The code can be used to sort the
strings alphabetically, numerically, or to filter out directory entries based
on their name alone.

=item C<postprocess>

The value should be a code reference.  It is invoked just before leaving the
current directory.  It is called in void context with no arguments.  The name
of the current directory is in $File::Find::dir.  This hook is handy for
summarizing a directory, such as calculating its disk usage.

=item C<follow>

Causes symbolic links to be followed. Since directory trees with symbolic
links (followed) may contain files more than once and may even have
cycles, a hash has to be built up with an entry for each file.
This might be expensive both in space and time for a large
directory tree. See I<follow_fast> and I<follow_skip> below.
If either I<follow> or I<follow_fast> is in effect:

=over 6

=item *

It is guaranteed that an I<lstat> has been called before the user's
I<wanted()> function is called. This enables fast file checks involving S< _>.

=item *

There is a variable C<$File::Find::fullname> which holds the absolute
pathname of the file with all symbolic links resolved

=back

=item C<follow_fast>

This is similar to I<follow> except that it may report some files more
than once.  It does detect cycles, however.  Since only symbolic links
have to be hashed, this is much cheaper both in space and time.  If
processing a file more than once (by the user's I<wanted()> function)
is worse than just taking time, the option I<follow> should be used.

=item C<follow_skip>

C<follow_skip==1>, which is the default, causes all files which are
neither directories nor symbolic links to be ignored if they are about
to be processed a second time. If a directory or a symbolic link 
are about to be processed a second time, File::Find dies.
C<follow_skip==0> causes File::Find to die if any file is about to be
processed a second time.
C<follow_skip==2> causes File::Find to ignore any duplicate files and
dirctories but to proceed normally otherwise.


=item C<no_chdir>

Does not C<chdir()> to each directory as it recurses. The wanted()
function will need to be aware of this, of course. In this case,
C<$_> will be the same as C<$File::Find::name>.

=item C<untaint>

If find is used in taint-mode (-T command line switch or if EUID != UID
or if EGID != GID) then internally directory names have to be untainted
before they can be cd'ed to. Therefore they are checked against a regular
expression I<untaint_pattern>.  Note that all names passed to the
user's I<wanted()> function are still tainted. 

=item C<untaint_pattern>

See above. This should be set using the C<qr> quoting operator.
The default is set to  C<qr|^([-+@\w./]+)$|>. 
Note that the parantheses are vital.

=item C<untaint_skip>

If set, directories (subtrees) which fail the I<untaint_pattern>
are skipped. The default is to 'die' in such a case.

=back

The wanted() function does whatever verifications you want.
C<$File::Find::dir> contains the current directory name, and C<$_> the
current filename within that directory.  C<$File::Find::name> contains
the complete pathname to the file. You are chdir()'d to
C<$File::Find::dir> when the function is called, unless C<no_chdir>
was specified.  When <follow> or <follow_fast> are in effect, there is
also a C<$File::Find::fullname>.  The function may set
C<$File::Find::prune> to prune the tree unless C<bydepth> was
specified.  Unless C<follow> or C<follow_fast> is specified, for
compatibility reasons (find.pl, find2perl) there are in addition the
following globals available: C<$File::Find::topdir>,
C<$File::Find::topdev>, C<$File::Find::topino>,
C<$File::Find::topmode> and C<$File::Find::topnlink>.

This library is useful for the C<find2perl> tool, which when fed,

    find2perl / -name .nfs\* -mtime +7 \
        -exec rm -f {} \; -o -fstype nfs -prune

produces something like:

    sub wanted {
        /^\.nfs.*\z/s &&
        (($dev, $ino, $mode, $nlink, $uid, $gid) = lstat($_)) &&
        int(-M _) > 7 &&
        unlink($_)
        ||
        ($nlink || (($dev, $ino, $mode, $nlink, $uid, $gid) = lstat($_))) &&
        $dev < 0 &&
        ($File::Find::prune = 1);
    }

Set the variable C<$File::Find::dont_use_nlink> if you're using AFS,
since AFS cheats.


Here's another interesting wanted function.  It will find all symlinks
that don't resolve:

    sub wanted {
         -l && !-e && print "bogus link: $File::Find::name\n";
    }

See also the script C<pfind> on CPAN for a nice application of this
module.

=head1 CAVEAT

Be aware that the option to follow symbolic links can be dangerous.
Depending on the structure of the directory tree (including symbolic
links to directories) you might traverse a given (physical) directory
more than once (only if C<follow_fast> is in effect). 
Furthermore, deleting or changing files in a symbolically linked directory
might cause very unpleasant surprises, since you delete or change files
in an unknown directory.


=cut

@ISA = qw(Exporter);
@EXPORT = qw(find finddepth);


use strict;
my $Is_VMS;

require File::Basename;

my %SLnkSeen;
my ($wanted_callback, $avoid_nlink, $bydepth, $no_chdir, $follow,
    $follow_skip, $full_check, $untaint, $untaint_skip, $untaint_pat,
    $pre_process, $post_process);

sub contract_name {
    my ($cdir,$fn) = @_;

    return substr($cdir,0,rindex($cdir,'/')) if $fn eq '.';

    $cdir = substr($cdir,0,rindex($cdir,'/')+1);

    $fn =~ s|^\./||;

    my $abs_name= $cdir . $fn;

    if (substr($fn,0,3) eq '../') {
	do 1 while ($abs_name=~ s|/(?>[^/]+)/\.\./|/|);
    }

    return $abs_name;
}


sub PathCombine($$) {
    my ($Base,$Name) = @_;
    my $AbsName;

    if (substr($Name,0,1) eq '/') {
	$AbsName= $Name;
    }
    else {
	$AbsName= contract_name($Base,$Name);
    }

    # (simple) check for recursion
    my $newlen= length($AbsName);
    if ($newlen <= length($Base)) {
	if (($newlen == length($Base) || substr($Base,$newlen,1) eq '/')
	    && $AbsName eq substr($Base,0,$newlen))
	{
	    return undef;
	}
    }
    return $AbsName;
}

sub Follow_SymLink($) {
    my ($AbsName) = @_;

    my ($NewName,$DEV, $INO);
    ($DEV, $INO)= lstat $AbsName;

    while (-l _) {
	if ($SLnkSeen{$DEV, $INO}++) {
	    if ($follow_skip < 2) {
		die "$AbsName is encountered a second time";
	    }
	    else {
		return undef;
	    }
	}
	$NewName= PathCombine($AbsName, readlink($AbsName));
	unless(defined $NewName) {
	    if ($follow_skip < 2) {
		die "$AbsName is a recursive symbolic link";
	    }
	    else {
		return undef;
	    }
	}
	else {
	    $AbsName= $NewName;
	}
	($DEV, $INO) = lstat($AbsName);
	return undef unless defined $DEV;  #  dangling symbolic link
    }

    if ($full_check && $SLnkSeen{$DEV, $INO}++) {
	if ($follow_skip < 1) {
	    die "$AbsName encountered a second time";
	}
	else {
	    return undef;
	}
    }

    return $AbsName;
}

our($dir, $name, $fullname, $prune);
sub _find_dir_symlnk($$$);
sub _find_dir($$$);

sub _find_opt {
    my $wanted = shift;
    die "invalid top directory" unless defined $_[0];

    my $cwd           = $wanted->{bydepth} ? Cwd::fastcwd() : Cwd::cwd();
    my $cwd_untainted = $cwd;
    $wanted_callback  = $wanted->{wanted};
    $bydepth          = $wanted->{bydepth};
    $pre_process      = $wanted->{preprocess};
    $post_process     = $wanted->{postprocess};
    $no_chdir         = $wanted->{no_chdir};
    $full_check       = $wanted->{follow};
    $follow           = $full_check || $wanted->{follow_fast};
    $follow_skip      = $wanted->{follow_skip};
    $untaint          = $wanted->{untaint};
    $untaint_pat      = $wanted->{untaint_pattern};
    $untaint_skip     = $wanted->{untaint_skip};

    # for compatability reasons (find.pl, find2perl)
    our ($topdir, $topdev, $topino, $topmode, $topnlink);

    # a symbolic link to a directory doesn't increase the link count
    $avoid_nlink      = $follow || $File::Find::dont_use_nlink;
    
    if ( $untaint ) {
	$cwd_untainted= $1 if $cwd_untainted =~ m|$untaint_pat|;
	die "insecure cwd in find(depth)"  unless defined($cwd_untainted);
    }
    
    my ($abs_dir, $Is_Dir);

    Proc_Top_Item:
    foreach my $TOP (@_) {
        my $top_item = $TOP;
        $top_item =~ s|/\z|| unless $top_item eq '/';
        $Is_Dir= 0;
        
        ($topdev,$topino,$topmode,$topnlink) = stat $top_item;

        if ($follow) {
            if (substr($top_item,0,1) eq '/') {
                $abs_dir = $top_item;
            }
	    elsif ($top_item eq '.') {
		$abs_dir = $cwd;
	    }
            else {  # care about any  ../
		$abs_dir = contract_name("$cwd/",$top_item); 
            }
            $abs_dir= Follow_SymLink($abs_dir);
            unless (defined $abs_dir) {
		warn "$top_item is a dangling symbolic link\n";
		next Proc_Top_Item;
            }
            if (-d _) {
		_find_dir_symlnk($wanted, $abs_dir, $top_item);
		$Is_Dir= 1;
            }
        }
	else { # no follow
            $topdir = $top_item;
            unless (defined $topnlink) {
                warn "Can't stat $top_item: $!\n";
                next Proc_Top_Item;
            }
            if (-d _) {
		$top_item =~ s/\.dir\z// if $Is_VMS;
		_find_dir($wanted, $top_item, $topnlink);
		$Is_Dir= 1;
            }
	    else {
		$abs_dir= $top_item;
            }
        }

        unless ($Is_Dir) {
	    unless (($_,$dir) = File::Basename::fileparse($abs_dir)) {
		($dir,$_) = ('./', $top_item);
	    }

            $abs_dir = $dir;
            if ($untaint) {
		my $abs_dir_save = $abs_dir;
		$abs_dir = $1 if $abs_dir =~ m|$untaint_pat|;
		unless (defined $abs_dir) {
		    if ($untaint_skip == 0) {
			die "directory $abs_dir_save is still tainted";
		    }
		    else {
			next Proc_Top_Item;
		    }
		}
            }

            unless ($no_chdir or chdir $abs_dir) {
                warn "Couldn't chdir $abs_dir: $!\n";
                next Proc_Top_Item;
            }

            $name = $abs_dir . $_;

            { &$wanted_callback }; # protect against wild "next"

        }

        $no_chdir or chdir $cwd_untainted;
    }
}

# API:
#  $wanted
#  $p_dir :  "parent directory"
#  $nlink :  what came back from the stat
# preconditions:
#  chdir (if not no_chdir) to dir

sub _find_dir($$$) {
    my ($wanted, $p_dir, $nlink) = @_;
    my ($CdLvl,$Level) = (0,0);
    my @Stack;
    my @filenames;
    my ($subcount,$sub_nlink);
    my $SE= [];
    my $dir_name= $p_dir;
    my $dir_pref= ( $p_dir eq '/' ? '/' : "$p_dir/" );
    my $dir_rel= '.';      # directory name relative to current directory

    local ($dir, $name, $prune, *DIR);
     
    unless ($no_chdir or $p_dir eq '.') {
	my $udir = $p_dir;
	if ($untaint) {
	    $udir = $1 if $p_dir =~ m|$untaint_pat|;
	    unless (defined $udir) {
		if ($untaint_skip == 0) {
		    die "directory $p_dir is still tainted";
		}
		else {
		    return;
		}
	    }
	}
	unless (chdir $udir) {
	    warn "Can't cd to $udir: $!\n";
	    return;
	}
    }
    
    push @Stack,[$CdLvl,$p_dir,$dir_rel,-1]  if  $bydepth;

    while (defined $SE) {
	unless ($bydepth) {
            $dir= $p_dir;
            $name= $dir_name;
            $_= ($no_chdir ? $dir_name : $dir_rel );
	    # prune may happen here
            $prune= 0;
            { &$wanted_callback }; 	# protect against wild "next"
            next if $prune;
	}
      
	# change to that directory
	unless ($no_chdir or $dir_rel eq '.') {
	    my $udir= $dir_rel;
	    if ($untaint) {
		$udir = $1 if $dir_rel =~ m|$untaint_pat|;
		unless (defined $udir) {
		    if ($untaint_skip == 0) {
			die "directory ("
			    . ($p_dir ne '/' ? $p_dir : '')
			    . "/) $dir_rel is still tainted";
		    }
		}
	    }
	    unless (chdir $udir) {
		warn "Can't cd to ("
		    . ($p_dir ne '/' ? $p_dir : '')
		    . "/) $udir : $!\n";
		next;
	    }
	    $CdLvl++;
	}

	$dir= $dir_name;

	# Get the list of files in the current directory.
	unless (opendir DIR, ($no_chdir ? $dir_name : '.')) {
	    warn "Can't opendir($dir_name): $!\n";
	    next;
	}
	@filenames = readdir DIR;
	closedir(DIR);
	@filenames = &$pre_process(@filenames) if $pre_process;
	push @Stack,[$CdLvl,$dir_name,"",-2]   if $post_process;

	if ($nlink == 2 && !$avoid_nlink) {
	    # This dir has no subdirectories.
	    for my $FN (@filenames) {
		next if $FN =~ /^\.{1,2}\z/;
		
		$name = $dir_pref . $FN;
		$_ = ($no_chdir ? $name : $FN);
		{ &$wanted_callback }; # protect against wild "next"
	    }

	}
	else {
	    # This dir has subdirectories.
	    $subcount = $nlink - 2;

	    for my $FN (@filenames) {
		next if $FN =~ /^\.{1,2}\z/;
		if ($subcount > 0 || $avoid_nlink) {
		    # Seen all the subdirs?
		    # check for directoriness.
		    # stat is faster for a file in the current directory
		    $sub_nlink = (lstat ($no_chdir ? $dir_pref . $FN : $FN))[3];

		    if (-d _) {
			--$subcount;
			$FN =~ s/\.dir\z// if $Is_VMS;
			push @Stack,[$CdLvl,$dir_name,$FN,$sub_nlink];
		    }
		    else {
			$name = $dir_pref . $FN;
			$_= ($no_chdir ? $name : $FN);
			{ &$wanted_callback }; # protect against wild "next"
		    }
		}
		else {
		    $name = $dir_pref . $FN;
		    $_= ($no_chdir ? $name : $FN);
		    { &$wanted_callback }; # protect against wild "next"
		}
	    }
	}
    }
    continue {
	while ( defined ($SE = pop @Stack) ) {
	    ($Level, $p_dir, $dir_rel, $nlink) = @$SE;
	    if ($CdLvl > $Level && !$no_chdir) {
                my $tmp = join('/',('..') x ($CdLvl-$Level));
                die "Can't cd to $dir_name" . $tmp
                    unless chdir ($tmp);
		$CdLvl = $Level;
	    }
	    $dir_name = ($p_dir eq '/' ? "/$dir_rel" : "$p_dir/$dir_rel");
	    $dir_pref = "$dir_name/";
	    if ( $nlink == -2 ) {
		$name = $dir = $p_dir;
		$_ = ".";
		&$post_process;		# End-of-directory processing
            } elsif ( $nlink < 0 ) {  # must be finddepth, report dirname now
                $name = $dir_name;
                if ( substr($name,-2) eq '/.' ) {
                  $name =~ s|/\.$||;
                }
                $dir = $p_dir;
                $_ = ($no_chdir ? $dir_name : $dir_rel );
                if ( substr($_,-2) eq '/.' ) {
                  s|/\.$||;
                }
                { &$wanted_callback }; # protect against wild "next"
            } else {
                push @Stack,[$CdLvl,$p_dir,$dir_rel,-1]  if  $bydepth;
                last;
            }
	}
    }
}


# API:
#  $wanted
#  $dir_loc : absolute location of a dir
#  $p_dir   : "parent directory"
# preconditions:
#  chdir (if not no_chdir) to dir

sub _find_dir_symlnk($$$) {
    my ($wanted, $dir_loc, $p_dir) = @_;
    my @Stack;
    my @filenames;
    my $new_loc;
    my $pdir_loc = $dir_loc;
    my $SE = [];
    my $dir_name = $p_dir;
    my $dir_pref = ( $p_dir   eq '/' ? '/' : "$p_dir/" );
    my $loc_pref = ( $dir_loc eq '/' ? '/' : "$dir_loc/" );
    my $dir_rel = '.';		# directory name relative to current directory
    my $byd_flag;               # flag for pending stack entry if $bydepth

    local ($dir, $name, $fullname, $prune, *DIR);
    
    unless ($no_chdir or $p_dir eq '.') {
	my $udir = $dir_loc;
	if ($untaint) {
	    $udir = $1 if $dir_loc =~ m|$untaint_pat|;
	    unless (defined $udir) {
		if ($untaint_skip == 0) {
		    die "directory $dir_loc is still tainted";
		}
		else {
		    return;
		}
	    }
	}
	unless (chdir $udir) {
	    warn "Can't cd to $udir: $!\n";
	    return;
	}
    }

    push @Stack,[$dir_loc,$pdir_loc,$p_dir,$dir_rel,-1]  if  $bydepth;

    while (defined $SE) {

	unless ($bydepth) {
	    # change to parent directory
	    unless ($no_chdir) {
		my $udir = $pdir_loc;
		if ($untaint) {
		    $udir = $1 if $pdir_loc =~ m|$untaint_pat|;
		}
		unless (chdir $udir) {
		    warn "Can't cd to $udir: $!\n";
		    next;
		}
	    }
	    $dir= $p_dir;
            $name= $dir_name;
            $_= ($no_chdir ? $dir_name : $dir_rel );
            $fullname= $dir_loc;
	    # prune may happen here
            $prune= 0;
	    lstat($_); # make sure  file tests with '_' work
            { &$wanted_callback }; # protect against wild "next"
            next if  $prune;
	}

	# change to that directory
	unless ($no_chdir or $dir_rel eq '.') {
	    my $udir = $dir_loc;
	    if ($untaint) {
		$udir = $1 if $dir_loc =~ m|$untaint_pat|;
		unless (defined $udir ) {
		    if ($untaint_skip == 0) {
			die "directory $dir_loc is still tainted";
		    }
		    else {
			next;
		    }
		}
	    }
	    unless (chdir $udir) {
		warn "Can't cd to $udir: $!\n";
		next;
	    }
	}

	$dir = $dir_name;

	# Get the list of files in the current directory.
	unless (opendir DIR, ($no_chdir ? $dir_loc : '.')) {
	    warn "Can't opendir($dir_loc): $!\n";
	    next;
	}
	@filenames = readdir DIR;
	closedir(DIR);

	for my $FN (@filenames) {
	    next if $FN =~ /^\.{1,2}\z/;

	    # follow symbolic links / do an lstat
	    $new_loc = Follow_SymLink($loc_pref.$FN);

	    # ignore if invalid symlink
	    next unless defined $new_loc;
     
	    if (-d _) {
		push @Stack,[$new_loc,$dir_loc,$dir_name,$FN,1];
	    }
	    else {
		$fullname = $new_loc;
		$name = $dir_pref . $FN;
		$_ = ($no_chdir ? $name : $FN);
		{ &$wanted_callback }; # protect against wild "next"
	    }
	}

    }
    continue {
	while (defined($SE = pop @Stack)) {
	    ($dir_loc, $pdir_loc, $p_dir, $dir_rel, $byd_flag) = @$SE;
	    $dir_name = ($p_dir eq '/' ? "/$dir_rel" : "$p_dir/$dir_rel");
	    $dir_pref = "$dir_name/";
	    $loc_pref = "$dir_loc/";
            if ( $byd_flag < 0 ) {  # must be finddepth, report dirname now
	        unless ($no_chdir or $dir_rel eq '.') {
	            my $udir = $pdir_loc;
	            if ($untaint) {
		        $udir = $1 if $dir_loc =~ m|$untaint_pat|;
	            }
	            unless (chdir $udir) {
		        warn "Can't cd to $udir: $!\n";
		        next;
	            }
	        }
	        $fullname = $dir_loc;
	        $name = $dir_name;
                if ( substr($name,-2) eq '/.' ) {
                  $name =~ s|/\.$||;
                }
                $dir = $p_dir;
	        $_ = ($no_chdir ? $dir_name : $dir_rel);
                if ( substr($_,-2) eq '/.' ) {
                  s|/\.$||;
                }

		lstat($_); # make sure  file tests with '_' work
	        { &$wanted_callback }; # protect against wild "next"
            } else {
                push @Stack,[$dir_loc, $pdir_loc, $p_dir, $dir_rel,-1]  if  $bydepth;
                last;
            }
	}
    }
}


sub wrap_wanted {
    my $wanted = shift;
    if ( ref($wanted) eq 'HASH' ) {
	if ( $wanted->{follow} || $wanted->{follow_fast}) {
	    $wanted->{follow_skip} = 1 unless defined $wanted->{follow_skip};
	}
	if ( $wanted->{untaint} ) {
	    $wanted->{untaint_pattern} = qr|^([-+@\w./]+)$|  
		unless defined $wanted->{untaint_pattern};
	    $wanted->{untaint_skip} = 0 unless defined $wanted->{untaint_skip};
	}
	return $wanted;
    }
    else {
	return { wanted => $wanted };
    }
}

sub find {
    my $wanted = shift;
    _find_opt(wrap_wanted($wanted), @_);
    %SLnkSeen= ();  # free memory
}

sub finddepth {
    my $wanted = wrap_wanted(shift);
    $wanted->{bydepth} = 1;
    _find_opt($wanted, @_);
    %SLnkSeen= ();  # free memory
}

# These are hard-coded for now, but may move to hint files.
if ($^O eq 'VMS') {
    $Is_VMS = 1;
    $File::Find::dont_use_nlink = 1;
}

$File::Find::dont_use_nlink = 1
    if $^O eq 'os2' || $^O eq 'dos' || $^O eq 'amigaos' || $^O eq 'MSWin32' ||
       $^O eq 'cygwin' || $^O eq 'epoc';

# Set dont_use_nlink in your hint file if your system's stat doesn't
# report the number of links in a directory as an indication
# of the number of files.
# See, e.g. hints/machten.sh for MachTen 2.2.
unless ($File::Find::dont_use_nlink) {
    require Config;
    $File::Find::dont_use_nlink = 1 if ($Config::Config{'dont_use_nlink'});
}

1;

package ExtUtils::Liblist;

@ISA = qw(ExtUtils::Liblist::Kid File::Spec);

sub lsdir {
  shift;
  my $rex = qr/$_[1]/;
  opendir my $dir, $_[0];
  grep /$rex/, readdir $dir;
}

sub file_name_is_absolute {
  require File::Spec;
  shift;
  'File::Spec'->file_name_is_absolute(@_);
}


package ExtUtils::Liblist::Kid;

# This kid package is to be used by MakeMaker.  It will not work if
# $self is not a Makemaker.

use 5.005_64;
# Broken out of MakeMaker from version 4.11

our $VERSION = substr q$Revision: 1.26 $, 10;

use Config;
use Cwd 'cwd';
use File::Basename;

sub ext {
  if   ($^O eq 'VMS')     { return &_vms_ext;      }
  elsif($^O eq 'MSWin32') { return &_win32_ext;    }
  else                    { return &_unix_os2_ext; }
}

sub _unix_os2_ext {
    my($self,$potential_libs, $verbose, $give_libs) = @_;
    if ($^O =~ 'os2' and $Config{perllibs}) { 
	# Dynamic libraries are not transitive, so we may need including
	# the libraries linked against perl.dll again.

	$potential_libs .= " " if $potential_libs;
	$potential_libs .= $Config{perllibs};
    }
    return ("", "", "", "", ($give_libs ? [] : ())) unless $potential_libs;
    warn "Potential libraries are '$potential_libs':\n" if $verbose;

    my($so)   = $Config{'so'};
    my($libs) = $Config{'perllibs'};
    my $Config_libext = $Config{lib_ext} || ".a";


    # compute $extralibs, $bsloadlibs and $ldloadlibs from
    # $potential_libs
    # this is a rewrite of Andy Dougherty's extliblist in perl

    my(@searchpath); # from "-L/path" entries in $potential_libs
    my(@libpath) = split " ", $Config{'libpth'};
    my(@ldloadlibs, @bsloadlibs, @extralibs, @ld_run_path, %ld_run_path_seen);
    my(@libs, %libs_seen);
    my($fullname, $thislib, $thispth, @fullname);
    my($pwd) = cwd(); # from Cwd.pm
    my($found) = 0;

    foreach $thislib (split ' ', $potential_libs){

	# Handle possible linker path arguments.
	if ($thislib =~ s/^(-[LR])//){	# save path flag type
	    my($ptype) = $1;
	    unless (-d $thislib){
		warn "$ptype$thislib ignored, directory does not exist\n"
			if $verbose;
		next;
	    }
	    unless ($self->file_name_is_absolute($thislib)) {
	      warn "Warning: $ptype$thislib changed to $ptype$pwd/$thislib\n";
	      $thislib = $self->catdir($pwd,$thislib);
	    }
	    push(@searchpath, $thislib);
	    push(@extralibs,  "$ptype$thislib");
	    push(@ldloadlibs, "$ptype$thislib");
	    next;
	}

	# Handle possible library arguments.
	unless ($thislib =~ s/^-l//){
	  warn "Unrecognized argument in LIBS ignored: '$thislib'\n";
	  next;
	}

	my($found_lib)=0;
	foreach $thispth (@searchpath, @libpath){

		# Try to find the full name of the library.  We need this to
		# determine whether it's a dynamically-loadable library or not.
		# This tends to be subject to various os-specific quirks.
		# For gcc-2.6.2 on linux (March 1995), DLD can not load
		# .sa libraries, with the exception of libm.sa, so we
		# deliberately skip them.
	    if (@fullname =
		    $self->lsdir($thispth,"^\Qlib$thislib.$so.\E[0-9]+")){
		# Take care that libfoo.so.10 wins against libfoo.so.9.
		# Compare two libraries to find the most recent version
		# number.  E.g.  if you have libfoo.so.9.0.7 and
		# libfoo.so.10.1, first convert all digits into two
		# decimal places.  Then we'll add ".00" to the shorter
		# strings so that we're comparing strings of equal length
		# Thus we'll compare libfoo.so.09.07.00 with
		# libfoo.so.10.01.00.  Some libraries might have letters
		# in the version.  We don't know what they mean, but will
		# try to skip them gracefully -- we'll set any letter to
		# '0'.  Finally, sort in reverse so we can take the
		# first element.

		#TODO: iterate through the directory instead of sorting

		$fullname = "$thispth/" .
		(sort { my($ma) = $a;
			my($mb) = $b;
			$ma =~ tr/A-Za-z/0/s;
			$ma =~ s/\b(\d)\b/0$1/g;
			$mb =~ tr/A-Za-z/0/s;
			$mb =~ s/\b(\d)\b/0$1/g;
			while (length($ma) < length($mb)) { $ma .= ".00"; }
			while (length($mb) < length($ma)) { $mb .= ".00"; }
			# Comparison deliberately backwards
			$mb cmp $ma;} @fullname)[0];
	    } elsif (-f ($fullname="$thispth/lib$thislib.$so")
		 && (($Config{'dlsrc'} ne "dl_dld.xs") || ($thislib eq "m"))){
	    } elsif (-f ($fullname="$thispth/lib${thislib}_s$Config_libext")
                 && (! $Config{'archname'} =~ /RM\d\d\d-svr4/)
		 && ($thislib .= "_s") ){ # we must explicitly use _s version
	    } elsif (-f ($fullname="$thispth/lib$thislib$Config_libext")){
	    } elsif (-f ($fullname="$thispth/$thislib$Config_libext")){
	    } elsif (-f ($fullname="$thispth/Slib$thislib$Config_libext")){
	    } elsif ($^O eq 'dgux'
		 && -l ($fullname="$thispth/lib$thislib$Config_libext")
		 && readlink($fullname) =~ /^elink:/s) {
		 # Some of DG's libraries look like misconnected symbolic
		 # links, but development tools can follow them.  (They
		 # look like this:
		 #
		 #    libm.a -> elink:${SDE_PATH:-/usr}/sde/\
		 #    ${TARGET_BINARY_INTERFACE:-m88kdgux}/usr/lib/libm.a
		 #
		 # , the compilation tools expand the environment variables.)
	    } else {
		warn "$thislib not found in $thispth\n" if $verbose;
		next;
	    }
	    warn "'-l$thislib' found at $fullname\n" if $verbose;
	    my($fullnamedir) = dirname($fullname);
	    push @ld_run_path, $fullnamedir unless $ld_run_path_seen{$fullnamedir}++;
	    push @libs, $fullname unless $libs_seen{$fullname}++;
	    $found++;
	    $found_lib++;

	    # Now update library lists

	    # what do we know about this library...
	    my $is_dyna = ($fullname !~ /\Q$Config_libext\E\z/);
	    my $in_perl = ($libs =~ /\B-l\Q$ {thislib}\E\b/s);

	    # Do not add it into the list if it is already linked in
	    # with the main perl executable.
	    # We have to special-case the NeXT, because math and ndbm 
	    # are both in libsys_s
	    unless ($in_perl || 
		($Config{'osname'} eq 'next' &&
		    ($thislib eq 'm' || $thislib eq 'ndbm')) ){
		push(@extralibs, "-l$thislib");
	    }

	    # We might be able to load this archive file dynamically
	    if ( ($Config{'dlsrc'} =~ /dl_next/ && $Config{'osvers'} lt '4_0')
	    ||   ($Config{'dlsrc'} =~ /dl_dld/) )
	    {
		# We push -l$thislib instead of $fullname because
		# it avoids hardwiring a fixed path into the .bs file.
		# Mkbootstrap will automatically add dl_findfile() to
		# the .bs file if it sees a name in the -l format.
		# USE THIS, when dl_findfile() is fixed: 
		# push(@bsloadlibs, "-l$thislib");
		# OLD USE WAS while checking results against old_extliblist
		push(@bsloadlibs, "$fullname");
	    } else {
		if ($is_dyna){
                    # For SunOS4, do not add in this shared library if
                    # it is already linked in the main perl executable
		    push(@ldloadlibs, "-l$thislib")
			unless ($in_perl and $^O eq 'sunos');
		} else {
		    push(@ldloadlibs, "-l$thislib");
		}
	    }
	    last;	# found one here so don't bother looking further
	}
	warn "Note (probably harmless): "
		     ."No library found for -l$thislib\n"
	    unless $found_lib>0;
    }
    return ('','','','', ($give_libs ? \@libs : ())) unless $found;
    ("@extralibs", "@bsloadlibs", "@ldloadlibs",join(":",@ld_run_path), ($give_libs ? \@libs : ()));
}

sub _win32_ext {

    require Text::ParseWords;

    my($self, $potential_libs, $verbose, $give_libs) = @_;

    # If user did not supply a list, we punt.
    # (caller should probably use the list in $Config{libs})
    return ("", "", "", "", ($give_libs ? [] : ())) unless $potential_libs;

    my $cc		= $Config{cc};
    my $VC		= 1 if $cc =~ /^cl/i;
    my $BC		= 1 if $cc =~ /^bcc/i;
    my $GC		= 1 if $cc =~ /^gcc/i;
    my $so		= $Config{'so'};
    my $libs		= $Config{'perllibs'};
    my $libpth		= $Config{'libpth'};
    my $libext		= $Config{'lib_ext'} || ".lib";
    my(@libs, %libs_seen);

    if ($libs and $potential_libs !~ /:nodefault/i) { 
	# If Config.pm defines a set of default libs, we always
	# tack them on to the user-supplied list, unless the user
	# specified :nodefault

	$potential_libs .= " " if $potential_libs;
	$potential_libs .= $libs;
    }
    warn "Potential libraries are '$potential_libs':\n" if $verbose;

    # normalize to forward slashes
    $libpth =~ s,\\,/,g;
    $potential_libs =~ s,\\,/,g;

    # compute $extralibs from $potential_libs

    my @searchpath;		    # from "-L/path" in $potential_libs
    my @libpath		= Text::ParseWords::quotewords('\s+', 0, $libpth);
    my @extralibs;
    my $pwd		= cwd();    # from Cwd.pm
    my $lib		= '';
    my $found		= 0;
    my $search		= 1;
    my($fullname, $thislib, $thispth);

    # add "$Config{installarchlib}/CORE" to default search path
    push @libpath, "$Config{installarchlib}/CORE";

    if ($VC and exists $ENV{LIB} and $ENV{LIB}) {
        push @libpath, split /;/, $ENV{LIB};
    }

    foreach (Text::ParseWords::quotewords('\s+', 0, $potential_libs)){

	$thislib = $_;

        # see if entry is a flag
	if (/^:\w+$/) {
	    $search	= 0 if lc eq ':nosearch';
	    $search	= 1 if lc eq ':search';
	    warn "Ignoring unknown flag '$thislib'\n"
		if $verbose and !/^:(no)?(search|default)$/i;
	    next;
	}

	# if searching is disabled, do compiler-specific translations
	unless ($search) {
	    s/^-l(.+)$/$1.lib/ unless $GC;
	    s/^-L/-libpath:/ if $VC;
	    push(@extralibs, $_);
	    $found++;
	    next;
	}

	# handle possible linker path arguments
	if (s/^-L// and not -d) {
	    warn "$thislib ignored, directory does not exist\n"
		if $verbose;
	    next;
	}
	elsif (-d) {
	    unless ($self->file_name_is_absolute($_)) {
	      warn "Warning: '$thislib' changed to '-L$pwd/$_'\n";
	      $_ = $self->catdir($pwd,$_);
	    }
	    push(@searchpath, $_);
	    next;
	}

	# handle possible library arguments
	if (s/^-l// and $GC and !/^lib/i) {
	    $_ = "lib$_";
	}
	$_ .= $libext if !/\Q$libext\E$/i;

	my $secondpass = 0;
    LOOKAGAIN:

        # look for the file itself
	if (-f) {
	    warn "'$thislib' found as '$_'\n" if $verbose;
	    $found++;
	    push(@extralibs, $_);
	    next;
	}

	my $found_lib = 0;
	foreach $thispth (@searchpath, @libpath){
	    unless (-f ($fullname="$thispth\\$_")) {
		warn "'$thislib' not found as '$fullname'\n" if $verbose;
		next;
	    }
	    warn "'$thislib' found as '$fullname'\n" if $verbose;
	    $found++;
	    $found_lib++;
	    push(@extralibs, $fullname);
	    push @libs, $fullname unless $libs_seen{$fullname}++;
	    last;
	}

	# do another pass with (or without) leading 'lib' if they used -l
	if (!$found_lib and $thislib =~ /^-l/ and !$secondpass++) {
	    if ($GC) {
		goto LOOKAGAIN if s/^lib//i;
	    }
	    elsif (!/^lib/i) {
		$_ = "lib$_";
		goto LOOKAGAIN;
	    }
	}

	# give up
	warn "Note (probably harmless): "
		     ."No library found for '$thislib'\n"
	    unless $found_lib>0;

    }

    return ('','','','', ($give_libs ? \@libs : ())) unless $found;

    # make sure paths with spaces are properly quoted
    @extralibs = map { (/\s/ && !/^".*"$/) ? qq["$_"] : $_ } @extralibs;
    @libs = map { (/\s/ && !/^".*"$/) ? qq["$_"] : $_ } @libs;
    $lib = join(' ',@extralibs);

    # normalize back to backward slashes (to help braindead tools)
    # XXX this may break equally braindead GNU tools that don't understand
    # backslashes, either.  Seems like one can't win here.  Cursed be CP/M.
    $lib =~ s,/,\\,g;

    warn "Result: $lib\n" if $verbose;
    wantarray ? ($lib, '', $lib, '', ($give_libs ? \@libs : ())) : $lib;
}


sub _vms_ext {
  my($self, $potential_libs,$verbose,$give_libs) = @_;
  my(@crtls,$crtlstr);
  my($dbgqual) = $self->{OPTIMIZE} || $Config{'optimize'} ||
                 $self->{CCFLAS}   || $Config{'ccflags'};
  @crtls = ( ($dbgqual =~ m-/Debug-i ? $Config{'dbgprefix'} : '')
              . 'PerlShr/Share' );
  push(@crtls, grep { not /\(/ } split /\s+/, $Config{'perllibs'});
  push(@crtls, grep { not /\(/ } split /\s+/, $Config{'libc'});
  # In general, we pass through the basic libraries from %Config unchanged.
  # The one exception is that if we're building in the Perl source tree, and
  # a library spec could be resolved via a logical name, we go to some trouble
  # to insure that the copy in the local tree is used, rather than one to
  # which a system-wide logical may point.
  if ($self->{PERL_SRC}) {
    my($lib,$locspec,$type);
    foreach $lib (@crtls) { 
      if (($locspec,$type) = $lib =~ m-^([\w$\-]+)(/\w+)?- and $locspec =~ /perl/i) {
        if    (lc $type eq '/share')   { $locspec .= $Config{'exe_ext'}; }
        elsif (lc $type eq '/library') { $locspec .= $Config{'lib_ext'}; }
        else                           { $locspec .= $Config{'obj_ext'}; }
        $locspec = $self->catfile($self->{PERL_SRC},$locspec);
        $lib = "$locspec$type" if -e $locspec;
      }
    }
  }
  $crtlstr = @crtls ? join(' ',@crtls) : '';

  unless ($potential_libs) {
    warn "Result:\n\tEXTRALIBS: \n\tLDLOADLIBS: $crtlstr\n" if $verbose;
    return ('', '', $crtlstr, '', ($give_libs ? [] : ()));
  }

  my(@dirs,@libs,$dir,$lib,%found,@fndlibs,$ldlib);
  my $cwd = cwd();
  my($so,$lib_ext,$obj_ext) = @Config{'so','lib_ext','obj_ext'};
  # List of common Unix library names and there VMS equivalents
  # (VMS equivalent of '' indicates that the library is automatially
  # searched by the linker, and should be skipped here.)
  my(@flibs, %libs_seen);
  my %libmap = ( 'm' => '', 'f77' => '', 'F77' => '', 'V77' => '', 'c' => '',
                 'malloc' => '', 'crypt' => '', 'resolv' => '', 'c_s' => '',
                 'socket' => '', 'X11' => 'DECW$XLIBSHR',
                 'Xt' => 'DECW$XTSHR', 'Xm' => 'DECW$XMLIBSHR',
                 'Xmu' => 'DECW$XMULIBSHR');
  if ($Config{'vms_cc_type'} ne 'decc') { $libmap{'curses'} = 'VAXCCURSE'; }

  warn "Potential libraries are '$potential_libs'\n" if $verbose;

  # First, sort out directories and library names in the input
  foreach $lib (split ' ',$potential_libs) {
    push(@dirs,$1),   next if $lib =~ /^-L(.*)/;
    push(@dirs,$lib), next if $lib =~ /[:>\]]$/;
    push(@dirs,$lib), next if -d $lib;
    push(@libs,$1),   next if $lib =~ /^-l(.*)/;
    push(@libs,$lib);
  }
  push(@dirs,split(' ',$Config{'libpth'}));

  # Now make sure we've got VMS-syntax absolute directory specs
  # (We don't, however, check whether someone's hidden a relative
  # path in a logical name.)
  foreach $dir (@dirs) {
    unless (-d $dir) {
      warn "Skipping nonexistent Directory $dir\n" if $verbose > 1;
      $dir = '';
      next;
    }
    warn "Resolving directory $dir\n" if $verbose;
    if ($self->file_name_is_absolute($dir)) { $dir = $self->fixpath($dir,1); }
    else                                    { $dir = $self->catdir($cwd,$dir); }
  }
  @dirs = grep { length($_) } @dirs;
  unshift(@dirs,''); # Check each $lib without additions first

  LIB: foreach $lib (@libs) {
    if (exists $libmap{$lib}) {
      next unless length $libmap{$lib};
      $lib = $libmap{$lib};
    }

    my(@variants,$variant,$name,$test,$cand);
    my($ctype) = '';

    # If we don't have a file type, consider it a possibly abbreviated name and
    # check for common variants.  We try these first to grab libraries before
    # a like-named executable image (e.g. -lperl resolves to perlshr.exe
    # before perl.exe).
    if ($lib !~ /\.[^:>\]]*$/) {
      push(@variants,"${lib}shr","${lib}rtl","${lib}lib");
      push(@variants,"lib$lib") if $lib !~ /[:>\]]/;
    }
    push(@variants,$lib);
    warn "Looking for $lib\n" if $verbose;
    foreach $variant (@variants) {
      foreach $dir (@dirs) {
        my($type);

        $name = "$dir$variant";
        warn "\tChecking $name\n" if $verbose > 2;
        if (-f ($test = VMS::Filespec::rmsexpand($name))) {
          # It's got its own suffix, so we'll have to figure out the type
          if    ($test =~ /(?:$so|exe)$/i)      { $type = 'SHR'; }
          elsif ($test =~ /(?:$lib_ext|olb)$/i) { $type = 'OLB'; }
          elsif ($test =~ /(?:$obj_ext|obj)$/i) {
            warn "Note (probably harmless): "
			 ."Plain object file $test found in library list\n";
            $type = 'OBJ';
          }
          else {
            warn "Note (probably harmless): "
			 ."Unknown library type for $test; assuming shared\n";
            $type = 'SHR';
          }
        }
        elsif (-f ($test = VMS::Filespec::rmsexpand($name,$so))      or
               -f ($test = VMS::Filespec::rmsexpand($name,'.exe')))     {
          $type = 'SHR';
          $name = $test unless $test =~ /exe;?\d*$/i;
        }
        elsif (not length($ctype) and  # If we've got a lib already, don't bother
               ( -f ($test = VMS::Filespec::rmsexpand($name,$lib_ext)) or
                 -f ($test = VMS::Filespec::rmsexpand($name,'.olb'))))  {
          $type = 'OLB';
          $name = $test unless $test =~ /olb;?\d*$/i;
        }
        elsif (not length($ctype) and  # If we've got a lib already, don't bother
               ( -f ($test = VMS::Filespec::rmsexpand($name,$obj_ext)) or
                 -f ($test = VMS::Filespec::rmsexpand($name,'.obj'))))  {
          warn "Note (probably harmless): "
		       ."Plain object file $test found in library list\n";
          $type = 'OBJ';
          $name = $test unless $test =~ /obj;?\d*$/i;
        }
        if (defined $type) {
          $ctype = $type; $cand = $name;
          last if $ctype eq 'SHR';
        }
      }
      if ($ctype) { 
        # This has to precede any other CRTLs, so just make it first
        if ($cand eq 'VAXCCURSE') { unshift @{$found{$ctype}}, $cand; }  
        else                      { push    @{$found{$ctype}}, $cand; }
        warn "\tFound as $cand (really $test), type $ctype\n" if $verbose > 1;
	push @flibs, $name unless $libs_seen{$fullname}++;
        next LIB;
      }
    }
    warn "Note (probably harmless): "
		 ."No library found for $lib\n";
  }

  push @fndlibs, @{$found{OBJ}}                      if exists $found{OBJ};
  push @fndlibs, map { "$_/Library" } @{$found{OLB}} if exists $found{OLB};
  push @fndlibs, map { "$_/Share"   } @{$found{SHR}} if exists $found{SHR};
  $lib = join(' ',@fndlibs);

  $ldlib = $crtlstr ? "$lib $crtlstr" : $lib;
  warn "Result:\n\tEXTRALIBS: $lib\n\tLDLOADLIBS: $ldlib\n" if $verbose;
  wantarray ? ($lib, '', $ldlib, '', ($give_libs ? \@flibs : ())) : $lib;
}

1;

__END__

=head1 NAME

ExtUtils::Liblist - determine libraries to use and how to use them

=head1 SYNOPSIS

C<require ExtUtils::Liblist;>

C<ExtUtils::Liblist::ext($self, $potential_libs, $verbose, $need_names);>

=head1 DESCRIPTION

This utility takes a list of libraries in the form C<-llib1 -llib2
-llib3> and returns lines suitable for inclusion in an extension
Makefile.  Extra library paths may be included with the form
C<-L/another/path> this will affect the searches for all subsequent
libraries.

It returns an array of four or five scalar values: EXTRALIBS,
BSLOADLIBS, LDLOADLIBS, LD_RUN_PATH, and, optionally, a reference to
the array of the filenames of actual libraries.  Some of these don't
mean anything unless on Unix.  See the details about those platform
specifics below.  The list of the filenames is returned only if
$need_names argument is true.

Dependent libraries can be linked in one of three ways:

=over 2

=item * For static extensions

by the ld command when the perl binary is linked with the extension
library. See EXTRALIBS below.

=item * For dynamic extensions

by the ld command when the shared object is built/linked. See
LDLOADLIBS below.

=item * For dynamic extensions

by the DynaLoader when the shared object is loaded. See BSLOADLIBS
below.

=back

=head2 EXTRALIBS

List of libraries that need to be linked with when linking a perl
binary which includes this extension. Only those libraries that
actually exist are included.  These are written to a file and used
when linking perl.

=head2 LDLOADLIBS and LD_RUN_PATH

List of those libraries which can or must be linked into the shared
library when created using ld. These may be static or dynamic
libraries.  LD_RUN_PATH is a colon separated list of the directories
in LDLOADLIBS. It is passed as an environment variable to the process
that links the shared library.

=head2 BSLOADLIBS

List of those libraries that are needed but can be linked in
dynamically at run time on this platform.  SunOS/Solaris does not need
this because ld records the information (from LDLOADLIBS) into the
object file.  This list is used to create a .bs (bootstrap) file.

=head1 PORTABILITY

This module deals with a lot of system dependencies and has quite a
few architecture specific C<if>s in the code.

=head2 VMS implementation

The version of ext() which is executed under VMS differs from the
Unix-OS/2 version in several respects:

=over 2

=item *

Input library and path specifications are accepted with or without the
C<-l> and C<-L> prefixes used by Unix linkers.  If neither prefix is
present, a token is considered a directory to search if it is in fact
a directory, and a library to search for otherwise.  Authors who wish
their extensions to be portable to Unix or OS/2 should use the Unix
prefixes, since the Unix-OS/2 version of ext() requires them.

=item *

Wherever possible, shareable images are preferred to object libraries,
and object libraries to plain object files.  In accordance with VMS
naming conventions, ext() looks for files named I<lib>shr and I<lib>rtl;
it also looks for I<lib>lib and libI<lib> to accommodate Unix conventions
used in some ported software.

=item *

For each library that is found, an appropriate directive for a linker options
file is generated.  The return values are space-separated strings of
these directives, rather than elements used on the linker command line.

=item *

LDLOADLIBS contains both the libraries found based on C<$potential_libs> and
the CRTLs, if any, specified in Config.pm.  EXTRALIBS contains just those
libraries found based on C<$potential_libs>.  BSLOADLIBS and LD_RUN_PATH
are always empty.

=back

In addition, an attempt is made to recognize several common Unix library
names, and filter them out or convert them to their VMS equivalents, as
appropriate.

In general, the VMS version of ext() should properly handle input from
extensions originally designed for a Unix or VMS environment.  If you
encounter problems, or discover cases where the search could be improved,
please let us know.

=head2 Win32 implementation

The version of ext() which is executed under Win32 differs from the
Unix-OS/2 version in several respects:

=over 2

=item *

If C<$potential_libs> is empty, the return value will be empty.
Otherwise, the libraries specified by C<$Config{perllibs}> (see Config.pm)
will be appended to the list of C<$potential_libs>.  The libraries
will be searched for in the directories specified in C<$potential_libs>,
C<$Config{libpth}>, and in C<$Config{installarchlib}/CORE>.
For each library that is found,  a space-separated list of fully qualified
library pathnames is generated.

=item *

Input library and path specifications are accepted with or without the
C<-l> and C<-L> prefixes used by Unix linkers.

An entry of the form C<-La:\foo> specifies the C<a:\foo> directory to look
for the libraries that follow.

An entry of the form C<-lfoo> specifies the library C<foo>, which may be
spelled differently depending on what kind of compiler you are using.  If
you are using GCC, it gets translated to C<libfoo.a>, but for other win32
compilers, it becomes C<foo.lib>.  If no files are found by those translated
names, one more attempt is made to find them using either C<foo.a> or
C<libfoo.lib>, depending on whether GCC or some other win32 compiler is
being used, respectively.

If neither the C<-L> or C<-l> prefix is present in an entry, the entry is
considered a directory to search if it is in fact a directory, and a
library to search for otherwise.  The C<$Config{lib_ext}> suffix will
be appended to any entries that are not directories and don't already have
the suffix.

Note that the C<-L> and C<-l> prefixes are B<not required>, but authors
who wish their extensions to be portable to Unix or OS/2 should use the
prefixes, since the Unix-OS/2 version of ext() requires them.

=item *

Entries cannot be plain object files, as many Win32 compilers will
not handle object files in the place of libraries.

=item *

Entries in C<$potential_libs> beginning with a colon and followed by
alphanumeric characters are treated as flags.  Unknown flags will be ignored.

An entry that matches C</:nodefault/i> disables the appending of default
libraries found in C<$Config{perllibs}> (this should be only needed very rarely).

An entry that matches C</:nosearch/i> disables all searching for
the libraries specified after it.  Translation of C<-Lfoo> and
C<-lfoo> still happens as appropriate (depending on compiler being used,
as reflected by C<$Config{cc}>), but the entries are not verified to be
valid files or directories.

An entry that matches C</:search/i> reenables searching for
the libraries specified after it.  You can put it at the end to
enable searching for default libraries specified by C<$Config{perllibs}>.

=item *

The libraries specified may be a mixture of static libraries and
import libraries (to link with DLLs).  Since both kinds are used
pretty transparently on the Win32 platform, we do not attempt to
distinguish between them.

=item *

LDLOADLIBS and EXTRALIBS are always identical under Win32, and BSLOADLIBS
and LD_RUN_PATH are always empty (this may change in future).

=item *

You must make sure that any paths and path components are properly
surrounded with double-quotes if they contain spaces. For example,
C<$potential_libs> could be (literally):

	"-Lc:\Program Files\vc\lib" msvcrt.lib "la test\foo bar.lib"

Note how the first and last entries are protected by quotes in order
to protect the spaces.

=item *

Since this module is most often used only indirectly from extension
C<Makefile.PL> files, here is an example C<Makefile.PL> entry to add
a library to the build process for an extension:

        LIBS => ['-lgl']

When using GCC, that entry specifies that MakeMaker should first look
for C<libgl.a> (followed by C<gl.a>) in all the locations specified by
C<$Config{libpth}>.

When using a compiler other than GCC, the above entry will search for
C<gl.lib> (followed by C<libgl.lib>).

If the library happens to be in a location not in C<$Config{libpth}>,
you need:

        LIBS => ['-Lc:\gllibs -lgl']

Here is a less often used example:

        LIBS => ['-lgl', ':nosearch -Ld:\mesalibs -lmesa -luser32']

This specifies a search for library C<gl> as before.  If that search
fails to find the library, it looks at the next item in the list. The
C<:nosearch> flag will prevent searching for the libraries that follow,
so it simply returns the value as C<-Ld:\mesalibs -lmesa -luser32>,
since GCC can use that value as is with its linker.

When using the Visual C compiler, the second item is returned as
C<-libpath:d:\mesalibs mesa.lib user32.lib>.

When using the Borland compiler, the second item is returned as
C<-Ld:\mesalibs mesa.lib user32.lib>, and MakeMaker takes care of
moving the C<-Ld:\mesalibs> to the correct place in the linker
command line.

=back


=head1 SEE ALSO

L<ExtUtils::MakeMaker>

=cut


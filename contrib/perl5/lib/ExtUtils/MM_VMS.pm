#   MM_VMS.pm
#   MakeMaker default methods for VMS
#   This package is inserted into @ISA of MakeMaker's MM before the
#   built-in ExtUtils::MM_Unix methods if MakeMaker.pm is run under VMS.
#
#   Author:  Charles Bailey  bailey@newman.upenn.edu

package ExtUtils::MM_VMS;

use Carp qw( &carp );
use Config;
require Exporter;
use VMS::Filespec;
use File::Basename;

use vars qw($Revision);
$Revision = '5.52 (12-Sep-1998)';

unshift @MM::ISA, 'ExtUtils::MM_VMS';

Exporter::import('ExtUtils::MakeMaker', '$Verbose', '&neatvalue');

=head1 NAME

ExtUtils::MM_VMS - methods to override UN*X behaviour in ExtUtils::MakeMaker

=head1 SYNOPSIS

 use ExtUtils::MM_VMS; # Done internally by ExtUtils::MakeMaker if needed

=head1 DESCRIPTION

See ExtUtils::MM_Unix for a documentation of the methods provided
there. This package overrides the implementation of these methods, not
the semantics.

=head2 Methods always loaded

=over

=item eliminate_macros

Expands MM[KS]/Make macros in a text string, using the contents of
identically named elements of C<%$self>, and returns the result
as a file specification in Unix syntax.

=cut

sub eliminate_macros {
    my($self,$path) = @_;
    unless ($path) {
	print "eliminate_macros('') = ||\n" if $Verbose >= 3;
	return '';
    }
    my($npath) = unixify($path);
    my($complex) = 0;
    my($head,$macro,$tail);

    # perform m##g in scalar context so it acts as an iterator
    while ($npath =~ m#(.*?)\$\((\S+?)\)(.*)#g) { 
        if ($self->{$2}) {
            ($head,$macro,$tail) = ($1,$2,$3);
            if (ref $self->{$macro}) {
                if (ref $self->{$macro} eq 'ARRAY') {
                    print "Note: expanded array macro \$($macro) in $path\n" if $Verbose;
                    $macro = join ' ', @{$self->{$macro}};
                }
                else {
                    print "Note: can't expand macro \$($macro) containing ",ref($self->{$macro}),
                          "\n\t(using MMK-specific deferred substitutuon; MMS will break)\n";
                    $macro = "\cB$macro\cB";
                    $complex = 1;
                }
            }
            else { ($macro = unixify($self->{$macro})) =~ s#/$##; }
            $npath = "$head$macro$tail";
        }
    }
    if ($complex) { $npath =~ s#\cB(.*?)\cB#\${$1}#g; }
    print "eliminate_macros($path) = |$npath|\n" if $Verbose >= 3;
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
    unless ($path) {
	print "eliminate_macros('') = ||\n" if $Verbose >= 3;
	return '';
    }
    my($fixedpath,$prefix,$name);

    if ($path =~ m#^\$\([^\)]+\)$# || $path =~ m#[/:>\]]#) { 
        if ($force_path or $path =~ /(?:DIR\)|\])$/) {
            $fixedpath = vmspath($self->eliminate_macros($path));
        }
        else {
            $fixedpath = vmsify($self->eliminate_macros($path));
        }
    }
    elsif ((($prefix,$name) = ($path =~ m#^\$\(([^\)]+)\)(.+)#)) && $self->{$prefix}) {
        my($vmspre) = $self->eliminate_macros("\$($prefix)");
        # is it a dir or just a name?
        $vmspre = ($vmspre =~ m|/| or $prefix =~ /DIR$/) ? vmspath($vmspre) : '';
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
    print "fixpath($path) = |$fixedpath|\n" if $Verbose >= 3;
    $fixedpath;
}

=item catdir

Concatenates a list of file specifications, and returns the result as a
VMS-syntax directory specification.

=cut

sub catdir {
    my($self,@dirs) = @_;
    my($dir) = pop @dirs;
    @dirs = grep($_,@dirs);
    my($rslt);
    if (@dirs) {
      my($path) = (@dirs == 1 ? $dirs[0] : $self->catdir(@dirs));
      my($spath,$sdir) = ($path,$dir);
      $spath =~ s/.dir$//; $sdir =~ s/.dir$//; 
      $sdir = $self->eliminate_macros($sdir) unless $sdir =~ /^[\w\-]+$/;
      $rslt = $self->fixpath($self->eliminate_macros($spath)."/$sdir",1);
    }
    else { 
      if ($dir =~ /^\$\([^\)]+\)$/) { $rslt = $dir; }
      else                          { $rslt = vmspath($dir); }
    }
    print "catdir(",join(',',@_[1..$#_]),") = |$rslt|\n" if $Verbose >= 3;
    $rslt;
}

=item catfile

Concatenates a list of file specifications, and returns the result as a
VMS-syntax directory specification.

=cut

sub catfile {
    my($self,@files) = @_;
    my($file) = pop @files;
    @files = grep($_,@files);
    my($rslt);
    if (@files) {
      my($path) = (@files == 1 ? $files[0] : $self->catdir(@files));
      my($spath) = $path;
      $spath =~ s/.dir$//;
      if ( $spath =~ /^[^\)\]\/:>]+\)$/ && basename($file) eq $file) { $rslt = "$spath$file"; }
      else {
          $rslt = $self->eliminate_macros($spath);
          $rslt = vmsify($rslt.($rslt ? '/' : '').unixify($file));
      }
    }
    else { $rslt = vmsify($file); }
    print "catfile(",join(',',@_[1..$#_]),") = |$rslt|\n" if $Verbose >= 3;
    $rslt;
}

=item wraplist

Converts a list into a string wrapped at approximately 80 columns.

=cut

sub wraplist {
    my($self) = shift;
    my($line,$hlen) = ('',0);
    my($word);

    foreach $word (@_) {
      # Perl bug -- seems to occasionally insert extra elements when
      # traversing array (scalar(@array) doesn't show them, but
      # foreach(@array) does) (5.00307)
      next unless $word =~ /\w/;
      $line .= ' ' if length($line);
      if ($hlen > 80) { $line .= "\\\n\t"; $hlen = 0; }
      $line .= $word;
      $hlen += length($word) + 2;
    }
    $line;
}

=item curdir (override)

Returns a string representing of the current directory.

=cut

sub curdir {
    return '[]';
}

=item rootdir (override)

Returns a string representing of the root directory.

=cut

sub rootdir {
    return '';
}

=item updir (override)

Returns a string representing of the parent directory.

=cut

sub updir {
    return '[-]';
}

package ExtUtils::MM_VMS;

sub ExtUtils::MM_VMS::ext;
sub ExtUtils::MM_VMS::guess_name;
sub ExtUtils::MM_VMS::find_perl;
sub ExtUtils::MM_VMS::path;
sub ExtUtils::MM_VMS::maybe_command;
sub ExtUtils::MM_VMS::maybe_command_in_dirs;
sub ExtUtils::MM_VMS::perl_script;
sub ExtUtils::MM_VMS::file_name_is_absolute;
sub ExtUtils::MM_VMS::replace_manpage_separator;
sub ExtUtils::MM_VMS::init_others;
sub ExtUtils::MM_VMS::constants;
sub ExtUtils::MM_VMS::cflags;
sub ExtUtils::MM_VMS::const_cccmd;
sub ExtUtils::MM_VMS::pm_to_blib;
sub ExtUtils::MM_VMS::tool_autosplit;
sub ExtUtils::MM_VMS::tool_xsubpp;
sub ExtUtils::MM_VMS::xsubpp_version;
sub ExtUtils::MM_VMS::tools_other;
sub ExtUtils::MM_VMS::dist;
sub ExtUtils::MM_VMS::c_o;
sub ExtUtils::MM_VMS::xs_c;
sub ExtUtils::MM_VMS::xs_o;
sub ExtUtils::MM_VMS::top_targets;
sub ExtUtils::MM_VMS::dlsyms;
sub ExtUtils::MM_VMS::dynamic_lib;
sub ExtUtils::MM_VMS::dynamic_bs;
sub ExtUtils::MM_VMS::static_lib;
sub ExtUtils::MM_VMS::manifypods;
sub ExtUtils::MM_VMS::processPL;
sub ExtUtils::MM_VMS::installbin;
sub ExtUtils::MM_VMS::subdir_x;
sub ExtUtils::MM_VMS::clean;
sub ExtUtils::MM_VMS::realclean;
sub ExtUtils::MM_VMS::dist_basics;
sub ExtUtils::MM_VMS::dist_core;
sub ExtUtils::MM_VMS::dist_dir;
sub ExtUtils::MM_VMS::dist_test;
sub ExtUtils::MM_VMS::install;
sub ExtUtils::MM_VMS::perldepend;
sub ExtUtils::MM_VMS::makefile;
sub ExtUtils::MM_VMS::test;
sub ExtUtils::MM_VMS::test_via_harness;
sub ExtUtils::MM_VMS::test_via_script;
sub ExtUtils::MM_VMS::makeaperl;
sub ExtUtils::MM_VMS::ext;
sub ExtUtils::MM_VMS::nicetext;

#use SelfLoader;
sub AUTOLOAD {
    my $code;
    if (defined fileno(DATA)) {
	my $fh = select DATA;
	my $o = $/;			# For future reads from the file.
	$/ = "\n__END__\n";
	$code = <DATA>;
	$/ = $o;
	select $fh;
	close DATA;
	eval $code;
	if ($@) {
	    $@ =~ s/ at .*\n//;
	    Carp::croak $@;
	}
    } else {
	warn "AUTOLOAD called unexpectedly for $AUTOLOAD"; 
    }
    defined(&$AUTOLOAD) or die "Myloader inconsistency error";
    goto &$AUTOLOAD;
}

1;

#__DATA__


# This isn't really an override.  It's just here because ExtUtils::MM_VMS
# appears in @MM::ISA before ExtUtils::Liblist, so if there isn't an ext()
# in MM_VMS, then AUTOLOAD is called, and bad things happen.  So, we just
# mimic inheritance here and hand off to ExtUtils::Liblist.
sub ext {
  ExtUtils::Liblist::ext(@_);
}

=back

=head2 SelfLoaded methods

Those methods which override default MM_Unix methods are marked
"(override)", while methods unique to MM_VMS are marked "(specific)".
For overridden methods, documentation is limited to an explanation
of why this method overrides the MM_Unix method; see the ExtUtils::MM_Unix
documentation for more details.

=over

=item guess_name (override)

Try to determine name of extension being built.  We begin with the name
of the current directory.  Since VMS filenames are case-insensitive,
however, we look for a F<.pm> file whose name matches that of the current
directory (presumably the 'main' F<.pm> file for this extension), and try
to find a C<package> statement from which to obtain the Mixed::Case
package name.

=cut

sub guess_name {
    my($self) = @_;
    my($defname,$defpm,@pm,%xs,$pm);
    local *PM;

    $defname = basename(fileify($ENV{'DEFAULT'}));
    $defname =~ s![\d\-_]*\.dir.*$!!;  # Clip off .dir;1 suffix, and package version
    $defpm = $defname;
    # Fallback in case for some reason a user has copied the files for an
    # extension into a working directory whose name doesn't reflect the
    # extension's name.  We'll use the name of a unique .pm file, or the
    # first .pm file with a matching .xs file.
    if (not -e "${defpm}.pm") {
      @pm = map { s/.pm$//; $_ } glob('*.pm');
      if (@pm == 1) { ($defpm = $pm[0]) =~ s/.pm$//; }
      elsif (@pm) {
        %xs = map { s/.xs$//; ($_,1) } glob('*.xs');
        if (%xs) { foreach $pm (@pm) { $defpm = $pm, last if exists $xs{$pm}; } }
      }
    }
    if (open(PM,"${defpm}.pm")){
        while (<PM>) {
            if (/^\s*package\s+([^;]+)/i) {
                $defname = $1;
                last;
            }
        }
        print STDOUT "Warning (non-fatal): Couldn't find package name in ${defpm}.pm;\n\t",
                     "defaulting package name to $defname\n"
            if eof(PM);
        close PM;
    }
    else {
        print STDOUT "Warning (non-fatal): Couldn't find ${defpm}.pm;\n\t",
                     "defaulting package name to $defname\n";
    }
    $defname =~ s#[\d.\-_]+$##;
    $defname;
}

=item find_perl (override)

Use VMS file specification syntax and CLI commands to find and
invoke Perl images.

=cut

sub find_perl {
    my($self, $ver, $names, $dirs, $trace) = @_;
    my($name,$dir,$vmsfile,@sdirs,@snames,@cand);
    my($inabs) = 0;
    # Check in relative directories first, so we pick up the current
    # version of Perl if we're running MakeMaker as part of the main build.
    @sdirs = sort { my($absa) = $self->file_name_is_absolute($a);
                    my($absb) = $self->file_name_is_absolute($b);
                    if ($absa && $absb) { return $a cmp $b }
                    else { return $absa ? 1 : ($absb ? -1 : ($a cmp $b)); }
                  } @$dirs;
    # Check miniperl before perl, and check names likely to contain
    # version numbers before "generic" names, so we pick up an
    # executable that's less likely to be from an old installation.
    @snames = sort { my($ba) = $a =~ m!([^:>\]/]+)$!;  # basename
                     my($bb) = $b =~ m!([^:>\]/]+)$!;
                     my($ahasdir) = (length($a) - length($ba) > 0);
                     my($bhasdir) = (length($b) - length($bb) > 0);
                     if    ($ahasdir and not $bhasdir) { return 1; }
                     elsif ($bhasdir and not $ahasdir) { return -1; }
                     else { $bb =~ /\d/ <=> $ba =~ /\d/
                            or substr($ba,0,1) cmp substr($bb,0,1)
                            or length($bb) <=> length($ba) } } @$names;
    # Image names containing Perl version use '_' instead of '.' under VMS
    foreach $name (@snames) { $name =~ s/\.(\d+)$/_$1/; }
    if ($trace >= 2){
	print "Looking for perl $ver by these names:\n";
	print "\t@snames,\n";
	print "in these dirs:\n";
	print "\t@sdirs\n";
    }
    foreach $dir (@sdirs){
	next unless defined $dir; # $self->{PERL_SRC} may be undefined
	$inabs++ if $self->file_name_is_absolute($dir);
	if ($inabs == 1) {
	    # We've covered relative dirs; everything else is an absolute
	    # dir (probably an installed location).  First, we'll try potential
	    # command names, to see whether we can avoid a long MCR expression.
	    foreach $name (@snames) { push(@cand,$name) if $name =~ /^[\w\-\$]+$/; }
	    $inabs++; # Should happen above in next $dir, but just in case . . .
	}
	foreach $name (@snames){
	    if ($name !~ m![/:>\]]!) { push(@cand,$self->catfile($dir,$name)); }
	    else                     { push(@cand,$self->fixpath($name,0));    }
	}
    }
    foreach $name (@cand) {
	print "Checking $name\n" if ($trace >= 2);
	# If it looks like a potential command, try it without the MCR
	if ($name =~ /^[\w\-\$]+$/ &&
	    `$name -e "require $ver; print ""VER_OK\n"""` =~ /VER_OK/) {
	    print "Using PERL=$name\n" if $trace;
	    return $name;
	}
	next unless $vmsfile = $self->maybe_command($name);
	$vmsfile =~ s/;[\d\-]*$//;  # Clip off version number; we can use a newer version as well
	print "Executing $vmsfile\n" if ($trace >= 2);
	if (`MCR $vmsfile -e "require $ver; print ""VER_OK\n"""` =~ /VER_OK/) {
	    print "Using PERL=MCR $vmsfile\n" if $trace;
	    return "MCR $vmsfile";
	}
    }
    print STDOUT "Unable to find a perl $ver (by these names: @$names, in these dirs: @$dirs)\n";
    0; # false and not empty
}

=item path (override)

Translate logical name DCL$PATH as a searchlist, rather than trying
to C<split> string value of C<$ENV{'PATH'}>.

=cut

sub path {
    my(@dirs,$dir,$i);
    while ($dir = $ENV{'DCL$PATH;' . $i++}) { push(@dirs,$dir); }
    @dirs;
}

=item maybe_command (override)

Follows VMS naming conventions for executable files.
If the name passed in doesn't exactly match an executable file,
appends F<.Exe> (or equivalent) to check for executable image, and F<.Com>
to check for DCL procedure.  If this fails, checks directories in DCL$PATH
and finally F<Sys$System:> for an executable file having the name specified,
with or without the F<.Exe>-equivalent suffix.

=cut

sub maybe_command {
    my($self,$file) = @_;
    return $file if -x $file && ! -d _;
    my(@dirs) = ('');
    my(@exts) = ('',$Config{'exe_ext'},'.exe','.com');
    my($dir,$ext);
    if ($file !~ m![/:>\]]!) {
	for (my $i = 0; defined $ENV{"DCL\$PATH;$i"}; $i++) {
	    $dir = $ENV{"DCL\$PATH;$i"};
	    $dir .= ':' unless $dir =~ m%[\]:]$%;
	    push(@dirs,$dir);
	}
	push(@dirs,'Sys$System:');
	foreach $dir (@dirs) {
	    my $sysfile = "$dir$file";
	    foreach $ext (@exts) {
		return $file if -x "$sysfile$ext" && ! -d _;
	    }
	}
    }
    return 0;
}

=item maybe_command_in_dirs (override)

Uses DCL argument quoting on test command line.

=cut

sub maybe_command_in_dirs {	# $ver is optional argument if looking for perl
    my($self, $names, $dirs, $trace, $ver) = @_;
    my($name, $dir);
    foreach $dir (@$dirs){
	next unless defined $dir; # $self->{PERL_SRC} may be undefined
	foreach $name (@$names){
	    my($abs,$tryabs);
	    if ($self->file_name_is_absolute($name)) {
		$abs = $name;
	    } else {
		$abs = $self->catfile($dir, $name);
	    }
	    print "Checking $abs for $name\n" if ($trace >= 2);
	    next unless $tryabs = $self->maybe_command($abs);
	    print "Substituting $tryabs instead of $abs\n" 
		if ($trace >= 2 and $tryabs ne $abs);
	    $abs = $tryabs;
	    if (defined $ver) {
		print "Executing $abs\n" if ($trace >= 2);
		if (`$abs -e 'require $ver; print "VER_OK\n" ' 2>&1` =~ /VER_OK/) {
		    print "Using $abs\n" if $trace;
		    return $abs;
		}
	    } else { # Do not look for perl
		return $abs;
	    }
	}
    }
}

=item perl_script (override)

If name passed in doesn't specify a readable file, appends F<.com> or
F<.pl> and tries again, since it's customary to have file types on all files
under VMS.

=cut

sub perl_script {
    my($self,$file) = @_;
    return $file if -r $file && ! -d _;
    return "$file.com" if -r "$file.com";
    return "$file.pl" if -r "$file.pl";
    return '';
}

=item file_name_is_absolute (override)

Checks for VMS directory spec as well as Unix separators.

=cut

sub file_name_is_absolute {
    my($self,$file) = @_;
    # If it's a logical name, expand it.
    $file = $ENV{$file} while $file =~ /^[\w\$\-]+$/ and $ENV{$file};
    $file =~ m!^/! or $file =~ m![<\[][^.\-\]>]! or $file =~ /:[^<\[]/;
}

=item replace_manpage_separator

Use as separator a character which is legal in a VMS-syntax file name.

=cut

sub replace_manpage_separator {
    my($self,$man) = @_;
    $man = unixify($man);
    $man =~ s#/+#__#g;
    $man;
}

=item init_others (override)

Provide VMS-specific forms of various utility commands, then hand
off to the default MM_Unix method.

=cut

sub init_others {
    my($self) = @_;

    $self->{NOOP} = 'Continue';
    $self->{FIRST_MAKEFILE} ||= 'Descrip.MMS';
    $self->{MAKE_APERL_FILE} ||= 'Makeaperl.MMS';
    $self->{MAKEFILE} ||= $self->{FIRST_MAKEFILE};
    $self->{NOECHO} ||= '@ ';
    $self->{RM_F} = '$(PERL) -e "foreach (@ARGV) { 1 while ( -d $_ ? rmdir $_ : unlink $_)}"';
    $self->{RM_RF} = '$(PERL) "-I$(PERL_LIB)" -e "use File::Path; @dirs = map(VMS::Filespec::unixify($_),@ARGV); rmtree(\@dirs,0,0)"';
    $self->{TOUCH} = '$(PERL) -e "$t=time; foreach (@ARGV) { -e $_ ? utime($t,$t,@ARGV) : (open(F,qq(>$_)),close F)}"';
    $self->{CHMOD} = '$(PERL) -e "chmod @ARGV"';  # expect Unix syntax from MakeMaker
    $self->{CP} = 'Copy/NoConfirm';
    $self->{MV} = 'Rename/NoConfirm';
    $self->{UMASK_NULL} = '! ';  
    &ExtUtils::MM_Unix::init_others;
}

=item constants (override)

Fixes up numerous file and directory macros to insure VMS syntax
regardless of input syntax.  Also adds a few VMS-specific macros
and makes lists of files comma-separated.

=cut

sub constants {
    my($self) = @_;
    my(@m,$def,$macro);

    if ($self->{DEFINE} ne '') {
	my(@defs) = split(/\s+/,$self->{DEFINE});
	foreach $def (@defs) {
	    next unless $def;
	    if ($def =~ s/^-D//) {       # If it was a Unix-style definition
		$def =~ s/='(.*)'$/=$1/;  # then remove shell-protection ''
		$def =~ s/^'(.*)'$/$1/;   # from entire term or argument
	    }
	    if ($def =~ /=/) {
		$def =~ s/"/""/g;  # Protect existing " from DCL
		$def = qq["$def"]; # and quote to prevent parsing of =
	    }
	}
	$self->{DEFINE} = join ',',@defs;
    }

    if ($self->{OBJECT} =~ /\s/) {
	$self->{OBJECT} =~ s/(\\)?\n+\s+/ /g;
	$self->{OBJECT} = $self->wraplist(map($self->fixpath($_,0),split(/,?\s+/,$self->{OBJECT})));
    }
    $self->{LDFROM} = $self->wraplist(map($self->fixpath($_,0),split(/,?\s+/,$self->{LDFROM})));


    # Fix up directory specs
    $self->{ROOTEXT} = $self->{ROOTEXT} ? $self->fixpath($self->{ROOTEXT},1)
                                        : '[]';
    foreach $macro ( qw [
            INST_BIN INST_SCRIPT INST_LIB INST_ARCHLIB INST_EXE INSTALLPRIVLIB
            INSTALLARCHLIB INSTALLSCRIPT INSTALLBIN PERL_LIB PERL_ARCHLIB
            PERL_INC PERL_SRC FULLEXT INST_MAN1DIR INSTALLMAN1DIR
            INST_MAN3DIR INSTALLMAN3DIR INSTALLSITELIB INSTALLSITEARCH
            SITELIBEXP SITEARCHEXP ] ) {
	next unless defined $self->{$macro};
	$self->{$macro} = $self->fixpath($self->{$macro},1);
    }
    $self->{PERL_VMS} = $self->catdir($self->{PERL_SRC},q(VMS))
	if ($self->{PERL_SRC});
                        


    # Fix up file specs
    foreach $macro ( qw[LIBPERL_A FIRST_MAKEFILE MAKE_APERL_FILE MYEXTLIB] ) {
	next unless defined $self->{$macro};
	$self->{$macro} = $self->fixpath($self->{$macro},0);
    }

    foreach $macro (qw/
	      AR_STATIC_ARGS NAME DISTNAME NAME_SYM VERSION VERSION_SYM XS_VERSION
	      INST_BIN INST_EXE INST_LIB INST_ARCHLIB INST_SCRIPT PREFIX
	      INSTALLDIRS INSTALLPRIVLIB  INSTALLARCHLIB INSTALLSITELIB
	      INSTALLSITEARCH INSTALLBIN INSTALLSCRIPT PERL_LIB
	      PERL_ARCHLIB SITELIBEXP SITEARCHEXP LIBPERL_A MYEXTLIB
	      FIRST_MAKEFILE MAKE_APERL_FILE PERLMAINCC PERL_SRC PERL_VMS
	      PERL_INC PERL FULLPERL
	      / ) {
	next unless defined $self->{$macro};
	push @m, "$macro = $self->{$macro}\n";
    }


    push @m, q[
VERSION_MACRO = VERSION
DEFINE_VERSION = "$(VERSION_MACRO)=""$(VERSION)"""
XS_VERSION_MACRO = XS_VERSION
XS_DEFINE_VERSION = "$(XS_VERSION_MACRO)=""$(XS_VERSION)"""

MAKEMAKER = ],$self->catfile($self->{PERL_LIB},'ExtUtils','MakeMaker.pm'),qq[
MM_VERSION = $ExtUtils::MakeMaker::VERSION
MM_REVISION = $ExtUtils::MakeMaker::Revision
MM_VMS_REVISION = $ExtUtils::MM_VMS::Revision

# FULLEXT = Pathname for extension directory (eg DBD/Oracle).
# BASEEXT = Basename part of FULLEXT. May be just equal FULLEXT.
# PARENT_NAME = NAME without BASEEXT and no trailing :: (eg Foo::Bar)
# DLBASE  = Basename part of dynamic library. May be just equal BASEEXT.
];

    for $tmp (qw/
	      FULLEXT VERSION_FROM OBJECT LDFROM
	      /	) {
	next unless defined $self->{$tmp};
	push @m, "$tmp = ",$self->fixpath($self->{$tmp},0),"\n";
    }

    for $tmp (qw/
	      BASEEXT PARENT_NAME DLBASE INC DEFINE LINKTYPE
	      /	) {
	next unless defined $self->{$tmp};
	push @m, "$tmp = $self->{$tmp}\n";
    }

    for $tmp (qw/ XS MAN1PODS MAN3PODS PM /) {
	next unless defined $self->{$tmp};
	my(%tmp,$key);
	for $key (keys %{$self->{$tmp}}) {
	    $tmp{$self->fixpath($key,0)} = $self->fixpath($self->{$tmp}{$key},0);
	}
	$self->{$tmp} = \%tmp;
    }

    for $tmp (qw/ C O_FILES H /) {
	next unless defined $self->{$tmp};
	my(@tmp,$val);
	for $val (@{$self->{$tmp}}) {
	    push(@tmp,$self->fixpath($val,0));
	}
	$self->{$tmp} = \@tmp;
    }

    push @m,'

# Handy lists of source code files:
XS_FILES = ',$self->wraplist(sort keys %{$self->{XS}}),'
C_FILES  = ',$self->wraplist(@{$self->{C}}),'
O_FILES  = ',$self->wraplist(@{$self->{O_FILES}} ),'
H_FILES  = ',$self->wraplist(@{$self->{H}}),'
MAN1PODS = ',$self->wraplist(sort keys %{$self->{MAN1PODS}}),'
MAN3PODS = ',$self->wraplist(sort keys %{$self->{MAN3PODS}}),'

';

    for $tmp (qw/
	      INST_MAN1DIR INSTALLMAN1DIR MAN1EXT INST_MAN3DIR INSTALLMAN3DIR MAN3EXT
	      /) {
	next unless defined $self->{$tmp};
	push @m, "$tmp = $self->{$tmp}\n";
    }

push @m,"
.SUFFIXES :
.SUFFIXES : \$(OBJ_EXT) .c .cpp .cxx .xs

# Here is the Config.pm that we are using/depend on
CONFIGDEP = \$(PERL_ARCHLIB)Config.pm, \$(PERL_INC)config.h \$(VERSION_FROM)

# Where to put things:
INST_LIBDIR      = $self->{INST_LIBDIR}
INST_ARCHLIBDIR  = $self->{INST_ARCHLIBDIR}

INST_AUTODIR     = $self->{INST_AUTODIR}
INST_ARCHAUTODIR = $self->{INST_ARCHAUTODIR}
";

    if ($self->has_link_code()) {
	push @m,'
INST_STATIC = $(INST_ARCHAUTODIR)$(BASEEXT)$(LIB_EXT)
INST_DYNAMIC = $(INST_ARCHAUTODIR)$(BASEEXT).$(DLEXT)
INST_BOOT = $(INST_ARCHAUTODIR)$(BASEEXT).bs
';
    } else {
	my $shr = $Config{'dbgprefix'} . 'PERLSHR';
	push @m,'
INST_STATIC =
INST_DYNAMIC =
INST_BOOT =
EXPORT_LIST = $(BASEEXT).opt
PERL_ARCHIVE = ',($ENV{$shr} ? $ENV{$shr} : "Sys\$Share:$shr.$Config{'dlext'}"),'
';
    }

    $self->{TO_INST_PM} = [ sort keys %{$self->{PM}} ];
    $self->{PM_TO_BLIB} = [ %{$self->{PM}} ];
    push @m,'
TO_INST_PM = ',$self->wraplist(@{$self->{TO_INST_PM}}),'

PM_TO_BLIB = ',$self->wraplist(@{$self->{PM_TO_BLIB}}),'
';

    join('',@m);
}

=item cflags (override)

Bypass shell script and produce qualifiers for CC directly (but warn
user if a shell script for this extension exists).  Fold multiple
/Defines into one, since some C compilers pay attention to only one
instance of this qualifier on the command line.

=cut

sub cflags {
    my($self,$libperl) = @_;
    my($quals) = $self->{CCFLAGS} || $Config{'ccflags'};
    my($definestr,$undefstr,$flagoptstr) = ('','','');
    my($incstr) = '/Include=($(PERL_INC)';
    my($name,$sys,@m);

    ( $name = $self->{NAME} . "_cflags" ) =~ s/:/_/g ;
    print STDOUT "Unix shell script ".$Config{"$self->{'BASEEXT'}_cflags"}.
         " required to modify CC command for $self->{'BASEEXT'}\n"
    if ($Config{$name});

    if ($quals =~ / -[DIUOg]/) {
	while ($quals =~ / -([Og])(\d*)\b/) {
	    my($type,$lvl) = ($1,$2);
	    $quals =~ s/ -$type$lvl\b\s*//;
	    if ($type eq 'g') { $flagoptstr = '/NoOptimize'; }
	    else { $flagoptstr = '/Optimize' . (defined($lvl) ? "=$lvl" : ''); }
	}
	while ($quals =~ / -([DIU])(\S+)/) {
	    my($type,$def) = ($1,$2);
	    $quals =~ s/ -$type$def\s*//;
	    $def =~ s/"/""/g;
	    if    ($type eq 'D') { $definestr .= qq["$def",]; }
	    elsif ($type eq 'I') { $incstr .= ',' . $self->fixpath($def,1); }
	    else                 { $undefstr  .= qq["$def",]; }
	}
    }
    if (length $quals and $quals !~ m!/!) {
	warn "MM_VMS: Ignoring unrecognized CCFLAGS elements \"$quals\"\n";
	$quals = '';
    }
    if (length $definestr) { chop($definestr); $quals .= "/Define=($definestr)"; }
    if (length $undefstr)  { chop($undefstr);  $quals .= "/Undef=($undefstr)";   }
    # Deal with $self->{DEFINE} here since some C compilers pay attention
    # to only one /Define clause on command line, so we have to
    # conflate the ones from $Config{'ccflags'} and $self->{DEFINE}
    if ($quals =~ m:(.*)/define=\(?([^\(\/\)\s]+)\)?(.*)?:i) {
	$quals = "$1/Define=($2," . ($self->{DEFINE} ? "$self->{DEFINE}," : '') .
	         "\$(DEFINE_VERSION),\$(XS_DEFINE_VERSION))$3";
    }
    else {
	$quals .= '/Define=(' . ($self->{DEFINE} ? "$self->{DEFINE}," : '') .
	          '$(DEFINE_VERSION),$(XS_DEFINE_VERSION))';
    }

    $libperl or $libperl = $self->{LIBPERL_A} || "libperl.olb";
# This whole section is commented out, since I don't think it's necessary (or applicable)
#    if ($libperl =~ s/^$Config{'dbgprefix'}//) { $libperl =~ s/perl([^Dd]*)\./perld$1./; }
#    if ($libperl =~ /libperl(\w+)\./i) {
#	my($type) = uc $1;
#	my(%map) = ( 'D'  => 'DEBUGGING', 'E' => 'EMBED', 'M' => 'MULTIPLICITY',
#	             'DE' => 'DEBUGGING,EMBED', 'DM' => 'DEBUGGING,MULTIPLICITY',
#	             'EM' => 'EMBED,MULTIPLICITY', 'DEM' => 'DEBUGGING,EMBED,MULTIPLICITY' );
#	my($add) = join(',', grep { $quals !~ /\b$_\b/ } split(/,/,$map{$type}));
#	$quals =~ s:/define=\(([^\)]+)\):/Define=($1,$add):i if $add;
#	$self->{PERLTYPE} ||= $type;
#    }

    # Likewise with $self->{INC} and /Include
    if ($self->{'INC'}) {
	my(@includes) = split(/\s+/,$self->{INC});
	foreach (@includes) {
	    s/^-I//;
	    $incstr .= ','.$self->fixpath($_,1);
	}
    }
    $quals .= "$incstr)";
    $self->{CCFLAGS} = $quals;

    $self->{OPTIMIZE} ||= $flagoptstr || $Config{'optimize'};
    if ($self->{OPTIMIZE} !~ m!/!) {
	if    ($self->{OPTIMIZE} =~ m!\b-g\b!) { $self->{OPTIMIZE} = '/Debug/NoOptimize' }
	elsif ($self->{OPTIMIZE} =~ /-O(\d*)/) {
	    $self->{OPTIMIZE} = '/Optimize' . (defined($1) ? "=$1" : '');
	}
	else {
	    warn "MM_VMS: Can't parse OPTIMIZE \"$self->{OPTIMIZE}\"; using default\n" if length $self->{OPTIMIZE};
	    $self->{OPTIMIZE} = '/Optimize';
	}
    }

    return $self->{CFLAGS} = qq{
CCFLAGS = $self->{CCFLAGS}
OPTIMIZE = $self->{OPTIMIZE}
PERLTYPE = $self->{PERLTYPE}
SPLIT =
LARGE =
};
}

=item const_cccmd (override)

Adds directives to point C preprocessor to the right place when
handling #include E<lt>sys/foo.hE<gt> directives.  Also constructs CC
command line a bit differently than MM_Unix method.

=cut

sub const_cccmd {
    my($self,$libperl) = @_;
    my(@m);

    return $self->{CONST_CCCMD} if $self->{CONST_CCCMD};
    return '' unless $self->needs_linking();
    if ($Config{'vms_cc_type'} eq 'gcc') {
        push @m,'
.FIRST
	',$self->{NOECHO},'If F$TrnLnm("Sys").eqs."" Then Define/NoLog SYS GNU_CC_Include:[VMS]';
    }
    elsif ($Config{'vms_cc_type'} eq 'vaxc') {
        push @m,'
.FIRST
	',$self->{NOECHO},'If F$TrnLnm("Sys").eqs."" .and. F$TrnLnm("VAXC$Include").eqs."" Then Define/NoLog SYS Sys$Library
	',$self->{NOECHO},'If F$TrnLnm("Sys").eqs."" .and. F$TrnLnm("VAXC$Include").nes."" Then Define/NoLog SYS VAXC$Include';
    }
    else {
        push @m,'
.FIRST
	',$self->{NOECHO},'If F$TrnLnm("Sys").eqs."" .and. F$TrnLnm("DECC$System_Include").eqs."" Then Define/NoLog SYS ',
		($Config{'arch'} eq 'VMS_AXP' ? 'Sys$Library' : 'DECC$Library_Include'),'
	',$self->{NOECHO},'If F$TrnLnm("Sys").eqs."" .and. F$TrnLnm("DECC$System_Include").nes."" Then Define/NoLog SYS DECC$System_Include';
    }

    push(@m, "\n\nCCCMD = $Config{'cc'} \$(CCFLAGS)\$(OPTIMIZE)\n");

    $self->{CONST_CCCMD} = join('',@m);
}

=item pm_to_blib (override)

DCL I<still> accepts a maximum of 255 characters on a command
line, so we write the (potentially) long list of file names
to a temp file, then persuade Perl to read it instead of the
command line to find args.

=cut

sub pm_to_blib {
    my($self) = @_;
    my($line,$from,$to,@m);
    my($autodir) = $self->catdir('$(INST_LIB)','auto');
    my(@files) = @{$self->{PM_TO_BLIB}};

    push @m, q{

# Dummy target to match Unix target name; we use pm_to_blib.ts as
# timestamp file to avoid repeated invocations under VMS
pm_to_blib : pm_to_blib.ts
	$(NOECHO) $(NOOP)

# As always, keep under DCL's 255-char limit
pm_to_blib.ts : $(TO_INST_PM)
	$(NOECHO) $(PERL) -e "print '},shift(@files),q{ },shift(@files),q{'" >.MM_tmp
};

    $line = '';  # avoid uninitialized var warning
    while ($from = shift(@files),$to = shift(@files)) {
	$line .= " $from $to";
	if (length($line) > 128) {
	    push(@m,"\t\$(NOECHO) \$(PERL) -e \"print '$line'\" >>.MM_tmp\n");
	    $line = '';
	}
    }
    push(@m,"\t\$(NOECHO) \$(PERL) -e \"print '$line'\" >>.MM_tmp\n") if $line;

    push(@m,q[	$(PERL) "-I$(PERL_LIB)" "-MExtUtils::Install" -e "pm_to_blib({split(' ',<STDIN>)},'].$autodir.q[')" <.MM_tmp]);
    push(@m,qq[
	\$(NOECHO) Delete/NoLog/NoConfirm .MM_tmp;
	\$(NOECHO) \$(TOUCH) pm_to_blib.ts
]);

    join('',@m);
}

=item tool_autosplit (override)

Use VMS-style quoting on command line.

=cut

sub tool_autosplit{
    my($self, %attribs) = @_;
    my($asl) = "";
    $asl = "\$AutoSplit::Maxlen=$attribs{MAXLEN};" if $attribs{MAXLEN};
    q{
# Usage: $(AUTOSPLITFILE) FileToSplit AutoDirToSplitInto
AUTOSPLITFILE = $(PERL) "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" -e "use AutoSplit;}.$asl.q{ AutoSplit::autosplit($ARGV[0], $ARGV[1], 0, 1, 1) ;"
};
}

=item tool_sxubpp (override)

Use VMS-style quoting on xsubpp command line.

=cut

sub tool_xsubpp {
    my($self) = @_;
    return '' unless $self->needs_linking;
    my($xsdir) = $self->catdir($self->{PERL_LIB},'ExtUtils');
    # drop back to old location if xsubpp is not in new location yet
    $xsdir = $self->catdir($self->{PERL_SRC},'ext') unless (-f $self->catfile($xsdir,'xsubpp'));
    my(@tmdeps) = '$(XSUBPPDIR)typemap';
    if( $self->{TYPEMAPS} ){
	my $typemap;
	foreach $typemap (@{$self->{TYPEMAPS}}){
		if( ! -f  $typemap ){
			warn "Typemap $typemap not found.\n";
		}
		else{
			push(@tmdeps, $self->fixpath($typemap,0));
		}
	}
    }
    push(@tmdeps, "typemap") if -f "typemap";
    my(@tmargs) = map("-typemap $_", @tmdeps);
    if( exists $self->{XSOPT} ){
	unshift( @tmargs, $self->{XSOPT} );
    }

    my $xsubpp_version = $self->xsubpp_version($self->catfile($xsdir,'xsubpp'));

    # What are the correct thresholds for version 1 && 2 Paul?
    if ( $xsubpp_version > 1.923 ){
	$self->{XSPROTOARG} = '' unless defined $self->{XSPROTOARG};
    } else {
	if (defined $self->{XSPROTOARG} && $self->{XSPROTOARG} =~ /\-prototypes/) {
	    print STDOUT qq{Warning: This extension wants to pass the switch "-prototypes" to xsubpp.
	Your version of xsubpp is $xsubpp_version and cannot handle this.
	Please upgrade to a more recent version of xsubpp.
};
	} else {
	    $self->{XSPROTOARG} = "";
	}
    }

    "
XSUBPPDIR = $xsdir
XSUBPP = \$(PERL) \"-I\$(PERL_ARCHLIB)\" \"-I\$(PERL_LIB)\" \$(XSUBPPDIR)xsubpp
XSPROTOARG = $self->{XSPROTOARG}
XSUBPPDEPS = @tmdeps
XSUBPPARGS = @tmargs
";
}

=item xsubpp_version (override)

Test xsubpp exit status according to VMS rules ($sts & 1 ==E<gt> good)
rather than Unix rules ($sts == 0 ==E<gt> good).

=cut

sub xsubpp_version
{
    my($self,$xsubpp) = @_;
    my ($version) ;
    return '' unless $self->needs_linking;

    # try to figure out the version number of the xsubpp on the system

    # first try the -v flag, introduced in 1.921 & 2.000a2

    my $command = "$self->{PERL} \"-I$self->{PERL_LIB}\" $xsubpp -v";
    print "Running: $command\n" if $Verbose;
    $version = `$command` ;
    if ($?) {
	use vmsish 'status';
	warn "Running '$command' exits with status $?";
    }
    chop $version ;

    return $1 if $version =~ /^xsubpp version (.*)/ ;

    # nope, then try something else

    my $counter = '000';
    my ($file) = 'temp' ;
    $counter++ while -e "$file$counter"; # don't overwrite anything
    $file .= $counter;

    local(*F);
    open(F, ">$file") or die "Cannot open file '$file': $!\n" ;
    print F <<EOM ;
MODULE = fred PACKAGE = fred

int
fred(a)
	int	a;
EOM

    close F ;

    $command = "$self->{PERL} $xsubpp $file";
    print "Running: $command\n" if $Verbose;
    my $text = `$command` ;
    if ($?) {
	use vmsish 'status';
	warn "Running '$command' exits with status $?";
    }
    unlink $file ;

    # gets 1.2 -> 1.92 and 2.000a1
    return $1 if $text =~ /automatically by xsubpp version ([\S]+)\s*/  ;

    # it is either 1.0 or 1.1
    return 1.1 if $text =~ /^Warning: ignored semicolon/ ;

    # none of the above, so 1.0
    return "1.0" ;
}

=item tools_other (override)

Adds a few MM[SK] macros, and shortens some the installatin commands,
in order to stay under DCL's 255-character limit.  Also changes
EQUALIZE_TIMESTAMP to set revision date of target file to one second
later than source file, since MMK interprets precisely equal revision
dates for a source and target file as a sign that the target needs
to be updated.

=cut

sub tools_other {
    my($self) = @_;
    qq!
# Assumes \$(MMS) invokes MMS or MMK
# (It is assumed in some cases later that the default makefile name
# (Descrip.MMS for MM[SK]) is used.)
USEMAKEFILE = /Descrip=
USEMACROS = /Macro=(
MACROEND = )
MAKEFILE = Descrip.MMS
SHELL = Posix
TOUCH = $self->{TOUCH}
CHMOD = $self->{CHMOD}
CP = $self->{CP}
MV = $self->{MV}
RM_F  = $self->{RM_F}
RM_RF = $self->{RM_RF}
SAY = Write Sys\$Output
UMASK_NULL = $self->{UMASK_NULL}
NOOP = $self->{NOOP}
NOECHO = $self->{NOECHO}
MKPATH = Create/Directory
EQUALIZE_TIMESTAMP = \$(PERL) -we "open F,qq{>\$ARGV[1]};close F;utime(0,(stat(\$ARGV[0]))[9]+1,\$ARGV[1])"
!. ($self->{PARENT} ? '' : 
qq!WARN_IF_OLD_PACKLIST = \$(PERL) -e "if (-f \$ARGV[0]){print qq[WARNING: Old package found (\$ARGV[0]); please check for collisions\\n]}"
MOD_INSTALL = \$(PERL) "-I\$(PERL_LIB)" "-MExtUtils::Install" -e "install({split(' ',<STDIN>)},1);"
DOC_INSTALL = \$(PERL) -e "\@ARGV=split(/\\|/,<STDIN>);print '=head2 ',scalar(localtime),': C<',shift,qq[>\\n\\n=over 4\\n\\n];while(\$key=shift && \$val=shift){print qq[=item *\\n\\nC<\$key: \$val>\\n\\n];}print qq[=back\\n\\n]"
UNINSTALL = \$(PERL) "-I\$(PERL_LIB)" "-MExtUtils::Install" -e "uninstall(\$ARGV[0],1,1);"
!);
}

=item dist (override)

Provide VMSish defaults for some values, then hand off to
default MM_Unix method.

=cut

sub dist {
    my($self, %attribs) = @_;
    $attribs{VERSION}      ||= $self->{VERSION_SYM};
    $attribs{NAME}         ||= $self->{DISTNAME};
    $attribs{ZIPFLAGS}     ||= '-Vu';
    $attribs{COMPRESS}     ||= 'gzip';
    $attribs{SUFFIX}       ||= '-gz';
    $attribs{SHAR}         ||= 'vms_share';
    $attribs{DIST_DEFAULT} ||= 'zipdist';

    # Sanitize these for use in $(DISTVNAME) filespec
    $attribs{VERSION} =~ s/[^\w\$]/_/g;
    $attribs{NAME} =~ s/[^\w\$]/_/g;

    return ExtUtils::MM_Unix::dist($self,%attribs);
}

=item c_o (override)

Use VMS syntax on command line.  In particular, $(DEFINE) and
$(PERL_INC) have been pulled into $(CCCMD).  Also use MM[SK] macros.

=cut

sub c_o {
    my($self) = @_;
    return '' unless $self->needs_linking();
    '
.c$(OBJ_EXT) :
	$(CCCMD) $(CCCDLFLAGS) $(MMS$TARGET_NAME).c

.cpp$(OBJ_EXT) :
	$(CCCMD) $(CCCDLFLAGS) $(MMS$TARGET_NAME).cpp

.cxx$(OBJ_EXT) :
	$(CCCMD) $(CCCDLFLAGS) $(MMS$TARGET_NAME).cxx

';
}

=item xs_c (override)

Use MM[SK] macros.

=cut

sub xs_c {
    my($self) = @_;
    return '' unless $self->needs_linking();
    '
.xs.c :
	$(XSUBPP) $(XSPROTOARG) $(XSUBPPARGS) $(MMS$TARGET_NAME).xs >$(MMS$TARGET)
';
}

=item xs_o (override)

Use MM[SK] macros, and VMS command line for C compiler.

=cut

sub xs_o {	# many makes are too dumb to use xs_c then c_o
    my($self) = @_;
    return '' unless $self->needs_linking();
    '
.xs$(OBJ_EXT) :
	$(XSUBPP) $(XSPROTOARG) $(XSUBPPARGS) $(MMS$TARGET_NAME).xs >$(MMS$TARGET_NAME).c
	$(CCCMD) $(CCCDLFLAGS) $(MMS$TARGET_NAME).c
';
}

=item top_targets (override)

Use VMS quoting on command line for Version_check.

=cut

sub top_targets {
    my($self) = shift;
    my(@m);
    push @m, '
all :: pure_all manifypods
	$(NOECHO) $(NOOP)

pure_all :: config pm_to_blib subdirs linkext
	$(NOECHO) $(NOOP)

subdirs :: $(MYEXTLIB)
	$(NOECHO) $(NOOP)

config :: $(MAKEFILE) $(INST_LIBDIR).exists
	$(NOECHO) $(NOOP)

config :: $(INST_ARCHAUTODIR).exists
	$(NOECHO) $(NOOP)

config :: $(INST_AUTODIR).exists
	$(NOECHO) $(NOOP)
';

    push @m, q{
config :: Version_check
	$(NOECHO) $(NOOP)

} unless $self->{PARENT} or ($self->{PERL_SRC} && $self->{INSTALLDIRS} eq "perl") or $self->{NO_VC};


    push @m, $self->dir_target(qw[$(INST_AUTODIR) $(INST_LIBDIR) $(INST_ARCHAUTODIR)]);
    if (%{$self->{MAN1PODS}}) {
	push @m, q[
config :: $(INST_MAN1DIR).exists
	$(NOECHO) $(NOOP)
];
	push @m, $self->dir_target(qw[$(INST_MAN1DIR)]);
    }
    if (%{$self->{MAN3PODS}}) {
	push @m, q[
config :: $(INST_MAN3DIR).exists
	$(NOECHO) $(NOOP)
];
	push @m, $self->dir_target(qw[$(INST_MAN3DIR)]);
    }

    push @m, '
$(O_FILES) : $(H_FILES)
' if @{$self->{O_FILES} || []} && @{$self->{H} || []};

    push @m, q{
help :
	perldoc ExtUtils::MakeMaker
};

    push @m, q{
Version_check :
	$(NOECHO) $(PERL) "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" -
	"-MExtUtils::MakeMaker=Version_check" -e "&Version_check('$(MM_VERSION)')"
};

    join('',@m);
}

=item dlsyms (override)

Create VMS linker options files specifying universal symbols for this
extension's shareable image, and listing other shareable images or 
libraries to which it should be linked.

=cut

sub dlsyms {
    my($self,%attribs) = @_;

    return '' unless $self->needs_linking();

    my($funcs) = $attribs{DL_FUNCS} || $self->{DL_FUNCS} || {};
    my($vars)  = $attribs{DL_VARS}  || $self->{DL_VARS}  || [];
    my($funclist)  = $attribs{FUNCLIST}  || $self->{FUNCLIST}  || [];
    my(@m);

    unless ($self->{SKIPHASH}{'dynamic'}) {
	push(@m,'
dynamic :: $(INST_ARCHAUTODIR)$(BASEEXT).opt
	$(NOECHO) $(NOOP)
');
    }

    push(@m,'
static :: $(INST_ARCHAUTODIR)$(BASEEXT).opt
	$(NOECHO) $(NOOP)
') unless $self->{SKIPHASH}{'static'};

    push(@m,'
$(INST_ARCHAUTODIR)$(BASEEXT).opt : $(BASEEXT).opt
	$(CP) $(MMS$SOURCE) $(MMS$TARGET)

$(BASEEXT).opt : Makefile.PL
	$(PERL) "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" -e "use ExtUtils::Mksymlists;" -
	',qq[-e "Mksymlists('NAME' => '$self->{NAME}', 'DL_FUNCS' => ],
	neatvalue($funcs),q[, 'DL_VARS' => ],neatvalue($vars),
	q[, 'FUNCLIST' => ],neatvalue($funclist),')"
	$(PERL) -e "print ""$(INST_STATIC)/Include=$(BASEEXT)\n$(INST_STATIC)/Library\n"";" >>$(MMS$TARGET)
');

    if (length $self->{LDLOADLIBS}) {
	my($lib); my($line) = '';
	foreach $lib (split ' ', $self->{LDLOADLIBS}) {
	    $lib =~ s%\$%\\\$%g;  # Escape '$' in VMS filespecs
	    if (length($line) + length($lib) > 160) {
		push @m, "\t\$(PERL) -e \"print qq{$line}\" >>\$(MMS\$TARGET)\n";
		$line = $lib . '\n';
	    }
	    else { $line .= $lib . '\n'; }
	}
	push @m, "\t\$(PERL) -e \"print qq{$line}\" >>\$(MMS\$TARGET)\n" if $line;
    }

    join('',@m);

}

=item dynamic_lib (override)

Use VMS Link command.

=cut

sub dynamic_lib {
    my($self, %attribs) = @_;
    return '' unless $self->needs_linking(); #might be because of a subdir

    return '' unless $self->has_link_code();

    my($otherldflags) = $attribs{OTHERLDFLAGS} || "";
    my($inst_dynamic_dep) = $attribs{INST_DYNAMIC_DEP} || "";
    my $shr = $Config{'dbgprefix'} . 'PerlShr';
    my(@m);
    push @m,"

OTHERLDFLAGS = $otherldflags
INST_DYNAMIC_DEP = $inst_dynamic_dep

";
    push @m, '
$(INST_DYNAMIC) : $(INST_STATIC) $(PERL_INC)perlshr_attr.opt $(INST_ARCHAUTODIR).exists $(EXPORT_LIST) $(PERL_ARCHIVE) $(INST_DYNAMIC_DEP)
	$(NOECHO) $(MKPATH) $(INST_ARCHAUTODIR)
	If F$TrnLNm("',$shr,'").eqs."" Then Define/NoLog/User ',"$shr Sys\$Share:$shr.$Config{'dlext'}",'
	Link $(LDFLAGS) /Shareable=$(MMS$TARGET)$(OTHERLDFLAGS) $(BASEEXT).opt/Option,$(PERL_INC)perlshr_attr.opt/Option
';

    push @m, $self->dir_target('$(INST_ARCHAUTODIR)');
    join('',@m);
}

=item dynamic_bs (override)

Use VMS-style quoting on Mkbootstrap command line.

=cut

sub dynamic_bs {
    my($self, %attribs) = @_;
    return '
BOOTSTRAP =
' unless $self->has_link_code();
    '
BOOTSTRAP = '."$self->{BASEEXT}.bs".'

# As MakeMaker mkbootstrap might not write a file (if none is required)
# we use touch to prevent make continually trying to remake it.
# The DynaLoader only reads a non-empty file.
$(BOOTSTRAP) : $(MAKEFILE) '."$self->{BOOTDEP}".' $(INST_ARCHAUTODIR).exists
	$(NOECHO) $(SAY) "Running mkbootstrap for $(NAME) ($(BSLOADLIBS))"
	$(NOECHO) $(PERL) "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" -
	-e "use ExtUtils::Mkbootstrap; Mkbootstrap(\'$(BASEEXT)\',\'$(BSLOADLIBS)\');"
	$(NOECHO) $(TOUCH) $(MMS$TARGET)

$(INST_BOOT) : $(BOOTSTRAP) $(INST_ARCHAUTODIR).exists
	$(NOECHO) $(RM_RF) $(INST_BOOT)
	- $(CP) $(BOOTSTRAP) $(INST_BOOT)
';
}

=item static_lib (override)

Use VMS commands to manipulate object library.

=cut

sub static_lib {
    my($self) = @_;
    return '' unless $self->needs_linking();

    return '
$(INST_STATIC) :
	$(NOECHO) $(NOOP)
' unless ($self->{OBJECT} or @{$self->{C} || []} or $self->{MYEXTLIB});

    my(@m,$lib);
    push @m,'
# Rely on suffix rule for update action
$(OBJECT) : $(INST_ARCHAUTODIR).exists

$(INST_STATIC) : $(OBJECT) $(MYEXTLIB)
';
    # If this extension has it's own library (eg SDBM_File)
    # then copy that to $(INST_STATIC) and add $(OBJECT) into it.
    push(@m, "\t",'$(CP) $(MYEXTLIB) $(MMS$TARGET)',"\n") if $self->{MYEXTLIB};

    push(@m,"\t",'If F$Search("$(MMS$TARGET)").eqs."" Then Library/Object/Create $(MMS$TARGET)',"\n");

    # if there was a library to copy, then we can't use MMS$SOURCE_LIST,
    # 'cause it's a library and you can't stick them in other libraries.
    # In that case, we use $OBJECT instead and hope for the best
    if ($self->{MYEXTLIB}) {
      push(@m,"\t",'Library/Object/Replace $(MMS$TARGET) $(OBJECT)',"\n"); 
    } else {
      push(@m,"\t",'Library/Object/Replace $(MMS$TARGET) $(MMS$SOURCE_LIST)',"\n");
    }
    
    foreach $lib (split $self->{EXTRALIBS}) {
      $lib = '""' if $lib eq '"';
      push(@m,"\t",'$(NOECHO) $(PERL) -e "print qq{',$lib,'\n}" >>$(INST_ARCHAUTODIR)extralibs.ld',"\n");
    }
    push @m, $self->dir_target('$(INST_ARCHAUTODIR)');
    join('',@m);
}


=item manifypods (override)

Use VMS-style quoting on command line, and VMS logical name
to specify fallback location at build time if we can't find pod2man.

=cut


sub manifypods {
    my($self, %attribs) = @_;
    return "\nmanifypods :\n\t\$(NOECHO) \$(NOOP)\n" unless %{$self->{MAN3PODS}} or %{$self->{MAN1PODS}};
    my($dist);
    my($pod2man_exe);
    if (defined $self->{PERL_SRC}) {
	$pod2man_exe = $self->catfile($self->{PERL_SRC},'pod','pod2man');
    } else {
	$pod2man_exe = $self->catfile($Config{scriptdirexp},'pod2man');
    }
    if (not ($pod2man_exe = $self->perl_script($pod2man_exe))) {
	# No pod2man but some MAN3PODS to be installed
	print <<END;

Warning: I could not locate your pod2man program.  As a last choice,
         I will look for the file to which the logical name POD2MAN
         points when MMK is invoked.

END
        $pod2man_exe = "pod2man";
    }
    my(@m);
    push @m,
qq[POD2MAN_EXE = $pod2man_exe\n],
q[POD2MAN = $(PERL) -we "%m=@ARGV;for (keys %m){" -
-e "system(""MCR $^X $(POD2MAN_EXE) $_ >$m{$_}"");}"
];
    push @m, "\nmanifypods : \$(MAN1PODS) \$(MAN3PODS)\n";
    if (%{$self->{MAN1PODS}} || %{$self->{MAN3PODS}}) {
	my($pod);
	foreach $pod (sort keys %{$self->{MAN1PODS}}) {
	    push @m, qq[\t\@- If F\$Search("\$(POD2MAN_EXE)").nes."" Then \$(POD2MAN) ];
	    push @m, "$pod $self->{MAN1PODS}{$pod}\n";
	}
	foreach $pod (sort keys %{$self->{MAN3PODS}}) {
	    push @m, qq[\t\@- If F\$Search("\$(POD2MAN_EXE)").nes."" Then \$(POD2MAN) ];
	    push @m, "$pod $self->{MAN3PODS}{$pod}\n";
	}
    }
    join('', @m);
}

=item processPL (override)

Use VMS-style quoting on command line.

=cut

sub processPL {
    my($self) = @_;
    return "" unless $self->{PL_FILES};
    my(@m, $plfile);
    foreach $plfile (sort keys %{$self->{PL_FILES}}) {
        my $list = ref($self->{PL_FILES}->{$plfile})
		? $self->{PL_FILES}->{$plfile}
		: [$self->{PL_FILES}->{$plfile}];
	foreach $target (@$list) {
	    my $vmsplfile = vmsify($plfile);
	    my $vmsfile = vmsify($target);
	    push @m, "
all :: $vmsfile
	\$(NOECHO) \$(NOOP)

$vmsfile :: $vmsplfile
",'	$(PERL) "-I$(INST_ARCHLIB)" "-I$(INST_LIB)" "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" '," $vmsplfile $vmsfile
";
	}
    }
    join "", @m;
}

=item installbin (override)

Stay under DCL's 255 character command line limit once again by
splitting potentially long list of files across multiple lines
in C<realclean> target.

=cut

sub installbin {
    my($self) = @_;
    return '' unless $self->{EXE_FILES} && ref $self->{EXE_FILES} eq "ARRAY";
    return '' unless @{$self->{EXE_FILES}};
    my(@m, $from, $to, %fromto, @to, $line);
    my(@exefiles) = map { vmsify($_) } @{$self->{EXE_FILES}};
    for $from (@exefiles) {
	my($path) = '$(INST_SCRIPT)' . basename($from);
	local($_) = $path;  # backward compatibility
	$to = $self->libscan($path);
	print "libscan($from) => '$to'\n" if ($Verbose >=2);
	$fromto{$from} = vmsify($to);
    }
    @to = values %fromto;
    push @m, "
EXE_FILES = @exefiles

all :: @to
	\$(NOECHO) \$(NOOP)

realclean ::
";
    $line = '';  #avoid unitialized var warning
    foreach $to (@to) {
	if (length($line) + length($to) > 80) {
	    push @m, "\t\$(RM_F) $line\n";
	    $line = $to;
	}
	else { $line .= " $to"; }
    }
    push @m, "\t\$(RM_F) $line\n\n" if $line;

    while (($from,$to) = each %fromto) {
	last unless defined $from;
	my $todir;
	if ($to =~ m#[/>:\]]#) { $todir = dirname($to); }
	else                   { ($todir = $to) =~ s/[^\)]+$//; }
	$todir = $self->fixpath($todir,1);
	push @m, "
$to : $from \$(MAKEFILE) ${todir}.exists
	\$(CP) $from $to

", $self->dir_target($todir);
    }
    join "", @m;
}

=item subdir_x (override)

Use VMS commands to change default directory.

=cut

sub subdir_x {
    my($self, $subdir) = @_;
    my(@m,$key);
    $subdir = $self->fixpath($subdir,1);
    push @m, '

subdirs ::
	olddef = F$Environment("Default")
	Set Default ',$subdir,'
	- $(MMS)$(MMSQUALIFIERS) all $(USEMACROS)$(PASTHRU)$(MACROEND)
	Set Default \'olddef\'
';
    join('',@m);
}

=item clean (override)

Split potentially long list of files across multiple commands (in
order to stay under the magic command line limit).  Also use MM[SK]
commands for handling subdirectories.

=cut

sub clean {
    my($self, %attribs) = @_;
    my(@m,$dir);
    push @m, '
# Delete temporary files but do not touch installed files. We don\'t delete
# the Descrip.MMS here so that a later make realclean still has it to use.
clean ::
';
    foreach $dir (@{$self->{DIR}}) { # clean subdirectories first
	my($vmsdir) = $self->fixpath($dir,1);
	push( @m, '	If F$Search("'.$vmsdir.'$(MAKEFILE)").nes."" Then \\',"\n\t",
	      '$(PERL) -e "chdir ',"'$vmsdir'",'; print `$(MMS)$(MMSQUALIFIERS) clean`;"',"\n");
    }
    push @m, '	$(RM_F) *.Map *.Dmp *.Lis *.cpp *.$(DLEXT) *$(OBJ_EXT) *$(LIB_EXT) *.Opt $(BOOTSTRAP) $(BASEEXT).bso .MM_Tmp
';

    my(@otherfiles) = values %{$self->{XS}}; # .c files from *.xs files
    # Unlink realclean, $attribs{FILES} is a string here; it may contain
    # a list or a macro that expands to a list.
    if ($attribs{FILES}) {
	my($word,$key,@filist);
	if (ref $attribs{FILES} eq 'ARRAY') { @filist = @{$attribs{FILES}}; }
	else { @filist = split /\s+/, $attribs{FILES}; }
	foreach $word (@filist) {
	    if (($key) = $word =~ m#^\$\((.*)\)$# and ref $self->{$key} eq 'ARRAY') {
		push(@otherfiles, @{$self->{$key}});
	    }
	    else { push(@otherfiles, $word); }
	}
    }
    push(@otherfiles, qw[ blib $(MAKE_APERL_FILE) extralibs.ld perlmain.c pm_to_blib.ts ]);
    push(@otherfiles,$self->catfile('$(INST_ARCHAUTODIR)','extralibs.all'));
    my($file,$line);
    $line = '';  #avoid unitialized var warning
    # Occasionally files are repeated several times from different sources
    { my(%of) = map { ($_,1) } @otherfiles; @otherfiles = keys %of; }
    
    foreach $file (@otherfiles) {
	$file = $self->fixpath($file);
	if (length($line) + length($file) > 80) {
	    push @m, "\t\$(RM_RF) $line\n";
	    $line = "$file";
	}
	else { $line .= " $file"; }
    }
    push @m, "\t\$(RM_RF) $line\n" if $line;
    push(@m, "	$attribs{POSTOP}\n") if $attribs{POSTOP};
    join('', @m);
}

=item realclean (override)

Guess what we're working around?  Also, use MM[SK] for subdirectories.

=cut

sub realclean {
    my($self, %attribs) = @_;
    my(@m);
    push(@m,'
# Delete temporary files (via clean) and also delete installed files
realclean :: clean
');
    foreach(@{$self->{DIR}}){
	my($vmsdir) = $self->fixpath($_,1);
	push(@m, '	If F$Search("'."$vmsdir".'$(MAKEFILE)").nes."" Then \\',"\n\t",
	      '$(PERL) -e "chdir ',"'$vmsdir'",'; print `$(MMS)$(MMSQUALIFIERS) realclean`;"',"\n");
    }
    push @m,'	$(RM_RF) $(INST_AUTODIR) $(INST_ARCHAUTODIR)
';
    # We can't expand several of the MMS macros here, since they don't have
    # corresponding %$self keys (i.e. they're defined in Descrip.MMS as a
    # combination of macros).  In order to stay below DCL's 255 char limit,
    # we put only 2 on a line.
    my($file,$line,$fcnt);
    my(@files) = qw{ $(MAKEFILE) $(MAKEFILE)_old };
    if ($self->has_link_code) {
	push(@files,qw{ $(INST_DYNAMIC) $(INST_STATIC) $(INST_BOOT) $(OBJECT) });
    }
    push(@files, values %{$self->{PM}});
    $line = '';  #avoid unitialized var warning
    # Occasionally files are repeated several times from different sources
    { my(%f) = map { ($_,1) } @files; @files = keys %f; }
    foreach $file (@files) {
	$file = $self->fixpath($file);
	if (length($line) + length($file) > 80 || ++$fcnt >= 2) {
	    push @m, "\t\$(RM_F) $line\n";
	    $line = "$file";
	    $fcnt = 0;
	}
	else { $line .= " $file"; }
    }
    push @m, "\t\$(RM_F) $line\n" if $line;
    if ($attribs{FILES}) {
	my($word,$key,@filist,@allfiles);
	if (ref $attribs{FILES} eq 'ARRAY') { @filist = @{$attribs{FILES}}; }
	else { @filist = split /\s+/, $attribs{FILES}; }
	foreach $word (@filist) {
	    if (($key) = $word =~ m#^\$\((.*)\)$# and ref $self->{$key} eq 'ARRAY') {
		push(@allfiles, @{$self->{$key}});
	    }
	    else { push(@allfiles, $word); }
	}
	$line = '';
	# Occasionally files are repeated several times from different sources
	{ my(%af) = map { ($_,1) } @allfiles; @allfiles = keys %af; }
	foreach $file (@allfiles) {
	    $file = $self->fixpath($file);
	    if (length($line) + length($file) > 80) {
		push @m, "\t\$(RM_RF) $line\n";
		$line = "$file";
	    }
	    else { $line .= " $file"; }
	}
	push @m, "\t\$(RM_RF) $line\n" if $line;
    }
    push(@m, "	$attribs{POSTOP}\n")                     if $attribs{POSTOP};
    join('', @m);
}

=item dist_basics (override)

Use VMS-style quoting on command line.

=cut

sub dist_basics {
    my($self) = @_;
'
distclean :: realclean distcheck
	$(NOECHO) $(NOOP)

distcheck :
	$(PERL) "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" -e "use ExtUtils::Manifest \'&fullcheck\'; fullcheck()"

skipcheck :
	$(PERL) "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" -e "use ExtUtils::Manifest \'&skipcheck\'; skipcheck()"

manifest :
	$(PERL) "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" -e "use ExtUtils::Manifest \'&mkmanifest\'; mkmanifest()"
';
}

=item dist_core (override)

Syntax for invoking F<VMS_Share> differs from that for Unix F<shar>,
so C<shdist> target actions are VMS-specific.

=cut

sub dist_core {
    my($self) = @_;
q[
dist : $(DIST_DEFAULT)
	$(NOECHO) $(PERL) -le "print 'Warning: $m older than $vf' if -e ($vf = '$(VERSION_FROM)') && -M $vf < -M ($m = '$(MAKEFILE)')"

zipdist : $(DISTVNAME).zip
	$(NOECHO) $(NOOP)

$(DISTVNAME).zip : distdir
	$(PREOP)
	$(ZIP) "$(ZIPFLAGS)" $(MMS$TARGET) [.$(DISTVNAME)...]*.*;
	$(RM_RF) $(DISTVNAME)
	$(POSTOP)

$(DISTVNAME).tar$(SUFFIX) : distdir
	$(PREOP)
	$(TO_UNIX)
	$(TAR) "$(TARFLAGS)" $(DISTVNAME).tar [.$(DISTVNAME)]
	$(RM_RF) $(DISTVNAME)
	$(COMPRESS) $(DISTVNAME).tar
	$(POSTOP)

shdist : distdir
	$(PREOP)
	$(SHAR) [.$(DISTVNAME...]*.*; $(DISTVNAME).share
	$(RM_RF) $(DISTVNAME)
	$(POSTOP)
];
}

=item dist_dir (override)

Use VMS-style quoting on command line.

=cut

sub dist_dir {
    my($self) = @_;
q{
distdir :
	$(RM_RF) $(DISTVNAME)
	$(PERL) "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" -e "use ExtUtils::Manifest '/mani/';" \\
	-e "manicopy(maniread(),'$(DISTVNAME)','$(DIST_CP)');"
};
}

=item dist_test (override)

Use VMS commands to change default directory, and use VMS-style
quoting on command line.

=cut

sub dist_test {
    my($self) = @_;
q{
disttest : distdir
	startdir = F$Environment("Default")
	Set Default [.$(DISTVNAME)]
	$(PERL) "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" Makefile.PL
	$(MMS)$(MMSQUALIFIERS)
	$(MMS)$(MMSQUALIFIERS) test
	Set Default 'startdir'
};
}

# --- Test and Installation Sections ---

=item install (override)

Work around DCL's 255 character limit several times,and use
VMS-style command line quoting in a few cases.

=cut

sub install {
    my($self, %attribs) = @_;
    my(@m,@docfiles);

    if ($self->{EXE_FILES}) {
	my($line,$file) = ('','');
	foreach $file (@{$self->{EXE_FILES}}) {
	    $line .= "$file ";
	    if (length($line) > 128) {
		push(@docfiles,qq[\t\$(PERL) -e "print '$line'" >>.MM_tmp\n]);
		$line = '';
	    }
	}
	push(@docfiles,qq[\t\$(PERL) -e "print '$line'" >>.MM_tmp\n]) if $line;
    }

    push @m, q[
install :: all pure_install doc_install
	$(NOECHO) $(NOOP)

install_perl :: all pure_perl_install doc_perl_install
	$(NOECHO) $(NOOP)

install_site :: all pure_site_install doc_site_install
	$(NOECHO) $(NOOP)

install_ :: install_site
	$(NOECHO) $(SAY) "INSTALLDIRS not defined, defaulting to INSTALLDIRS=site"

pure_install :: pure_$(INSTALLDIRS)_install
	$(NOECHO) $(NOOP)

doc_install :: doc_$(INSTALLDIRS)_install
	$(NOECHO) $(SAY) "Appending installation info to $(INSTALLARCHLIB)perllocal.pod"

pure__install : pure_site_install
	$(NOECHO) $(SAY) "INSTALLDIRS not defined, defaulting to INSTALLDIRS=site"

doc__install : doc_site_install
	$(NOECHO) $(SAY) "INSTALLDIRS not defined, defaulting to INSTALLDIRS=site"

# This hack brought to you by DCL's 255-character command line limit
pure_perl_install ::
	$(NOECHO) $(PERL) -e "print 'read ].$self->catfile('$(PERL_ARCHLIB)','auto','$(FULLEXT)','.packlist').q[ '" >.MM_tmp
	$(NOECHO) $(PERL) -e "print 'write ].$self->catfile('$(INSTALLARCHLIB)','auto','$(FULLEXT)','.packlist').q[ '" >>.MM_tmp
	$(NOECHO) $(PERL) -e "print '$(INST_LIB) $(INSTALLPRIVLIB) '" >>.MM_tmp
	$(NOECHO) $(PERL) -e "print '$(INST_ARCHLIB) $(INSTALLARCHLIB) '" >>.MM_tmp
	$(NOECHO) $(PERL) -e "print '$(INST_BIN) $(INSTALLBIN) '" >>.MM_tmp
	$(NOECHO) $(PERL) -e "print '$(INST_SCRIPT) $(INSTALLSCRIPT) '" >>.MM_tmp
	$(NOECHO) $(PERL) -e "print '$(INST_MAN1DIR) $(INSTALLMAN1DIR) '" >>.MM_tmp
	$(NOECHO) $(PERL) -e "print '$(INST_MAN3DIR) $(INSTALLMAN3DIR) '" >>.MM_tmp
	$(MOD_INSTALL) <.MM_tmp
	$(NOECHO) Delete/NoLog/NoConfirm .MM_tmp;
	$(NOECHO) $(WARN_IF_OLD_PACKLIST) ].$self->catfile('$(SITEARCHEXP)','auto','$(FULLEXT)','.packlist').q[

# Likewise
pure_site_install ::
	$(NOECHO) $(PERL) -e "print 'read ].$self->catfile('$(SITEARCHEXP)','auto','$(FULLEXT)','.packlist').q[ '" >.MM_tmp
	$(NOECHO) $(PERL) -e "print 'write ].$self->catfile('$(INSTALLSITEARCH)','auto','$(FULLEXT)','.packlist').q[ '" >>.MM_tmp
	$(NOECHO) $(PERL) -e "print '$(INST_LIB) $(INSTALLSITELIB) '" >>.MM_tmp
	$(NOECHO) $(PERL) -e "print '$(INST_ARCHLIB) $(INSTALLSITEARCH) '" >>.MM_tmp
	$(NOECHO) $(PERL) -e "print '$(INST_BIN) $(INSTALLBIN) '" >>.MM_tmp
	$(NOECHO) $(PERL) -e "print '$(INST_SCRIPT) $(INSTALLSCRIPT) '" >>.MM_tmp
	$(NOECHO) $(PERL) -e "print '$(INST_MAN1DIR) $(INSTALLMAN1DIR) '" >>.MM_tmp
	$(NOECHO) $(PERL) -e "print '$(INST_MAN3DIR) $(INSTALLMAN3DIR) '" >>.MM_tmp
	$(MOD_INSTALL) <.MM_tmp
	$(NOECHO) Delete/NoLog/NoConfirm .MM_tmp;
	$(NOECHO) $(WARN_IF_OLD_PACKLIST) ].$self->catfile('$(PERL_ARCHLIB)','auto','$(FULLEXT)','.packlist').q[

# Ditto
doc_perl_install ::
	$(NOECHO) $(PERL) -e "print 'Module $(NAME)|installed into|$(INSTALLPRIVLIB)|'" >.MM_tmp
	$(NOECHO) $(PERL) -e "print 'LINKTYPE|$(LINKTYPE)|VERSION|$(VERSION)|EXE_FILES|$(EXE_FILES)|'" >>.MM_tmp
],@docfiles,
q%	$(NOECHO) $(PERL) -e "print q[@ARGV=split(/\\|/,<STDIN>);]" >.MM2_tmp
	$(NOECHO) $(PERL) -e "print q[print '=head2 ',scalar(localtime),': C<',shift,qq[>\\n\\n=over 4\\n\\n];]" >>.MM2_tmp
	$(NOECHO) $(PERL) -e "print q[while(($key=shift) && ($val=shift)) ]" >>.MM2_tmp
	$(NOECHO) $(PERL) -e "print q[{print qq[=item *\\n\\nC<$key: $val>\\n\\n];}print qq[=back\\n\\n];]" >>.MM2_tmp
	$(NOECHO) $(PERL) .MM2_tmp <.MM_tmp >>%.$self->catfile('$(INSTALLARCHLIB)','perllocal.pod').q[
	$(NOECHO) Delete/NoLog/NoConfirm .MM_tmp;,.MM2_tmp;

# And again
doc_site_install ::
	$(NOECHO) $(PERL) -e "print 'Module $(NAME)|installed into|$(INSTALLSITELIB)|'" >.MM_tmp
	$(NOECHO) $(PERL) -e "print 'LINKTYPE|$(LINKTYPE)|VERSION|$(VERSION)|EXE_FILES|$(EXE_FILES)|'" >>.MM_tmp
],@docfiles,
q%	$(NOECHO) $(PERL) -e "print q[@ARGV=split(/\\|/,<STDIN>);]" >.MM2_tmp
	$(NOECHO) $(PERL) -e "print q[print '=head2 ',scalar(localtime),': C<',shift,qq[>\\n\\n=over 4\\n\\n];]" >>.MM2_tmp
	$(NOECHO) $(PERL) -e "print q[while(($key=shift) && ($val=shift)) ]" >>.MM2_tmp
	$(NOECHO) $(PERL) -e "print q[{print qq[=item *\\n\\nC<$key: $val>\\n\\n];}print qq[=back\\n\\n];]" >>.MM2_tmp
	$(NOECHO) $(PERL) .MM2_tmp <.MM_tmp >>%.$self->catfile('$(INSTALLARCHLIB)','perllocal.pod').q[
	$(NOECHO) Delete/NoLog/NoConfirm .MM_tmp;,.MM2_tmp;

];

    push @m, q[
uninstall :: uninstall_from_$(INSTALLDIRS)dirs
	$(NOECHO) $(NOOP)

uninstall_from_perldirs ::
	$(NOECHO) $(UNINSTALL) ].$self->catfile('$(PERL_ARCHLIB)','auto','$(FULLEXT)','.packlist').q[
	$(NOECHO) $(SAY) "Uninstall is now deprecated and makes no actual changes."
	$(NOECHO) $(SAY) "Please check the list above carefully for errors, and manually remove"
	$(NOECHO) $(SAY) "the appropriate files.  Sorry for the inconvenience."

uninstall_from_sitedirs ::
	$(NOECHO) $(UNINSTALL) ],$self->catfile('$(SITEARCHEXP)','auto','$(FULLEXT)','.packlist'),"\n",q[
	$(NOECHO) $(SAY) "Uninstall is now deprecated and makes no actual changes."
	$(NOECHO) $(SAY) "Please check the list above carefully for errors, and manually remove"
	$(NOECHO) $(SAY) "the appropriate files.  Sorry for the inconvenience."
];

    join('',@m);
}

=item perldepend (override)

Use VMS-style syntax for files; it's cheaper to just do it directly here
than to have the MM_Unix method call C<catfile> repeatedly.  Also, if
we have to rebuild Config.pm, use MM[SK] to do it.

=cut

sub perldepend {
    my($self) = @_;
    my(@m);

    push @m, '
$(OBJECT) : $(PERL_INC)EXTERN.h, $(PERL_INC)INTERN.h, $(PERL_INC)XSUB.h, $(PERL_INC)av.h
$(OBJECT) : $(PERL_INC)cop.h, $(PERL_INC)cv.h, $(PERL_INC)embed.h, $(PERL_INC)form.h
$(OBJECT) : $(PERL_INC)gv.h, $(PERL_INC)handy.h, $(PERL_INC)hv.h, $(PERL_INC)keywords.h
$(OBJECT) : $(PERL_INC)mg.h, $(PERL_INC)op.h, $(PERL_INC)opcode.h, $(PERL_INC)patchlevel.h
$(OBJECT) : $(PERL_INC)perl.h, $(PERL_INC)perly.h, $(PERL_INC)pp.h, $(PERL_INC)proto.h
$(OBJECT) : $(PERL_INC)regcomp.h, $(PERL_INC)regexp.h, $(PERL_INC)scope.h, $(PERL_INC)sv.h
$(OBJECT) : $(PERL_INC)vmsish.h, $(PERL_INC)util.h, $(PERL_INC)config.h
$(OBJECT) : $(PERL_INC)iperlsys.h

' if $self->{OBJECT}; 

    if ($self->{PERL_SRC}) {
	my(@macros);
	my($mmsquals) = '$(USEMAKEFILE)[.vms]$(MAKEFILE)';
	push(@macros,'__AXP__=1') if $Config{'arch'} eq 'VMS_AXP';
	push(@macros,'DECC=1')    if $Config{'vms_cc_type'} eq 'decc';
	push(@macros,'GNUC=1')    if $Config{'vms_cc_type'} eq 'gcc';
	push(@macros,'SOCKET=1')  if $Config{'d_has_sockets'};
	push(@macros,qq["CC=$Config{'cc'}"])  if $Config{'cc'} =~ m!/!;
	$mmsquals .= '$(USEMACROS)' . join(',',@macros) . '$(MACROEND)' if @macros;
	push(@m,q[
# Check for unpropagated config.sh changes. Should never happen.
# We do NOT just update config.h because that is not sufficient.
# An out of date config.h is not fatal but complains loudly!
$(PERL_INC)config.h : $(PERL_SRC)config.sh

$(PERL_ARCHLIB)Config.pm : $(PERL_SRC)config.sh
	$(NOECHO) Write Sys$Error "$(PERL_ARCHLIB)Config.pm may be out of date with config.h or genconfig.pl"
	olddef = F$Environment("Default")
	Set Default $(PERL_SRC)
	$(MMS)],$mmsquals,);
	if ($self->{PERL_ARCHLIB} =~ m|\[-| && $self->{PERL_SRC} =~ m|(\[-+)|) {
	    my($prefix,$target) = ($1,$self->fixpath('$(PERL_ARCHLIB)Config.pm',0));
	    $target =~ s/\Q$prefix/[/;
	    push(@m," $target");
	}
	else { push(@m,' $(MMS$TARGET)'); }
	push(@m,q[
	Set Default 'olddef'
]);
    }

    push(@m, join(" ", map($self->fixpath($_,0),values %{$self->{XS}}))." : \$(XSUBPPDEPS)\n")
      if %{$self->{XS}};

    join('',@m);
}

=item makefile (override)

Use VMS commands and quoting.

=cut

sub makefile {
    my($self) = @_;
    my(@m,@cmd);
    # We do not know what target was originally specified so we
    # must force a manual rerun to be sure. But as it should only
    # happen very rarely it is not a significant problem.
    push @m, q[
$(OBJECT) : $(FIRST_MAKEFILE)
] if $self->{OBJECT};

    push @m,q[
# We take a very conservative approach here, but it\'s worth it.
# We move $(MAKEFILE) to $(MAKEFILE)_old here to avoid gnu make looping.
$(MAKEFILE) : Makefile.PL $(CONFIGDEP)
	$(NOECHO) $(SAY) "$(MAKEFILE) out-of-date with respect to $(MMS$SOURCE_LIST)"
	$(NOECHO) $(SAY) "Cleaning current config before rebuilding $(MAKEFILE) ..."
	- $(MV) $(MAKEFILE) $(MAKEFILE)_old
	- $(MMS)$(MMSQUALIFIERS) $(USEMAKEFILE)$(MAKEFILE)_old clean
	$(PERL) "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" Makefile.PL ],join(' ',map(qq["$_"],@ARGV)),q[
	$(NOECHO) $(SAY) "$(MAKEFILE) has been rebuilt."
	$(NOECHO) $(SAY) "Please run $(MMS) to build the extension."
];

    join('',@m);
}

=item test (override)

Use VMS commands for handling subdirectories.

=cut

sub test {
    my($self, %attribs) = @_;
    my($tests) = $attribs{TESTS} || ( -d 't' ? 't/*.t' : '');
    my(@m);
    push @m,"
TEST_VERBOSE = 0
TEST_TYPE = test_\$(LINKTYPE)
TEST_FILE = test.pl
TESTDB_SW = -d

test :: \$(TEST_TYPE)
	\$(NOECHO) \$(NOOP)

testdb :: testdb_\$(LINKTYPE)
	\$(NOECHO) \$(NOOP)

";
    foreach(@{$self->{DIR}}){
      my($vmsdir) = $self->fixpath($_,1);
      push(@m, '	If F$Search("',$vmsdir,'$(MAKEFILE)").nes."" Then $(PERL) -e "chdir ',"'$vmsdir'",
           '; print `$(MMS)$(MMSQUALIFIERS) $(PASTHRU2) test`'."\n");
    }
    push(@m, "\t\$(NOECHO) \$(SAY) \"No tests defined for \$(NAME) extension.\"\n")
        unless $tests or -f "test.pl" or @{$self->{DIR}};
    push(@m, "\n");

    push(@m, "test_dynamic :: pure_all\n");
    push(@m, $self->test_via_harness('$(FULLPERL)', $tests)) if $tests;
    push(@m, $self->test_via_script('$(FULLPERL)', 'test.pl')) if -f "test.pl";
    push(@m, "\t\$(NOECHO) \$(NOOP)\n") if (!$tests && ! -f "test.pl");
    push(@m, "\n");

    push(@m, "testdb_dynamic :: pure_all\n");
    push(@m, $self->test_via_script('$(FULLPERL) "$(TESTDB_SW)"', '$(TEST_FILE)'));
    push(@m, "\n");

    # Occasionally we may face this degenerate target:
    push @m, "test_ : test_dynamic\n\n";
 
    if ($self->needs_linking()) {
	push(@m, "test_static :: pure_all \$(MAP_TARGET)\n");
	push(@m, $self->test_via_harness('$(MAP_TARGET)', $tests)) if $tests;
	push(@m, $self->test_via_script('$(MAP_TARGET)', 'test.pl')) if -f 'test.pl';
	push(@m, "\n");
	push(@m, "testdb_static :: pure_all \$(MAP_TARGET)\n");
	push(@m, $self->test_via_script('$(MAP_TARGET) $(TESTDB_SW)', '$(TEST_FILE)'));
	push(@m, "\n");
    }
    else {
	push @m, "test_static :: test_dynamic\n\t\$(NOECHO) \$(NOOP)\n\n";
	push @m, "testdb_static :: testdb_dynamic\n\t\$(NOECHO) \$(NOOP)\n";
    }

    join('',@m);
}

=item test_via_harness (override)

Use VMS-style quoting on command line.

=cut

sub test_via_harness {
    my($self,$perl,$tests) = @_;
    "	$perl".' "-I$(INST_ARCHLIB)" "-I$(INST_LIB)" "-I$(PERL_LIB)" "-I$(PERL_ARCHLIB)" \\'."\n\t".
    '-e "use Test::Harness qw(&runtests $verbose); $verbose=$(TEST_VERBOSE); runtests @ARGV;" \\'."\n\t$tests\n";
}

=item test_via_script (override)

Use VMS-style quoting on command line.

=cut

sub test_via_script {
    my($self,$perl,$script) = @_;
    "	$perl".' "-I$(INST_ARCHLIB)" "-I$(INST_LIB)" "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" '.$script.'
';
}

=item makeaperl (override)

Undertake to build a new set of Perl images using VMS commands.  Since
VMS does dynamic loading, it's not necessary to statically link each
extension into the Perl image, so this isn't the normal build path.
Consequently, it hasn't really been tested, and may well be incomplete.

=cut

sub makeaperl {
    my($self, %attribs) = @_;
    my($makefilename, $searchdirs, $static, $extra, $perlinc, $target, $tmp, $libperl) = 
      @attribs{qw(MAKE DIRS STAT EXTRA INCL TARGET TMP LIBPERL)};
    my(@m);
    push @m, "
# --- MakeMaker makeaperl section ---
MAP_TARGET    = $target
";
    return join '', @m if $self->{PARENT};

    my($dir) = join ":", @{$self->{DIR}};

    unless ($self->{MAKEAPERL}) {
	push @m, q{
$(MAKE_APERL_FILE) : $(FIRST_MAKEFILE)
	$(NOECHO) $(SAY) "Writing ""$(MMS$TARGET)"" for this $(MAP_TARGET)"
	$(NOECHO) $(PERL) "-I$(INST_ARCHLIB)" "-I$(INST_LIB)" "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" \
		Makefile.PL DIR=}, $dir, q{ \
		MAKEFILE=$(MAKE_APERL_FILE) LINKTYPE=static \
		MAKEAPERL=1 NORECURS=1

$(MAP_TARGET) :: $(MAKE_APERL_FILE)
	$(MMS)$(MMSQUALIFIERS)$(USEMAKEFILE)$(MAKE_APERL_FILE) static $(MMS$TARGET)
};
	push @m, map( " \\\n\t\t$_", @ARGV );
	push @m, "\n";

	return join '', @m;
    }


    my($linkcmd,@optlibs,@staticpkgs,$extralist,$targdir,$libperldir,%libseen);
    local($_);

    # The front matter of the linkcommand...
    $linkcmd = join ' ', $Config{'ld'},
	    grep($_, @Config{qw(large split ldflags ccdlflags)});
    $linkcmd =~ s/\s+/ /g;

    # Which *.olb files could we make use of...
    local(%olbs);
    $olbs{$self->{INST_ARCHAUTODIR}} = "$self->{BASEEXT}\$(LIB_EXT)";
    require File::Find;
    File::Find::find(sub {
	return unless m/\Q$self->{LIB_EXT}\E$/;
	return if m/^libperl/;

	if( exists $self->{INCLUDE_EXT} ){
		my $found = 0;
		my $incl;
		my $xx;

		($xx = $File::Find::name) =~ s,.*?/auto/,,;
		$xx =~ s,/?$_,,;
		$xx =~ s,/,::,g;

		# Throw away anything not explicitly marked for inclusion.
		# DynaLoader is implied.
		foreach $incl ((@{$self->{INCLUDE_EXT}},'DynaLoader')){
			if( $xx eq $incl ){
				$found++;
				last;
			}
		}
		return unless $found;
	}
	elsif( exists $self->{EXCLUDE_EXT} ){
		my $excl;
		my $xx;

		($xx = $File::Find::name) =~ s,.*?/auto/,,;
		$xx =~ s,/?$_,,;
		$xx =~ s,/,::,g;

		# Throw away anything explicitly marked for exclusion
		foreach $excl (@{$self->{EXCLUDE_EXT}}){
			return if( $xx eq $excl );
		}
	}

	$olbs{$ENV{DEFAULT}} = $_;
    }, grep( -d $_, @{$searchdirs || []}));

    # We trust that what has been handed in as argument will be buildable
    $static = [] unless $static;
    @olbs{@{$static}} = (1) x @{$static};
 
    $extra = [] unless $extra && ref $extra eq 'ARRAY';
    # Sort the object libraries in inverse order of
    # filespec length to try to insure that dependent extensions
    # will appear before their parents, so the linker will
    # search the parent library to resolve references.
    # (e.g. Intuit::DWIM will precede Intuit, so unresolved
    # references from [.intuit.dwim]dwim.obj can be found
    # in [.intuit]intuit.olb).
    for (sort { length($a) <=> length($b) } keys %olbs) {
	next unless $olbs{$_} =~ /\Q$self->{LIB_EXT}\E$/;
	my($dir) = $self->fixpath($_,1);
	my($extralibs) = $dir . "extralibs.ld";
	my($extopt) = $dir . $olbs{$_};
	$extopt =~ s/$self->{LIB_EXT}$/.opt/;
	push @optlibs, "$dir$olbs{$_}";
	# Get external libraries this extension will need
	if (-f $extralibs ) {
	    my %seenthis;
	    open LIST,$extralibs or warn $!,next;
	    while (<LIST>) {
		chomp;
		# Include a library in the link only once, unless it's mentioned
		# multiple times within a single extension's options file, in which
		# case we assume the builder needed to search it again later in the
		# link.
		my $skip = exists($libseen{$_}) && !exists($seenthis{$_});
		$libseen{$_}++;  $seenthis{$_}++;
		next if $skip;
		push @$extra,$_;
	    }
	    close LIST;
	}
	# Get full name of extension for ExtUtils::Miniperl
	if (-f $extopt) {
	    open OPT,$extopt or die $!;
	    while (<OPT>) {
		next unless /(?:UNIVERSAL|VECTOR)=boot_([\w_]+)/;
		my $pkg = $1;
		$pkg =~ s#__*#::#g;
		push @staticpkgs,$pkg;
	    }
	}
    }
    # Place all of the external libraries after all of the Perl extension
    # libraries in the final link, in order to maximize the opportunity
    # for XS code from multiple extensions to resolve symbols against the
    # same external library while only including that library once.
    push @optlibs, @$extra;

    $target = "Perl$Config{'exe_ext'}" unless $target;
    ($shrtarget,$targdir) = fileparse($target);
    $shrtarget =~ s/^([^.]*)/$1Shr/;
    $shrtarget = $targdir . $shrtarget;
    $target = "Perlshr.$Config{'dlext'}" unless $target;
    $tmp = "[]" unless $tmp;
    $tmp = $self->fixpath($tmp,1);
    if (@optlibs) { $extralist = join(' ',@optlibs); }
    else          { $extralist = ''; }
    # Let ExtUtils::Liblist find the necessary for us (but skip PerlShr;
    # that's what we're building here).
    push @optlibs, grep { !/PerlShr/i } split +($self->ext())[2];
    if ($libperl) {
	unless (-f $libperl || -f ($libperl = $self->catfile($Config{'installarchlib'},'CORE',$libperl))) {
	    print STDOUT "Warning: $libperl not found\n";
	    undef $libperl;
	}
    }
    unless ($libperl) {
	if (defined $self->{PERL_SRC}) {
	    $libperl = $self->catfile($self->{PERL_SRC},"libperl$self->{LIB_EXT}");
	} elsif (-f ($libperl = $self->catfile($Config{'installarchlib'},'CORE',"libperl$self->{LIB_EXT}")) ) {
	} else {
	    print STDOUT "Warning: $libperl not found
    If you're going to build a static perl binary, make sure perl is installed
    otherwise ignore this warning\n";
	}
    }
    $libperldir = $self->fixpath((fileparse($libperl))[1],1);

    push @m, '
# Fill in the target you want to produce if it\'s not perl
MAP_TARGET    = ',$self->fixpath($target,0),'
MAP_SHRTARGET = ',$self->fixpath($shrtarget,0),"
MAP_LINKCMD   = $linkcmd
MAP_PERLINC   = ", $perlinc ? map('"$_" ',@{$perlinc}) : '',"
MAP_EXTRA     = $extralist
MAP_LIBPERL = ",$self->fixpath($libperl,0),'
';


    push @m,"\n${tmp}Makeaperl.Opt : \$(MAP_EXTRA)\n";
    foreach (@optlibs) {
	push @m,'	$(NOECHO) $(PERL) -e "print q{',$_,'}" >>$(MMS$TARGET)',"\n";
    }
    push @m,"\n${tmp}PerlShr.Opt :\n\t";
    push @m,'$(NOECHO) $(PERL) -e "print q{$(MAP_SHRTARGET)}" >$(MMS$TARGET)',"\n";

push @m,'
$(MAP_SHRTARGET) : $(MAP_LIBPERL) Makeaperl.Opt ',"${libperldir}Perlshr_Attr.Opt",'
	$(MAP_LINKCMD)/Shareable=$(MMS$TARGET) $(MAP_LIBPERL), Makeaperl.Opt/Option ',"${libperldir}Perlshr_Attr.Opt/Option",'
$(MAP_TARGET) : $(MAP_SHRTARGET) ',"${tmp}perlmain\$(OBJ_EXT) ${tmp}PerlShr.Opt",'
	$(MAP_LINKCMD) ',"${tmp}perlmain\$(OBJ_EXT)",', PerlShr.Opt/Option
	$(NOECHO) $(SAY) "To install the new ""$(MAP_TARGET)"" binary, say"
	$(NOECHO) $(SAY) "    $(MMS)$(MMSQUALIFIERS)$(USEMAKEFILE)$(MAKEFILE) inst_perl $(USEMACROS)MAP_TARGET=$(MAP_TARGET)$(ENDMACRO)"
	$(NOECHO) $(SAY) "To remove the intermediate files, say
	$(NOECHO) $(SAY) "    $(MMS)$(MMSQUALIFIERS)$(USEMAKEFILE)$(MAKEFILE) map_clean"
';
    push @m,"\n${tmp}perlmain.c : \$(MAKEFILE)\n\t\$(NOECHO) \$(PERL) -e 1 >${tmp}Writemain.tmp\n";
    push @m, "# More from the 255-char line length limit\n";
    foreach (@staticpkgs) {
	push @m,'	$(NOECHO) $(PERL) -e "print q{',$_,qq[}" >>${tmp}Writemain.tmp\n];
    }
	push @m,'
	$(NOECHO) $(PERL) $(MAP_PERLINC) -ane "use ExtUtils::Miniperl; writemain(@F)" ',$tmp,'Writemain.tmp >$(MMS$TARGET)
	$(NOECHO) $(RM_F) ',"${tmp}Writemain.tmp\n";

    push @m, q[
# Still more from the 255-char line length limit
doc_inst_perl :
	$(NOECHO) $(PERL) -e "print 'Perl binary $(MAP_TARGET)|'" >.MM_tmp
	$(NOECHO) $(PERL) -e "print 'MAP_STATIC|$(MAP_STATIC)|'" >>.MM_tmp
	$(NOECHO) $(PERL) -pl040 -e " " ].$self->catfile('$(INST_ARCHAUTODIR)','extralibs.all'),q[ >>.MM_tmp
	$(NOECHO) $(PERL) -e "print 'MAP_LIBPERL|$(MAP_LIBPERL)|'" >>.MM_tmp
	$(DOC_INSTALL) <.MM_tmp >>].$self->catfile('$(INSTALLARCHLIB)','perllocal.pod').q[
	$(NOECHO) Delete/NoLog/NoConfirm .MM_tmp;
];

    push @m, "
inst_perl : pure_inst_perl doc_inst_perl
	\$(NOECHO) \$(NOOP)

pure_inst_perl : \$(MAP_TARGET)
	$self->{CP} \$(MAP_SHRTARGET) ",$self->fixpath($Config{'installbin'},1),"
	$self->{CP} \$(MAP_TARGET) ",$self->fixpath($Config{'installbin'},1),"

clean :: map_clean
	\$(NOECHO) \$(NOOP)

map_clean :
	\$(RM_F) ${tmp}perlmain\$(OBJ_EXT) ${tmp}perlmain.c \$(MAKEFILE)
	\$(RM_F) ${tmp}Makeaperl.Opt ${tmp}PerlShr.Opt \$(MAP_TARGET)
";

    join '', @m;
}
  
# --- Output postprocessing section ---

=item nicetext (override)

Insure that colons marking targets are preceded by space, in order
to distinguish the target delimiter from a colon appearing as
part of a filespec.

=cut

sub nicetext {

    my($self,$text) = @_;
    $text =~ s/([^\s:])(:+\s)/$1 $2/gs;
    $text;
}

1;

=back

=cut

__END__


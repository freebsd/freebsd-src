package ExtUtils::MM_Unix;

use Exporter ();
use Config;
use File::Basename qw(basename dirname fileparse);
use DirHandle;
use strict;
use vars qw($VERSION $Is_Mac $Is_OS2 $Is_VMS $Is_Win32 $Is_Dos $Is_PERL_OBJECT
	    $Verbose %pm %static $Xsubpp_Version);

$VERSION = substr q$Revision: 1.1.1.2 $, 10;
# $Id: MM_Unix.pm,v 1.1.1.2 1999/05/02 14:25:31 markm Exp $

Exporter::import('ExtUtils::MakeMaker',
	qw( $Verbose &neatvalue));

$Is_OS2 = $^O eq 'os2';
$Is_Mac = $^O eq 'MacOS';
$Is_Win32 = $^O eq 'MSWin32';
$Is_Dos = $^O eq 'dos';

$Is_PERL_OBJECT = $Config{'ccflags'} =~ /-DPERL_OBJECT/;

if ($Is_VMS = $^O eq 'VMS') {
    require VMS::Filespec;
    import VMS::Filespec qw( &vmsify );
}

=head1 NAME

ExtUtils::MM_Unix - methods used by ExtUtils::MakeMaker

=head1 SYNOPSIS

C<require ExtUtils::MM_Unix;>

=head1 DESCRIPTION

The methods provided by this package are designed to be used in
conjunction with ExtUtils::MakeMaker. When MakeMaker writes a
Makefile, it creates one or more objects that inherit their methods
from a package C<MM>. MM itself doesn't provide any methods, but it
ISA ExtUtils::MM_Unix class. The inheritance tree of MM lets operating
specific packages take the responsibility for all the methods provided
by MM_Unix. We are trying to reduce the number of the necessary
overrides by defining rather primitive operations within
ExtUtils::MM_Unix.

If you are going to write a platform specific MM package, please try
to limit the necessary overrides to primitive methods, and if it is not
possible to do so, let's work out how to achieve that gain.

If you are overriding any of these methods in your Makefile.PL (in the
MY class), please report that to the makemaker mailing list. We are
trying to minimize the necessary method overrides and switch to data
driven Makefile.PLs wherever possible. In the long run less methods
will be overridable via the MY class.

=head1 METHODS

The following description of methods is still under
development. Please refer to the code for not suitably documented
sections and complain loudly to the makemaker mailing list.

Not all of the methods below are overridable in a
Makefile.PL. Overridable methods are marked as (o). All methods are
overridable by a platform specific MM_*.pm file (See
L<ExtUtils::MM_VMS>) and L<ExtUtils::MM_OS2>).

=head2 Preloaded methods

=over 2

=item canonpath

No physical check on the filesystem, but a logical cleanup of a
path. On UNIX eliminated successive slashes and successive "/.".

=cut

sub canonpath {
    my($self,$path) = @_;
    my $node = '';
    if ( $^O eq 'qnx' && $path =~ s|^(//\d+)/|/| ) {
      $node = $1;
    }
    $path =~ s|(?<=[^/])/+|/|g ;                   # xx////xx  -> xx/xx
    $path =~ s|(/\.)+/|/|g ;                       # xx/././xx -> xx/xx
    $path =~ s|^(\./)+|| unless $path eq "./";     # ./xx      -> xx
    $path =~ s|(?<=[^/])/$|| ;                     # xx/       -> xx
    "$node$path";
}

=item catdir

Concatenate two or more directory names to form a complete path ending
with a directory. But remove the trailing slash from the resulting
string, because it doesn't look good, isn't necessary and confuses
OS2. Of course, if this is the root directory, don't cut off the
trailing slash :-)

=cut

# ';

sub catdir {
    my $self = shift @_;
    my @args = @_;
    for (@args) {
	# append a slash to each argument unless it has one there
	$_ .= "/" if $_ eq '' or substr($_,-1) ne "/";
    }
    $self->canonpath(join('', @args));
}

=item catfile

Concatenate one or more directory names and a filename to form a
complete path ending with a filename

=cut

sub catfile {
    my $self = shift @_;
    my $file = pop @_;
    return $self->canonpath($file) unless @_;
    my $dir = $self->catdir(@_);
    for ($dir) {
	$_ .= "/" unless substr($_,length($_)-1,1) eq "/";
    }
    return $self->canonpath($dir.$file);
}

=item curdir

Returns a string representing of the current directory.  "." on UNIX.

=cut

sub curdir {
    return "." ;
}

=item rootdir

Returns a string representing of the root directory.  "/" on UNIX.

=cut

sub rootdir {
    return "/";
}

=item updir

Returns a string representing of the parent directory.  ".." on UNIX.

=cut

sub updir {
    return "..";
}

sub ExtUtils::MM_Unix::c_o ;
sub ExtUtils::MM_Unix::clean ;
sub ExtUtils::MM_Unix::const_cccmd ;
sub ExtUtils::MM_Unix::const_config ;
sub ExtUtils::MM_Unix::const_loadlibs ;
sub ExtUtils::MM_Unix::constants ;
sub ExtUtils::MM_Unix::depend ;
sub ExtUtils::MM_Unix::dir_target ;
sub ExtUtils::MM_Unix::dist ;
sub ExtUtils::MM_Unix::dist_basics ;
sub ExtUtils::MM_Unix::dist_ci ;
sub ExtUtils::MM_Unix::dist_core ;
sub ExtUtils::MM_Unix::dist_dir ;
sub ExtUtils::MM_Unix::dist_test ;
sub ExtUtils::MM_Unix::dlsyms ;
sub ExtUtils::MM_Unix::dynamic ;
sub ExtUtils::MM_Unix::dynamic_bs ;
sub ExtUtils::MM_Unix::dynamic_lib ;
sub ExtUtils::MM_Unix::exescan ;
sub ExtUtils::MM_Unix::export_list ;
sub ExtUtils::MM_Unix::extliblist ;
sub ExtUtils::MM_Unix::file_name_is_absolute ;
sub ExtUtils::MM_Unix::find_perl ;
sub ExtUtils::MM_Unix::fixin ;
sub ExtUtils::MM_Unix::force ;
sub ExtUtils::MM_Unix::guess_name ;
sub ExtUtils::MM_Unix::has_link_code ;
sub ExtUtils::MM_Unix::init_dirscan ;
sub ExtUtils::MM_Unix::init_main ;
sub ExtUtils::MM_Unix::init_others ;
sub ExtUtils::MM_Unix::install ;
sub ExtUtils::MM_Unix::installbin ;
sub ExtUtils::MM_Unix::libscan ;
sub ExtUtils::MM_Unix::linkext ;
sub ExtUtils::MM_Unix::lsdir ;
sub ExtUtils::MM_Unix::macro ;
sub ExtUtils::MM_Unix::makeaperl ;
sub ExtUtils::MM_Unix::makefile ;
sub ExtUtils::MM_Unix::manifypods ;
sub ExtUtils::MM_Unix::maybe_command ;
sub ExtUtils::MM_Unix::maybe_command_in_dirs ;
sub ExtUtils::MM_Unix::needs_linking ;
sub ExtUtils::MM_Unix::nicetext ;
sub ExtUtils::MM_Unix::parse_version ;
sub ExtUtils::MM_Unix::pasthru ;
sub ExtUtils::MM_Unix::path ;
sub ExtUtils::MM_Unix::perl_archive;
sub ExtUtils::MM_Unix::perl_script ;
sub ExtUtils::MM_Unix::perldepend ;
sub ExtUtils::MM_Unix::pm_to_blib ;
sub ExtUtils::MM_Unix::post_constants ;
sub ExtUtils::MM_Unix::post_initialize ;
sub ExtUtils::MM_Unix::postamble ;
sub ExtUtils::MM_Unix::ppd ;
sub ExtUtils::MM_Unix::prefixify ;
sub ExtUtils::MM_Unix::processPL ;
sub ExtUtils::MM_Unix::realclean ;
sub ExtUtils::MM_Unix::replace_manpage_separator ;
sub ExtUtils::MM_Unix::static ;
sub ExtUtils::MM_Unix::static_lib ;
sub ExtUtils::MM_Unix::staticmake ;
sub ExtUtils::MM_Unix::subdir_x ;
sub ExtUtils::MM_Unix::subdirs ;
sub ExtUtils::MM_Unix::test ;
sub ExtUtils::MM_Unix::test_via_harness ;
sub ExtUtils::MM_Unix::test_via_script ;
sub ExtUtils::MM_Unix::tool_autosplit ;
sub ExtUtils::MM_Unix::tool_xsubpp ;
sub ExtUtils::MM_Unix::tools_other ;
sub ExtUtils::MM_Unix::top_targets ;
sub ExtUtils::MM_Unix::writedoc ;
sub ExtUtils::MM_Unix::xs_c ;
sub ExtUtils::MM_Unix::xs_cpp ;
sub ExtUtils::MM_Unix::xs_o ;
sub ExtUtils::MM_Unix::xsubpp_version ;

package ExtUtils::MM_Unix;

use SelfLoader;

1;

__DATA__

=back

=head2 SelfLoaded methods

=over 2

=item c_o (o)

Defines the suffix rules to compile different flavors of C files to
object files.

=cut

sub c_o {
# --- Translation Sections ---

    my($self) = shift;
    return '' unless $self->needs_linking();
    my(@m);
    push @m, '
.c$(OBJ_EXT):
	$(CCCMD) $(CCCDLFLAGS) -I$(PERL_INC) $(DEFINE) $*.c
';
    push @m, '
.C$(OBJ_EXT):
	$(CCCMD) $(CCCDLFLAGS) -I$(PERL_INC) $(DEFINE) $*.C
' if $^O ne 'os2' and $^O ne 'MSWin32' and $^O ne 'dos'; #Case-specific
    push @m, '
.cpp$(OBJ_EXT):
	$(CCCMD) $(CCCDLFLAGS) -I$(PERL_INC) $(DEFINE) $*.cpp

.cxx$(OBJ_EXT):
	$(CCCMD) $(CCCDLFLAGS) -I$(PERL_INC) $(DEFINE) $*.cxx

.cc$(OBJ_EXT):
	$(CCCMD) $(CCCDLFLAGS) -I$(PERL_INC) $(DEFINE) $*.cc
';
    join "", @m;
}

=item cflags (o)

Does very much the same as the cflags script in the perl
distribution. It doesn't return the whole compiler command line, but
initializes all of its parts. The const_cccmd method then actually
returns the definition of the CCCMD macro which uses these parts.

=cut

#'

sub cflags {
    my($self,$libperl)=@_;
    return $self->{CFLAGS} if $self->{CFLAGS};
    return '' unless $self->needs_linking();

    my($prog, $uc, $perltype, %cflags);
    $libperl ||= $self->{LIBPERL_A} || "libperl$self->{LIB_EXT}" ;
    $libperl =~ s/\.\$\(A\)$/$self->{LIB_EXT}/;

    @cflags{qw(cc ccflags optimize large split shellflags)}
	= @Config{qw(cc ccflags optimize large split shellflags)};
    my($optdebug) = "";

    $cflags{shellflags} ||= '';

    my(%map) =  (
		D =>   '-DDEBUGGING',
		E =>   '-DEMBED',
		DE =>  '-DDEBUGGING -DEMBED',
		M =>   '-DEMBED -DMULTIPLICITY',
		DM =>  '-DDEBUGGING -DEMBED -DMULTIPLICITY',
		);

    if ($libperl =~ /libperl(\w*)\Q$self->{LIB_EXT}/){
	$uc = uc($1);
    } else {
	$uc = ""; # avoid warning
    }
    $perltype = $map{$uc} ? $map{$uc} : "";

    if ($uc =~ /^D/) {
	$optdebug = "-g";
    }


    my($name);
    ( $name = $self->{NAME} . "_cflags" ) =~ s/:/_/g ;
    if ($prog = $Config::Config{$name}) {
	# Expand hints for this extension via the shell
	print STDOUT "Processing $name hint:\n" if $Verbose;
	my(@o)=`cc=\"$cflags{cc}\"
	  ccflags=\"$cflags{ccflags}\"
	  optimize=\"$cflags{optimize}\"
	  perltype=\"$cflags{perltype}\"
	  optdebug=\"$cflags{optdebug}\"
	  large=\"$cflags{large}\"
	  split=\"$cflags{'split'}\"
	  eval '$prog'
	  echo cc=\$cc
	  echo ccflags=\$ccflags
	  echo optimize=\$optimize
	  echo perltype=\$perltype
	  echo optdebug=\$optdebug
	  echo large=\$large
	  echo split=\$split
	  `;
	my($line);
	foreach $line (@o){
	    chomp $line;
	    if ($line =~ /(.*?)=\s*(.*)\s*$/){
		$cflags{$1} = $2;
		print STDOUT "	$1 = $2\n" if $Verbose;
	    } else {
		print STDOUT "Unrecognised result from hint: '$line'\n";
	    }
	}
    }

    if ($optdebug) {
	$cflags{optimize} = $optdebug;
    }

    for (qw(ccflags optimize perltype large split)) {
	$cflags{$_} =~ s/^\s+//;
	$cflags{$_} =~ s/\s+/ /g;
	$cflags{$_} =~ s/\s+$//;
	$self->{uc $_} ||= $cflags{$_}
    }

    if ($self->{CAPI} && $Is_PERL_OBJECT) {
        $self->{CCFLAGS} =~ s/-DPERL_OBJECT(\s|$)//;
        $self->{CCFLAGS} .= ' -DPERL_CAPI ';
        if ($Is_Win32 && $Config{'cc'} =~ /^cl.exe/i) {
            # Turn off C++ mode of the MSC compiler
            $self->{CCFLAGS} =~ s/-TP(\s|$)//;
            $self->{OPTIMIZE} =~ s/-TP(\s|$)//;
        }
    }
    return $self->{CFLAGS} = qq{
CCFLAGS = $self->{CCFLAGS}
OPTIMIZE = $self->{OPTIMIZE}
PERLTYPE = $self->{PERLTYPE}
LARGE = $self->{LARGE}
SPLIT = $self->{SPLIT}
};

}

=item clean (o)

Defines the clean target.

=cut

sub clean {
# --- Cleanup and Distribution Sections ---

    my($self, %attribs) = @_;
    my(@m,$dir);
    push(@m, '
# Delete temporary files but do not touch installed files. We don\'t delete
# the Makefile here so a later make realclean still has a makefile to use.

clean ::
');
    # clean subdirectories first
    for $dir (@{$self->{DIR}}) {
	push @m, "\t-cd $dir && \$(TEST_F) $self->{MAKEFILE} && \$(MAKE) clean\n";
    }

    my(@otherfiles) = values %{$self->{XS}}; # .c files from *.xs files
    push(@otherfiles, $attribs{FILES}) if $attribs{FILES};
    push(@otherfiles, qw[./blib $(MAKE_APERL_FILE) $(INST_ARCHAUTODIR)/extralibs.all
			 perlmain.c mon.out core so_locations pm_to_blib
			 *~ */*~ */*/*~ *$(OBJ_EXT) *$(LIB_EXT) perl.exe
			 $(BOOTSTRAP) $(BASEEXT).bso $(BASEEXT).def
			 $(BASEEXT).exp
			]);
    push @m, "\t-$self->{RM_RF} @otherfiles\n";
    # See realclean and ext/utils/make_ext for usage of Makefile.old
    push(@m,
	 "\t-$self->{MV} $self->{MAKEFILE} $self->{MAKEFILE}.old \$(DEV_NULL)\n");
    push(@m,
	 "\t$attribs{POSTOP}\n")   if $attribs{POSTOP};
    join("", @m);
}

=item const_cccmd (o)

Returns the full compiler call for C programs and stores the
definition in CONST_CCCMD.

=cut

sub const_cccmd {
    my($self,$libperl)=@_;
    return $self->{CONST_CCCMD} if $self->{CONST_CCCMD};
    return '' unless $self->needs_linking();
    return $self->{CONST_CCCMD} =
	q{CCCMD = $(CC) -c $(INC) $(CCFLAGS) $(OPTIMIZE) \\
	$(PERLTYPE) $(LARGE) $(SPLIT) $(DEFINE_VERSION) \\
	$(XS_DEFINE_VERSION)};
}

=item const_config (o)

Defines a couple of constants in the Makefile that are imported from
%Config.

=cut

sub const_config {
# --- Constants Sections ---

    my($self) = shift;
    my(@m,$m);
    push(@m,"\n# These definitions are from config.sh (via $INC{'Config.pm'})\n");
    push(@m,"\n# They may have been overridden via Makefile.PL or on the command line\n");
    my(%once_only);
    foreach $m (@{$self->{CONFIG}}){
	# SITE*EXP macros are defined in &constants; avoid duplicates here
	next if $once_only{$m} or $m eq 'sitelibexp' or $m eq 'sitearchexp';
	push @m, "\U$m\E = ".$self->{uc $m}."\n";
	$once_only{$m} = 1;
    }
    join('', @m);
}

=item const_loadlibs (o)

Defines EXTRALIBS, LDLOADLIBS, BSLOADLIBS, LD_RUN_PATH. See
L<ExtUtils::Liblist> for details.

=cut

sub const_loadlibs {
    my($self) = shift;
    return "" unless $self->needs_linking;
    my @m;
    push @m, qq{
# $self->{NAME} might depend on some other libraries:
# See ExtUtils::Liblist for details
#
};
    my($tmp);
    for $tmp (qw/
	 EXTRALIBS LDLOADLIBS BSLOADLIBS LD_RUN_PATH
	 /) {
	next unless defined $self->{$tmp};
	push @m, "$tmp = $self->{$tmp}\n";
    }
    return join "", @m;
}

=item constants (o)

Initializes lots of constants and .SUFFIXES and .PHONY

=cut

sub constants {
    my($self) = @_;
    my(@m,$tmp);

    for $tmp (qw/

	      AR_STATIC_ARGS NAME DISTNAME NAME_SYM VERSION
	      VERSION_SYM XS_VERSION INST_BIN INST_EXE INST_LIB
	      INST_ARCHLIB INST_SCRIPT PREFIX  INSTALLDIRS
	      INSTALLPRIVLIB INSTALLARCHLIB INSTALLSITELIB
	      INSTALLSITEARCH INSTALLBIN INSTALLSCRIPT PERL_LIB
	      PERL_ARCHLIB SITELIBEXP SITEARCHEXP LIBPERL_A MYEXTLIB
	      FIRST_MAKEFILE MAKE_APERL_FILE PERLMAINCC PERL_SRC
	      PERL_INC PERL FULLPERL

	      / ) {
	next unless defined $self->{$tmp};
	push @m, "$tmp = $self->{$tmp}\n";
    }

    push @m, qq{
VERSION_MACRO = VERSION
DEFINE_VERSION = -D\$(VERSION_MACRO)=\\\"\$(VERSION)\\\"
XS_VERSION_MACRO = XS_VERSION
XS_DEFINE_VERSION = -D\$(XS_VERSION_MACRO)=\\\"\$(XS_VERSION)\\\"
};

    push @m, qq{
MAKEMAKER = $INC{'ExtUtils/MakeMaker.pm'}
MM_VERSION = $ExtUtils::MakeMaker::VERSION
};

    push @m, q{
# FULLEXT = Pathname for extension directory (eg Foo/Bar/Oracle).
# BASEEXT = Basename part of FULLEXT. May be just equal FULLEXT. (eg Oracle)
# ROOTEXT = Directory part of FULLEXT with leading slash (eg /DBD)  !!! Deprecated from MM 5.32  !!!
# PARENT_NAME = NAME without BASEEXT and no trailing :: (eg Foo::Bar)
# DLBASE  = Basename part of dynamic library. May be just equal BASEEXT.
};

    for $tmp (qw/
	      FULLEXT BASEEXT PARENT_NAME DLBASE VERSION_FROM INC DEFINE OBJECT
	      LDFROM LINKTYPE
	      /	) {
	next unless defined $self->{$tmp};
	push @m, "$tmp = $self->{$tmp}\n";
    }

    push @m, "
# Handy lists of source code files:
XS_FILES= ".join(" \\\n\t", sort keys %{$self->{XS}})."
C_FILES = ".join(" \\\n\t", @{$self->{C}})."
O_FILES = ".join(" \\\n\t", @{$self->{O_FILES}})."
H_FILES = ".join(" \\\n\t", @{$self->{H}})."
MAN1PODS = ".join(" \\\n\t", sort keys %{$self->{MAN1PODS}})."
MAN3PODS = ".join(" \\\n\t", sort keys %{$self->{MAN3PODS}})."
";

    for $tmp (qw/
	      INST_MAN1DIR INSTALLMAN1DIR MAN1EXT INST_MAN3DIR INSTALLMAN3DIR MAN3EXT
	      /) {
	next unless defined $self->{$tmp};
	push @m, "$tmp = $self->{$tmp}\n";
    }

    for $tmp (qw(
		PERM_RW PERM_RWX
		)
	     ) {
        my $method = lc($tmp);
	# warn "self[$self] method[$method]";
        push @m, "$tmp = ", $self->$method(), "\n";
    }

    push @m, q{
.NO_CONFIG_REC: Makefile
} if $ENV{CLEARCASE_ROOT};

    # why not q{} ? -- emacs
    push @m, qq{
# work around a famous dec-osf make(1) feature(?):
makemakerdflt: all

.SUFFIXES: .xs .c .C .cpp .cxx .cc \$(OBJ_EXT)

# Nick wanted to get rid of .PRECIOUS. I don't remember why. I seem to recall, that
# some make implementations will delete the Makefile when we rebuild it. Because
# we call false(1) when we rebuild it. So make(1) is not completely wrong when it
# does so. Our milage may vary.
# .PRECIOUS: Makefile    # seems to be not necessary anymore

.PHONY: all config static dynamic test linkext manifest

# Where is the Config information that we are using/depend on
CONFIGDEP = \$(PERL_ARCHLIB)/Config.pm \$(PERL_INC)/config.h
};

    my @parentdir = split(/::/, $self->{PARENT_NAME});
    push @m, q{
# Where to put things:
INST_LIBDIR      = }. $self->catdir('$(INST_LIB)',@parentdir)        .q{
INST_ARCHLIBDIR  = }. $self->catdir('$(INST_ARCHLIB)',@parentdir)    .q{

INST_AUTODIR     = }. $self->catdir('$(INST_LIB)','auto','$(FULLEXT)')       .q{
INST_ARCHAUTODIR = }. $self->catdir('$(INST_ARCHLIB)','auto','$(FULLEXT)')   .q{
};

    if ($self->has_link_code()) {
	push @m, '
INST_STATIC  = $(INST_ARCHAUTODIR)/$(BASEEXT)$(LIB_EXT)
INST_DYNAMIC = $(INST_ARCHAUTODIR)/$(DLBASE).$(DLEXT)
INST_BOOT    = $(INST_ARCHAUTODIR)/$(BASEEXT).bs
';
    } else {
	push @m, '
INST_STATIC  =
INST_DYNAMIC =
INST_BOOT    =
';
    }

    $tmp = $self->export_list;
    push @m, "
EXPORT_LIST = $tmp
";
    $tmp = $self->perl_archive;
    push @m, "
PERL_ARCHIVE = $tmp
";

#    push @m, q{
#INST_PM = }.join(" \\\n\t", sort values %{$self->{PM}}).q{
#
#PM_TO_BLIB = }.join(" \\\n\t", %{$self->{PM}}).q{
#};

    push @m, q{
TO_INST_PM = }.join(" \\\n\t", sort keys %{$self->{PM}}).q{

PM_TO_BLIB = }.join(" \\\n\t", %{$self->{PM}}).q{
};

    join('',@m);
}

=item depend (o)

Same as macro for the depend attribute.

=cut

sub depend {
    my($self,%attribs) = @_;
    my(@m,$key,$val);
    while (($key,$val) = each %attribs){
	last unless defined $key;
	push @m, "$key: $val\n";
    }
    join "", @m;
}

=item dir_target (o)

Takes an array of directories that need to exist and returns a
Makefile entry for a .exists file in these directories. Returns
nothing, if the entry has already been processed. We're helpless
though, if the same directory comes as $(FOO) _and_ as "bar". Both of
them get an entry, that's why we use "::".

=cut

sub dir_target {
# --- Make-Directories section (internal method) ---
# dir_target(@array) returns a Makefile entry for the file .exists in each
# named directory. Returns nothing, if the entry has already been processed.
# We're helpless though, if the same directory comes as $(FOO) _and_ as "bar".
# Both of them get an entry, that's why we use "::". I chose '$(PERL)' as the
# prerequisite, because there has to be one, something that doesn't change
# too often :)

    my($self,@dirs) = @_;
    my(@m,$dir,$targdir);
    foreach $dir (@dirs) {
	my($src) = $self->catfile($self->{PERL_INC},'perl.h');
	my($targ) = $self->catfile($dir,'.exists');
	# catfile may have adapted syntax of $dir to target OS, so...
	if ($Is_VMS) { # Just remove file name; dirspec is often in macro
	    ($targdir = $targ) =~ s:/?\.exists$::;
	}
	else { # while elsewhere we expect to see the dir separator in $targ
	    $targdir = dirname($targ);
	}
	next if $self->{DIR_TARGET}{$self}{$targdir}++;
	push @m, qq{
$targ :: $src
	$self->{NOECHO}\$(MKPATH) $targdir
	$self->{NOECHO}\$(EQUALIZE_TIMESTAMP) $src $targ
};
	push(@m, qq{
	-$self->{NOECHO}\$(CHMOD) \$(PERM_RWX) $targdir
}) unless $Is_VMS;
    }
    join "", @m;
}

=item dist (o)

Defines a lot of macros for distribution support.

=cut

sub dist {
    my($self, %attribs) = @_;

    my(@m);
    # VERSION should be sanitised before use as a file name
    my($version)  = $attribs{VERSION}  || '$(VERSION)';
    my($name)     = $attribs{NAME}     || '$(DISTNAME)';
    my($tar)      = $attribs{TAR}      || 'tar';        # eg /usr/bin/gnutar
    my($tarflags) = $attribs{TARFLAGS} || 'cvf';
    my($zip)      = $attribs{ZIP}      || 'zip';        # eg pkzip Yuck!
    my($zipflags) = $attribs{ZIPFLAGS} || '-r';
    my($compress) = $attribs{COMPRESS} || 'gzip --best';
    my($suffix)   = $attribs{SUFFIX}   || '.gz';          # eg .gz
    my($shar)     = $attribs{SHAR}     || 'shar';       # eg "shar --gzip"
    my($preop)    = $attribs{PREOP}    || "$self->{NOECHO}\$(NOOP)"; # eg update MANIFEST
    my($postop)   = $attribs{POSTOP}   || "$self->{NOECHO}\$(NOOP)"; # eg remove the distdir

    my($to_unix)  = $attribs{TO_UNIX} || ($Is_OS2
					  ? "$self->{NOECHO}"
					  . '$(TEST_F) tmp.zip && $(RM) tmp.zip;'
					  . ' $(ZIP) -ll -mr tmp.zip $(DISTVNAME) && unzip -o tmp.zip && $(RM) tmp.zip'
					  : "$self->{NOECHO}\$(NOOP)");

    my($ci)       = $attribs{CI}       || 'ci -u';
    my($rcs_label)= $attribs{RCS_LABEL}|| 'rcs -Nv$(VERSION_SYM): -q';
    my($dist_cp)  = $attribs{DIST_CP}  || 'best';
    my($dist_default) = $attribs{DIST_DEFAULT} || 'tardist';

    push @m, "
DISTVNAME = ${name}-$version
TAR  = $tar
TARFLAGS = $tarflags
ZIP  = $zip
ZIPFLAGS = $zipflags
COMPRESS = $compress
SUFFIX = $suffix
SHAR = $shar
PREOP = $preop
POSTOP = $postop
TO_UNIX = $to_unix
CI = $ci
RCS_LABEL = $rcs_label
DIST_CP = $dist_cp
DIST_DEFAULT = $dist_default
";
    join "", @m;
}

=item dist_basics (o)

Defines the targets distclean, distcheck, skipcheck, manifest.

=cut

sub dist_basics {
    my($self) = shift;
    my @m;
    push @m, q{
distclean :: realclean distcheck
};

    push @m, q{
distcheck :
	$(PERL) -I$(PERL_ARCHLIB) -I$(PERL_LIB) -MExtUtils::Manifest=fullcheck \\
		-e fullcheck
};

    push @m, q{
skipcheck :
	$(PERL) -I$(PERL_ARCHLIB) -I$(PERL_LIB) -MExtUtils::Manifest=skipcheck \\
		-e skipcheck
};

    push @m, q{
manifest :
	$(PERL) -I$(PERL_ARCHLIB) -I$(PERL_LIB) -MExtUtils::Manifest=mkmanifest \\
		-e mkmanifest
};
    join "", @m;
}

=item dist_ci (o)

Defines a check in target for RCS.

=cut

sub dist_ci {
    my($self) = shift;
    my @m;
    push @m, q{
ci :
	$(PERL) -I$(PERL_ARCHLIB) -I$(PERL_LIB) -MExtUtils::Manifest=maniread \\
		-e "@all = keys %{ maniread() };" \\
		-e 'print("Executing $(CI) @all\n"); system("$(CI) @all");' \\
		-e 'print("Executing $(RCS_LABEL) ...\n"); system("$(RCS_LABEL) @all");'
};
    join "", @m;
}

=item dist_core (o)

Defines the targets dist, tardist, zipdist, uutardist, shdist

=cut

sub dist_core {
    my($self) = shift;
    my @m;
    push @m, q{
dist : $(DIST_DEFAULT)
	}.$self->{NOECHO}.q{$(PERL) -le 'print "Warning: Makefile possibly out of date with $$vf" if ' \
	    -e '-e ($$vf="$(VERSION_FROM)") and -M $$vf < -M "}.$self->{MAKEFILE}.q{";'

tardist : $(DISTVNAME).tar$(SUFFIX)

zipdist : $(DISTVNAME).zip

$(DISTVNAME).tar$(SUFFIX) : distdir
	$(PREOP)
	$(TO_UNIX)
	$(TAR) $(TARFLAGS) $(DISTVNAME).tar $(DISTVNAME)
	$(RM_RF) $(DISTVNAME)
	$(COMPRESS) $(DISTVNAME).tar
	$(POSTOP)

$(DISTVNAME).zip : distdir
	$(PREOP)
	$(ZIP) $(ZIPFLAGS) $(DISTVNAME).zip $(DISTVNAME)
	$(RM_RF) $(DISTVNAME)
	$(POSTOP)

uutardist : $(DISTVNAME).tar$(SUFFIX)
	uuencode $(DISTVNAME).tar$(SUFFIX) \\
		$(DISTVNAME).tar$(SUFFIX) > \\
		$(DISTVNAME).tar$(SUFFIX)_uu

shdist : distdir
	$(PREOP)
	$(SHAR) $(DISTVNAME) > $(DISTVNAME).shar
	$(RM_RF) $(DISTVNAME)
	$(POSTOP)
};
    join "", @m;
}

=item dist_dir (o)

Defines the scratch directory target that will hold the distribution
before tar-ing (or shar-ing).

=cut

sub dist_dir {
    my($self) = shift;
    my @m;
    push @m, q{
distdir :
	$(RM_RF) $(DISTVNAME)
	$(PERL) -I$(PERL_ARCHLIB) -I$(PERL_LIB) -MExtUtils::Manifest=manicopy,maniread \\
		-e "manicopy(maniread(),'$(DISTVNAME)', '$(DIST_CP)');"
};
    join "", @m;
}

=item dist_test (o)

Defines a target that produces the distribution in the
scratchdirectory, and runs 'perl Makefile.PL; make ;make test' in that
subdirectory.

=cut

sub dist_test {
    my($self) = shift;
    my @m;
    push @m, q{
disttest : distdir
	cd $(DISTVNAME) && $(PERL) -I$(PERL_ARCHLIB) -I$(PERL_LIB) Makefile.PL
	cd $(DISTVNAME) && $(MAKE)
	cd $(DISTVNAME) && $(MAKE) test
};
    join "", @m;
}

=item dlsyms (o)

Used by AIX and VMS to define DL_FUNCS and DL_VARS and write the *.exp
files.

=cut

sub dlsyms {
    my($self,%attribs) = @_;

    return '' unless ($^O eq 'aix' && $self->needs_linking() );

    my($funcs) = $attribs{DL_FUNCS} || $self->{DL_FUNCS} || {};
    my($vars)  = $attribs{DL_VARS} || $self->{DL_VARS} || [];
    my($funclist)  = $attribs{FUNCLIST} || $self->{FUNCLIST} || [];
    my(@m);

    push(@m,"
dynamic :: $self->{BASEEXT}.exp

") unless $self->{SKIPHASH}{'dynamic'}; # dynamic and static are subs, so...

    push(@m,"
static :: $self->{BASEEXT}.exp

") unless $self->{SKIPHASH}{'static'};  # we avoid a warning if we tick them

    push(@m,"
$self->{BASEEXT}.exp: Makefile.PL
",'	$(PERL) "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" -e \'use ExtUtils::Mksymlists; \\
	Mksymlists("NAME" => "',$self->{NAME},'", "DL_FUNCS" => ',
	neatvalue($funcs), ', "FUNCLIST" => ', neatvalue($funclist),
	', "DL_VARS" => ', neatvalue($vars), ');\'
');

    join('',@m);
}

=item dynamic (o)

Defines the dynamic target.

=cut

sub dynamic {
# --- Dynamic Loading Sections ---

    my($self) = shift;
    '
## $(INST_PM) has been moved to the all: target.
## It remains here for awhile to allow for old usage: "make dynamic"
#dynamic :: '.$self->{MAKEFILE}.' $(INST_DYNAMIC) $(INST_BOOT) $(INST_PM)
dynamic :: '.$self->{MAKEFILE}.' $(INST_DYNAMIC) $(INST_BOOT)
	'.$self->{NOECHO}.'$(NOOP)
';
}

=item dynamic_bs (o)

Defines targets for bootstrap files.

=cut

sub dynamic_bs {
    my($self, %attribs) = @_;
    return '
BOOTSTRAP =
' unless $self->has_link_code();

    return '
BOOTSTRAP = '."$self->{BASEEXT}.bs".'

# As Mkbootstrap might not write a file (if none is required)
# we use touch to prevent make continually trying to remake it.
# The DynaLoader only reads a non-empty file.
$(BOOTSTRAP): '."$self->{MAKEFILE} $self->{BOOTDEP}".' $(INST_ARCHAUTODIR)/.exists
	'.$self->{NOECHO}.'echo "Running Mkbootstrap for $(NAME) ($(BSLOADLIBS))"
	'.$self->{NOECHO}.'$(PERL) "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" \
		-MExtUtils::Mkbootstrap \
		-e "Mkbootstrap(\'$(BASEEXT)\',\'$(BSLOADLIBS)\');"
	'.$self->{NOECHO}.'$(TOUCH) $(BOOTSTRAP)
	$(CHMOD) $(PERM_RW) $@

$(INST_BOOT): $(BOOTSTRAP) $(INST_ARCHAUTODIR)/.exists
	'."$self->{NOECHO}$self->{RM_RF}".' $(INST_BOOT)
	-'.$self->{CP}.' $(BOOTSTRAP) $(INST_BOOT)
	$(CHMOD) $(PERM_RW) $@
';
}

=item dynamic_lib (o)

Defines how to produce the *.so (or equivalent) files.

=cut

sub dynamic_lib {
    my($self, %attribs) = @_;
    return '' unless $self->needs_linking(); #might be because of a subdir

    return '' unless $self->has_link_code;

    my($otherldflags) = $attribs{OTHERLDFLAGS} || "";
    my($inst_dynamic_dep) = $attribs{INST_DYNAMIC_DEP} || "";
    my($armaybe) = $attribs{ARMAYBE} || $self->{ARMAYBE} || ":";
    my($ldfrom) = '$(LDFROM)';
    $armaybe = 'ar' if ($^O eq 'dec_osf' and $armaybe eq ':');
    my(@m);
    push(@m,'
# This section creates the dynamically loadable $(INST_DYNAMIC)
# from $(OBJECT) and possibly $(MYEXTLIB).
ARMAYBE = '.$armaybe.'
OTHERLDFLAGS = '.$otherldflags.'
INST_DYNAMIC_DEP = '.$inst_dynamic_dep.'

$(INST_DYNAMIC): $(OBJECT) $(MYEXTLIB) $(BOOTSTRAP) $(INST_ARCHAUTODIR)/.exists $(EXPORT_LIST) $(PERL_ARCHIVE) $(INST_DYNAMIC_DEP)
');
    if ($armaybe ne ':'){
	$ldfrom = 'tmp$(LIB_EXT)';
	push(@m,'	$(ARMAYBE) cr '.$ldfrom.' $(OBJECT)'."\n");
	push(@m,'	$(RANLIB) '."$ldfrom\n");
    }
    $ldfrom = "-all $ldfrom -none" if ($^O eq 'dec_osf');

    # Brain dead solaris linker does not use LD_RUN_PATH?
    # This fixes dynamic extensions which need shared libs
    my $ldrun = '';
    $ldrun = join ' ', map "-R$_", split /:/, $self->{LD_RUN_PATH}
       if ($^O eq 'solaris');

    # The IRIX linker also doesn't use LD_RUN_PATH
    $ldrun = qq{-rpath "$self->{LD_RUN_PATH}"}
	if ($^O eq 'irix' && $self->{LD_RUN_PATH});

    push(@m,'	$(LD) -o $@ '.$ldrun.' $(LDDLFLAGS) '.$ldfrom.
		' $(OTHERLDFLAGS) $(MYEXTLIB) $(PERL_ARCHIVE) $(LDLOADLIBS) $(EXPORT_LIST)');
    push @m, '
	$(CHMOD) $(PERM_RWX) $@
';

    push @m, $self->dir_target('$(INST_ARCHAUTODIR)');
    join('',@m);
}

=item exescan

Deprecated method. Use libscan instead.

=cut

sub exescan {
    my($self,$path) = @_;
    $path;
}

=item extliblist

Called by init_others, and calls ext ExtUtils::Liblist. See
L<ExtUtils::Liblist> for details.

=cut

sub extliblist {
    my($self,$libs) = @_;
    require ExtUtils::Liblist;
    $self->ext($libs, $Verbose);
}

=item file_name_is_absolute

Takes as argument a path and returns true, if it is an absolute path.

=cut

sub file_name_is_absolute {
    my($self,$file) = @_;
    if ($Is_Dos){
        $file =~ m{^([a-z]:)?[\\/]}i ;
    }
    else {
        $file =~ m:^/: ;
    }
}

=item find_perl

Finds the executables PERL and FULLPERL

=cut

sub find_perl {
    my($self, $ver, $names, $dirs, $trace) = @_;
    my($name, $dir);
    if ($trace >= 2){
	print "Looking for perl $ver by these names:
@$names
in these dirs:
@$dirs
";
    }
    foreach $dir (@$dirs){
	next unless defined $dir; # $self->{PERL_SRC} may be undefined
	foreach $name (@$names){
	    my ($abs, $val);
	    if ($self->file_name_is_absolute($name)) { # /foo/bar
		$abs = $name;
	    } elsif ($self->canonpath($name) eq $self->canonpath(basename($name))) { # foo
		$abs = $self->catfile($dir, $name);
	    } else { # foo/bar
		$abs = $self->canonpath($self->catfile($self->curdir, $name));
	    }
	    print "Checking $abs\n" if ($trace >= 2);
	    next unless $self->maybe_command($abs);
	    print "Executing $abs\n" if ($trace >= 2);
	    $val = `$abs -e 'require $ver; print "VER_OK\n" ' 2>&1`;
	    if ($val =~ /VER_OK/) {
	        print "Using PERL=$abs\n" if $trace;
	        return $abs;
	    } elsif ($trace >= 2) {
		print "Result: `$val'\n";
	    }
	}
    }
    print STDOUT "Unable to find a perl $ver (by these names: @$names, in these dirs: @$dirs)\n";
    0; # false and not empty
}

=back

=head2 Methods to actually produce chunks of text for the Makefile

The methods here are called for each MakeMaker object in the order
specified by @ExtUtils::MakeMaker::MM_Sections.

=over 2

=item fixin

Inserts the sharpbang or equivalent magic number to a script

=cut

sub fixin { # stolen from the pink Camel book, more or less
    my($self,@files) = @_;
    my($does_shbang) = $Config::Config{'sharpbang'} =~ /^\s*\#\!/;
    my($file,$interpreter);
    for $file (@files) {
	local(*FIXIN);
	local(*FIXOUT);
	open(FIXIN, $file) or Carp::croak "Can't process '$file': $!";
	local $/ = "\n";
	chomp(my $line = <FIXIN>);
	next unless $line =~ s/^\s*\#!\s*//;     # Not a shbang file.
	# Now figure out the interpreter name.
	my($cmd,$arg) = split ' ', $line, 2;
	$cmd =~ s!^.*/!!;

	# Now look (in reverse) for interpreter in absolute PATH (unless perl).
	if ($cmd eq "perl") {
            if ($Config{startperl} =~ m,^\#!.*/perl,) {
                $interpreter = $Config{startperl};
                $interpreter =~ s,^\#!,,;
            } else {
                $interpreter = $Config{perlpath};
            }
	} else {
	    my(@absdirs) = reverse grep {$self->file_name_is_absolute} $self->path;
	    $interpreter = '';
	    my($dir);
	    foreach $dir (@absdirs) {
		if ($self->maybe_command($cmd)) {
		    warn "Ignoring $interpreter in $file\n" if $Verbose && $interpreter;
		    $interpreter = $self->catfile($dir,$cmd);
		}
	    }
	}
	# Figure out how to invoke interpreter on this machine.

	my($shb) = "";
	if ($interpreter) {
	    print STDOUT "Changing sharpbang in $file to $interpreter" if $Verbose;
	    # this is probably value-free on DOSISH platforms
	    if ($does_shbang) {
		$shb .= "$Config{'sharpbang'}$interpreter";
		$shb .= ' ' . $arg if defined $arg;
		$shb .= "\n";
	    }
	    $shb .= qq{
eval 'exec $interpreter $arg -S \$0 \${1+"\$\@"}'
    if 0; # not running under some shell
} unless $Is_Win32; # this won't work on win32, so don't
	} else {
	    warn "Can't find $cmd in PATH, $file unchanged"
		if $Verbose;
	    next;
	}

	unless ( open(FIXOUT,">$file.new") ) {
	    warn "Can't create new $file: $!\n";
	    next;
	}
	my($dev,$ino,$mode) = stat FIXIN;
	# If they override perm_rwx, we won't notice it during fixin,
	# because fixin is run through a new instance of MakeMaker.
	# That is why we must run another CHMOD later.
	$mode = oct($self->perm_rwx) unless $dev;
	chmod $mode, $file;
	
	# Print out the new #! line (or equivalent).
	local $\;
	undef $/;
	print FIXOUT $shb, <FIXIN>;
	close FIXIN;
	close FIXOUT;
	# can't rename open files on some DOSISH platforms
	unless ( rename($file, "$file.bak") ) {	
	    warn "Can't rename $file to $file.bak: $!";
	    next;
	}
	unless ( rename("$file.new", $file) ) {	
	    warn "Can't rename $file.new to $file: $!";
	    unless ( rename("$file.bak", $file) ) {
	        warn "Can't rename $file.bak back to $file either: $!";
		warn "Leaving $file renamed as $file.bak\n";
	    }
	    next;
	}
	unlink "$file.bak";
    } continue {
	chmod oct($self->perm_rwx), $file or
	  die "Can't reset permissions for $file: $!\n";
	system("$Config{'eunicefix'} $file") if $Config{'eunicefix'} ne ':';;
    }
}

=item force (o)

Just writes FORCE:

=cut

sub force {
    my($self) = shift;
    '# Phony target to force checking subdirectories.
FORCE:
	'.$self->{NOECHO}.'$(NOOP)
';
}

=item guess_name

Guess the name of this package by examining the working directory's
name. MakeMaker calls this only if the developer has not supplied a
NAME attribute.

=cut

# ';

sub guess_name {
    my($self) = @_;
    use Cwd 'cwd';
    my $name = basename(cwd());
    $name =~ s|[\-_][\d\.\-]+$||;   # this is new with MM 5.00, we
                                    # strip minus or underline
                                    # followed by a float or some such
    print "Warning: Guessing NAME [$name] from current directory name.\n";
    $name;
}

=item has_link_code

Returns true if C, XS, MYEXTLIB or similar objects exist within this
object that need a compiler. Does not descend into subdirectories as
needs_linking() does.

=cut

sub has_link_code {
    my($self) = shift;
    return $self->{HAS_LINK_CODE} if defined $self->{HAS_LINK_CODE};
    if ($self->{OBJECT} or @{$self->{C} || []} or $self->{MYEXTLIB}){
	$self->{HAS_LINK_CODE} = 1;
	return 1;
    }
    return $self->{HAS_LINK_CODE} = 0;
}

=item init_dirscan

Initializes DIR, XS, PM, C, O_FILES, H, PL_FILES, MAN*PODS, EXE_FILES.

=cut

sub init_dirscan {	# --- File and Directory Lists (.xs .pm .pod etc)
    my($self) = @_;
    my($name, %dir, %xs, %c, %h, %ignore, %pl_files, %manifypods);
    local(%pm); #the sub in find() has to see this hash
    @ignore{qw(Makefile.PL test.pl)} = (1,1);
    $ignore{'makefile.pl'} = 1 if $Is_VMS;
    foreach $name ($self->lsdir($self->curdir)){
	next if $name =~ /\#/;
	next if $name eq $self->curdir or $name eq $self->updir or $ignore{$name};
	next unless $self->libscan($name);
	if (-d $name){
	    next if -l $name; # We do not support symlinks at all
	    $dir{$name} = $name if (-f $self->catfile($name,"Makefile.PL"));
	} elsif ($name =~ /\.xs$/){
	    my($c); ($c = $name) =~ s/\.xs$/.c/;
	    $xs{$name} = $c;
	    $c{$c} = 1;
	} elsif ($name =~ /\.c(pp|xx|c)?$/i){  # .c .C .cpp .cxx .cc
	    $c{$name} = 1
		unless $name =~ m/perlmain\.c/; # See MAP_TARGET
	} elsif ($name =~ /\.h$/i){
	    $h{$name} = 1;
	} elsif ($name =~ /\.PL$/) {
	    ($pl_files{$name} = $name) =~ s/\.PL$// ;
	} elsif ($Is_VMS && $name =~ /\.pl$/) {  # case-insensitive filesystem
	    local($/); open(PL,$name); my $txt = <PL>; close PL;
	    if ($txt =~ /Extracting \S+ \(with variable substitutions/) {
		($pl_files{$name} = $name) =~ s/\.pl$// ;
	    }
	    else { $pm{$name} = $self->catfile('$(INST_LIBDIR)',$name); }
	} elsif ($name =~ /\.(p[ml]|pod)$/){
	    $pm{$name} = $self->catfile('$(INST_LIBDIR)',$name);
	}
    }

    # Some larger extensions often wish to install a number of *.pm/pl
    # files into the library in various locations.

    # The attribute PMLIBDIRS holds an array reference which lists
    # subdirectories which we should search for library files to
    # install. PMLIBDIRS defaults to [ 'lib', $self->{BASEEXT} ].  We
    # recursively search through the named directories (skipping any
    # which don't exist or contain Makefile.PL files).

    # For each *.pm or *.pl file found $self->libscan() is called with
    # the default installation path in $_[1]. The return value of
    # libscan defines the actual installation location.  The default
    # libscan function simply returns the path.  The file is skipped
    # if libscan returns false.

    # The default installation location passed to libscan in $_[1] is:
    #
    #  ./*.pm		=> $(INST_LIBDIR)/*.pm
    #  ./xyz/...	=> $(INST_LIBDIR)/xyz/...
    #  ./lib/...	=> $(INST_LIB)/...
    #
    # In this way the 'lib' directory is seen as the root of the actual
    # perl library whereas the others are relative to INST_LIBDIR
    # (which includes PARENT_NAME). This is a subtle distinction but one
    # that's important for nested modules.

    $self->{PMLIBDIRS} = ['lib', $self->{BASEEXT}]
	unless $self->{PMLIBDIRS};

    #only existing directories that aren't in $dir are allowed

    # Avoid $_ wherever possible:
    # @{$self->{PMLIBDIRS}} = grep -d && !$dir{$_}, @{$self->{PMLIBDIRS}};
    my (@pmlibdirs) = @{$self->{PMLIBDIRS}};
    my ($pmlibdir);
    @{$self->{PMLIBDIRS}} = ();
    foreach $pmlibdir (@pmlibdirs) {
	-d $pmlibdir && !$dir{$pmlibdir} && push @{$self->{PMLIBDIRS}}, $pmlibdir;
    }

    if (@{$self->{PMLIBDIRS}}){
	print "Searching PMLIBDIRS: @{$self->{PMLIBDIRS}}\n"
	    if ($Verbose >= 2);
	require File::Find;
	File::Find::find(sub {
	    if (-d $_){
		if ($_ eq "CVS" || $_ eq "RCS"){
		    $File::Find::prune = 1;
		}
		return;
	    }
	    return if /\#/;
	    my($path, $prefix) = ($File::Find::name, '$(INST_LIBDIR)');
	    my($striplibpath,$striplibname);
	    $prefix =  '$(INST_LIB)' if (($striplibpath = $path) =~ s:^(\W*)lib\W:$1:i);
	    ($striplibname,$striplibpath) = fileparse($striplibpath);
	    my($inst) = $self->catfile($prefix,$striplibpath,$striplibname);
	    local($_) = $inst; # for backwards compatibility
	    $inst = $self->libscan($inst);
	    print "libscan($path) => '$inst'\n" if ($Verbose >= 2);
	    return unless $inst;
	    $pm{$path} = $inst;
	}, @{$self->{PMLIBDIRS}});
    }

    $self->{DIR} = [sort keys %dir] unless $self->{DIR};
    $self->{XS}  = \%xs             unless $self->{XS};
    $self->{PM}  = \%pm             unless $self->{PM};
    $self->{C}   = [sort keys %c]   unless $self->{C};
    my(@o_files) = @{$self->{C}};
    $self->{O_FILES} = [grep s/\.c(pp|xx|c)?$/$self->{OBJ_EXT}/i, @o_files] ;
    $self->{H}   = [sort keys %h]   unless $self->{H};
    $self->{PL_FILES} = \%pl_files unless $self->{PL_FILES};

    # Set up names of manual pages to generate from pods
    if ($self->{MAN1PODS}) {
    } elsif ( $self->{INST_MAN1DIR} =~ /^(none|\s*)$/ ) {
    	$self->{MAN1PODS} = {};
    } else {
	my %manifypods = ();
	if ( exists $self->{EXE_FILES} ) {
	    foreach $name (@{$self->{EXE_FILES}}) {
#		use FileHandle ();
#		my $fh = new FileHandle;
		local *FH;
		my($ispod)=0;
#		if ($fh->open("<$name")) {
		if (open(FH,"<$name")) {
#		    while (<$fh>) {
		    while (<FH>) {
			if (/^=head1\s+\w+/) {
			    $ispod=1;
			    last;
			}
		    }
#		    $fh->close;
		    close FH;
		} else {
		    # If it doesn't exist yet, we assume, it has pods in it
		    $ispod = 1;
		}
		if( $ispod ) {
		    $manifypods{$name} =
			$self->catfile('$(INST_MAN1DIR)',
				       basename($name).'.$(MAN1EXT)');
		}
	    }
	}
	$self->{MAN1PODS} = \%manifypods;
    }
    if ($self->{MAN3PODS}) {
    } elsif ( $self->{INST_MAN3DIR} =~ /^(none|\s*)$/ ) {
    	$self->{MAN3PODS} = {};
    } else {
	my %manifypods = (); # we collect the keys first, i.e. the files
			     # we have to convert to pod
	foreach $name (keys %{$self->{PM}}) {
	    if ($name =~ /\.pod$/ ) {
		$manifypods{$name} = $self->{PM}{$name};
	    } elsif ($name =~ /\.p[ml]$/ ) {
#		use FileHandle ();
#		my $fh = new FileHandle;
		local *FH;
		my($ispod)=0;
#		$fh->open("<$name");
		if (open(FH,"<$name")) {
		    #		while (<$fh>) {
		    while (<FH>) {
			if (/^=head1\s+\w+/) {
			    $ispod=1;
			    last;
			}
		    }
		    #		$fh->close;
		    close FH;
		} else {
		    $ispod = 1;
		}
		if( $ispod ) {
		    $manifypods{$name} = $self->{PM}{$name};
		}
	    }
	}

	# Remove "Configure.pm" and similar, if it's not the only pod listed
	# To force inclusion, just name it "Configure.pod", or override MAN3PODS
	foreach $name (keys %manifypods) {
	    if ($name =~ /(config|setup).*\.pm/i) {
		delete $manifypods{$name};
		next;
	    }
	    my($manpagename) = $name;
	    unless ($manpagename =~ s!^\W*lib\W+!!) { # everything below lib is ok
		$manpagename = $self->catfile(split(/::/,$self->{PARENT_NAME}),$manpagename);
	    }
	    $manpagename =~ s/\.p(od|m|l)$//;
	    $manpagename = $self->replace_manpage_separator($manpagename);
	    $manifypods{$name} = $self->catfile("\$(INST_MAN3DIR)","$manpagename.\$(MAN3EXT)");
	}
	$self->{MAN3PODS} = \%manifypods;
    }
}

=item init_main

Initializes NAME, FULLEXT, BASEEXT, PARENT_NAME, DLBASE, PERL_SRC,
PERL_LIB, PERL_ARCHLIB, PERL_INC, INSTALLDIRS, INST_*, INSTALL*,
PREFIX, CONFIG, AR, AR_STATIC_ARGS, LD, OBJ_EXT, LIB_EXT, EXE_EXT, MAP_TARGET,
LIBPERL_A, VERSION_FROM, VERSION, DISTNAME, VERSION_SYM.

=cut

sub init_main {
    my($self) = @_;

    # --- Initialize Module Name and Paths

    # NAME    = Foo::Bar::Oracle
    # FULLEXT = Foo/Bar/Oracle
    # BASEEXT = Oracle
    # ROOTEXT = Directory part of FULLEXT with leading /. !!! Deprecated from MM 5.32 !!!
    # PARENT_NAME = Foo::Bar
### Only UNIX:
###    ($self->{FULLEXT} =
###     $self->{NAME}) =~ s!::!/!g ; #eg. BSD/Foo/Socket
    $self->{FULLEXT} = $self->catdir(split /::/, $self->{NAME});


    # Copied from DynaLoader:

    my(@modparts) = split(/::/,$self->{NAME});
    my($modfname) = $modparts[-1];

    # Some systems have restrictions on files names for DLL's etc.
    # mod2fname returns appropriate file base name (typically truncated)
    # It may also edit @modparts if required.
    if (defined &DynaLoader::mod2fname) {
        $modfname = &DynaLoader::mod2fname(\@modparts);
    }

    ($self->{PARENT_NAME}, $self->{BASEEXT}) = $self->{NAME} =~ m!(?:([\w:]+)::)?(\w+)$! ;

    if (defined &DynaLoader::mod2fname) {
	# As of 5.001m, dl_os2 appends '_'
	$self->{DLBASE} = $modfname;
    } else {
	$self->{DLBASE} = '$(BASEEXT)';
    }


    ### ROOTEXT deprecated from MM 5.32
###    ($self->{ROOTEXT} =
###     $self->{FULLEXT}) =~ s#/?\Q$self->{BASEEXT}\E$## ;      #eg. /BSD/Foo
###    $self->{ROOTEXT} = ($Is_VMS ? '' : '/') . $self->{ROOTEXT} if $self->{ROOTEXT};


    # --- Initialize PERL_LIB, INST_LIB, PERL_SRC

    # *Real* information: where did we get these two from? ...
    my $inc_config_dir = dirname($INC{'Config.pm'});
    my $inc_carp_dir   = dirname($INC{'Carp.pm'});

    unless ($self->{PERL_SRC}){
	my($dir);
	foreach $dir ($self->updir(),$self->catdir($self->updir(),$self->updir()),$self->catdir($self->updir(),$self->updir(),$self->updir())){
	    if (
		-f $self->catfile($dir,"config.sh")
		&&
		-f $self->catfile($dir,"perl.h")
		&&
		-f $self->catfile($dir,"lib","Exporter.pm")
	       ) {
		$self->{PERL_SRC}=$dir ;
		last;
	    }
	}
    }
    if ($self->{PERL_SRC}){
	$self->{PERL_LIB}     ||= $self->catdir("$self->{PERL_SRC}","lib");
	$self->{PERL_ARCHLIB} = $self->{PERL_LIB};
	$self->{PERL_INC}     = ($Is_Win32) ? $self->catdir($self->{PERL_LIB},"CORE") : $self->{PERL_SRC};

	# catch a situation that has occurred a few times in the past:
	unless (
		-s $self->catfile($self->{PERL_SRC},'cflags')
		or
		$Is_VMS
		&&
		-s $self->catfile($self->{PERL_SRC},'perlshr_attr.opt')
		or
		$Is_Mac
		or
		$Is_Win32
	       ){
	    warn qq{
You cannot build extensions below the perl source tree after executing
a 'make clean' in the perl source tree.

To rebuild extensions distributed with the perl source you should
simply Configure (to include those extensions) and then build perl as
normal. After installing perl the source tree can be deleted. It is
not needed for building extensions by running 'perl Makefile.PL'
usually without extra arguments.

It is recommended that you unpack and build additional extensions away
from the perl source tree.
};
	}
    } else {
	# we should also consider $ENV{PERL5LIB} here
	$self->{PERL_LIB}     ||= $Config::Config{privlibexp};
	$self->{PERL_ARCHLIB} ||= $Config::Config{archlibexp};
	$self->{PERL_INC}     = $self->catdir("$self->{PERL_ARCHLIB}","CORE"); # wild guess for now
	my $perl_h;
	unless (-f ($perl_h = $self->catfile($self->{PERL_INC},"perl.h"))){
	    die qq{
Error: Unable to locate installed Perl libraries or Perl source code.

It is recommended that you install perl in a standard location before
building extensions. Some precompiled versions of perl do not contain
these header files, so you cannot build extensions. In such a case,
please build and install your perl from a fresh perl distribution. It
usually solves this kind of problem.

\(You get this message, because MakeMaker could not find "$perl_h"\)
};
	}
#	 print STDOUT "Using header files found in $self->{PERL_INC}\n"
#	     if $Verbose && $self->needs_linking();

    }

    # We get SITELIBEXP and SITEARCHEXP directly via
    # Get_from_Config. When we are running standard modules, these
    # won't matter, we will set INSTALLDIRS to "perl". Otherwise we
    # set it to "site". I prefer that INSTALLDIRS be set from outside
    # MakeMaker.
    $self->{INSTALLDIRS} ||= "site";

    # INST_LIB typically pre-set if building an extension after
    # perl has been built and installed. Setting INST_LIB allows
    # you to build directly into, say $Config::Config{privlibexp}.
    unless ($self->{INST_LIB}){


	##### XXXXX We have to change this nonsense

	if (defined $self->{PERL_SRC} and $self->{INSTALLDIRS} eq "perl") {
	    $self->{INST_LIB} = $self->{INST_ARCHLIB} = $self->{PERL_LIB};
	} else {
	    $self->{INST_LIB} = $self->catdir($self->curdir,"blib","lib");
	}
    }
    $self->{INST_ARCHLIB} ||= $self->catdir($self->curdir,"blib","arch");
    $self->{INST_BIN} ||= $self->catdir($self->curdir,'blib','bin');

    # We need to set up INST_LIBDIR before init_libscan() for VMS
    my @parentdir = split(/::/, $self->{PARENT_NAME});
    $self->{INST_LIBDIR} = $self->catdir('$(INST_LIB)',@parentdir);
    $self->{INST_ARCHLIBDIR} = $self->catdir('$(INST_ARCHLIB)',@parentdir);
    $self->{INST_AUTODIR} = $self->catdir('$(INST_LIB)','auto','$(FULLEXT)');
    $self->{INST_ARCHAUTODIR} = $self->catdir('$(INST_ARCHLIB)','auto','$(FULLEXT)');

    # INST_EXE is deprecated, should go away March '97
    $self->{INST_EXE} ||= $self->catdir($self->curdir,'blib','script');
    $self->{INST_SCRIPT} ||= $self->catdir($self->curdir,'blib','script');

    # The user who requests an installation directory explicitly
    # should not have to tell us a architecture installation directory
    # as well. We look if a directory exists that is named after the
    # architecture. If not we take it as a sign that it should be the
    # same as the requested installation directory. Otherwise we take
    # the found one.
    # We do the same thing twice: for privlib/archlib and for sitelib/sitearch
    my($libpair);
    for $libpair ({l=>"privlib", a=>"archlib"}, {l=>"sitelib", a=>"sitearch"}) {
	my $lib = "install$libpair->{l}";
	my $Lib = uc $lib;
	my $Arch = uc "install$libpair->{a}";
	if( $self->{$Lib} && ! $self->{$Arch} ){
	    my($ilib) = $Config{$lib};
	    $ilib = VMS::Filespec::unixify($ilib) if $Is_VMS;

	    $self->prefixify($Arch,$ilib,$self->{$Lib});

	    unless (-d $self->{$Arch}) {
		print STDOUT "Directory $self->{$Arch} not found, thusly\n" if $Verbose;
		$self->{$Arch} = $self->{$Lib};
	    }
	    print STDOUT "Defaulting $Arch to $self->{$Arch}\n" if $Verbose;
	}
    }

    # we have to look at the relation between $Config{prefix} and the
    # requested values. We're going to set the $Config{prefix} part of
    # all the installation path variables to literally $(PREFIX), so
    # the user can still say make PREFIX=foo
    my($configure_prefix) = $Config{'prefix'};
    $configure_prefix = VMS::Filespec::unixify($configure_prefix) if $Is_VMS;
    $self->{PREFIX} ||= $configure_prefix;


    my($install_variable,$search_prefix,$replace_prefix);

    # The rule, taken from Configure, is that if prefix contains perl,
    # we shape the tree
    #    perlprefix/lib/                INSTALLPRIVLIB
    #    perlprefix/lib/pod/
    #    perlprefix/lib/site_perl/	INSTALLSITELIB
    #    perlprefix/bin/		INSTALLBIN
    #    perlprefix/man/		INSTALLMAN1DIR
    # else
    #    prefix/lib/perl5/		INSTALLPRIVLIB
    #    prefix/lib/perl5/pod/
    #    prefix/lib/perl5/site_perl/	INSTALLSITELIB
    #    prefix/bin/			INSTALLBIN
    #    prefix/lib/perl5/man/		INSTALLMAN1DIR

    $replace_prefix = qq[\$\(PREFIX\)];
    $search_prefix = $self->catdir($configure_prefix,"local");
    for $install_variable (qw/
			   INSTALLBIN
			   INSTALLSCRIPT
			   /) {
	$self->prefixify($install_variable,$search_prefix,$replace_prefix);
    }
    $search_prefix = $configure_prefix =~ /perl/ ?
	$self->catdir($configure_prefix,"lib") :
	$self->catdir($configure_prefix,"lib","perl5");
    if ($self->{LIB}) {
	$self->{INSTALLPRIVLIB} = $self->{INSTALLSITELIB} = $self->{LIB};
	$self->{INSTALLARCHLIB} = $self->{INSTALLSITEARCH} = 
	    $self->catdir($self->{LIB},$Config{'archname'});
    } else {
	$replace_prefix = $self->{PREFIX} =~ /perl/ ? 
	    $self->catdir(qq[\$\(PREFIX\)],"lib") :
		$self->catdir(qq[\$\(PREFIX\)],"lib","perl5");
	for $install_variable (qw/
			       INSTALLPRIVLIB
			       INSTALLARCHLIB
			       INSTALLSITELIB
			       INSTALLSITEARCH
			       /) {
	    $self->prefixify($install_variable,$search_prefix,$replace_prefix);
	}
    }
    $search_prefix = $configure_prefix =~ /perl/ ?
	$self->catdir($configure_prefix,"man") :
	    $self->catdir($configure_prefix,"lib","perl5","man");
    $replace_prefix = $self->{PREFIX} =~ /perl/ ? 
	$self->catdir(qq[\$\(PREFIX\)],"man") :
	    $self->catdir(qq[\$\(PREFIX\)],"lib","perl5","man");
    for $install_variable (qw/
			   INSTALLMAN1DIR
			   INSTALLMAN3DIR
			   /) {
	$self->prefixify($install_variable,$search_prefix,$replace_prefix);
    }

    # Now we head at the manpages. Maybe they DO NOT want manpages
    # installed
    $self->{INSTALLMAN1DIR} = $Config::Config{installman1dir}
	unless defined $self->{INSTALLMAN1DIR};
    unless (defined $self->{INST_MAN1DIR}){
	if ($self->{INSTALLMAN1DIR} =~ /^(none|\s*)$/){
	    $self->{INST_MAN1DIR} = $self->{INSTALLMAN1DIR};
	} else {
	    $self->{INST_MAN1DIR} = $self->catdir($self->curdir,'blib','man1');
	}
    }
    $self->{MAN1EXT} ||= $Config::Config{man1ext};

    $self->{INSTALLMAN3DIR} = $Config::Config{installman3dir}
	unless defined $self->{INSTALLMAN3DIR};
    unless (defined $self->{INST_MAN3DIR}){
	if ($self->{INSTALLMAN3DIR} =~ /^(none|\s*)$/){
	    $self->{INST_MAN3DIR} = $self->{INSTALLMAN3DIR};
	} else {
	    $self->{INST_MAN3DIR} = $self->catdir($self->curdir,'blib','man3');
	}
    }
    $self->{MAN3EXT} ||= $Config::Config{man3ext};


    # Get some stuff out of %Config if we haven't yet done so
    print STDOUT "CONFIG must be an array ref\n"
	if ($self->{CONFIG} and ref $self->{CONFIG} ne 'ARRAY');
    $self->{CONFIG} = [] unless (ref $self->{CONFIG});
    push(@{$self->{CONFIG}}, @ExtUtils::MakeMaker::Get_from_Config);
    push(@{$self->{CONFIG}}, 'shellflags') if $Config::Config{shellflags};
    my(%once_only,$m);
    foreach $m (@{$self->{CONFIG}}){
	next if $once_only{$m};
	print STDOUT "CONFIG key '$m' does not exist in Config.pm\n"
		unless exists $Config::Config{$m};
	$self->{uc $m} ||= $Config::Config{$m};
	$once_only{$m} = 1;
    }

# This is too dangerous:
#    if ($^O eq "next") {
#	$self->{AR} = "libtool";
#	$self->{AR_STATIC_ARGS} = "-o";
#    }
# But I leave it as a placeholder

    $self->{AR_STATIC_ARGS} ||= "cr";

    # These should never be needed
    $self->{LD} ||= 'ld';
    $self->{OBJ_EXT} ||= '.o';
    $self->{LIB_EXT} ||= '.a';

    $self->{MAP_TARGET} ||= "perl";

    $self->{LIBPERL_A} ||= "libperl$self->{LIB_EXT}";

    # make a simple check if we find Exporter
    warn "Warning: PERL_LIB ($self->{PERL_LIB}) seems not to be a perl library directory
        (Exporter.pm not found)"
	unless -f $self->catfile("$self->{PERL_LIB}","Exporter.pm") ||
        $self->{NAME} eq "ExtUtils::MakeMaker";

    # Determine VERSION and VERSION_FROM
    ($self->{DISTNAME}=$self->{NAME}) =~ s#(::)#-#g unless $self->{DISTNAME};
    if ($self->{VERSION_FROM}){
	$self->{VERSION} = $self->parse_version($self->{VERSION_FROM}) or
	    Carp::carp "WARNING: Setting VERSION via file '$self->{VERSION_FROM}' failed\n"
    }

    # strip blanks
    if ($self->{VERSION}) {
	$self->{VERSION} =~ s/^\s+//;
	$self->{VERSION} =~ s/\s+$//;
    }

    $self->{VERSION} ||= "0.10";
    ($self->{VERSION_SYM} = $self->{VERSION}) =~ s/\W/_/g;


    # Graham Barr and Paul Marquess had some ideas how to ensure
    # version compatibility between the *.pm file and the
    # corresponding *.xs file. The bottomline was, that we need an
    # XS_VERSION macro that defaults to VERSION:
    $self->{XS_VERSION} ||= $self->{VERSION};

    # --- Initialize Perl Binary Locations

    # Find Perl 5. The only contract here is that both 'PERL' and 'FULLPERL'
    # will be working versions of perl 5. miniperl has priority over perl
    # for PERL to ensure that $(PERL) is usable while building ./ext/*
    my ($component,@defpath);
    foreach $component ($self->{PERL_SRC}, $self->path(), $Config::Config{binexp}) {
	push @defpath, $component if defined $component;
    }
    $self->{PERL} ||=
        $self->find_perl(5.0, [ $^X, 'miniperl','perl','perl5',"perl$]" ],
	    \@defpath, $Verbose );
    # don't check if perl is executable, maybe they have decided to
    # supply switches with perl

    # Define 'FULLPERL' to be a non-miniperl (used in test: target)
    ($self->{FULLPERL} = $self->{PERL}) =~ s/miniperl/perl/i
	unless ($self->{FULLPERL});
}

=item init_others

Initializes EXTRALIBS, BSLOADLIBS, LDLOADLIBS, LIBS, LD_RUN_PATH,
OBJECT, BOOTDEP, PERLMAINCC, LDFROM, LINKTYPE, NOOP, FIRST_MAKEFILE,
MAKEFILE, NOECHO, RM_F, RM_RF, TEST_F, TOUCH, CP, MV, CHMOD, UMASK_NULL

=cut

sub init_others {	# --- Initialize Other Attributes
    my($self) = shift;

    # Compute EXTRALIBS, BSLOADLIBS and LDLOADLIBS from $self->{LIBS}
    # Lets look at $self->{LIBS} carefully: It may be an anon array, a string or
    # undefined. In any case we turn it into an anon array:

    # May check $Config{libs} too, thus not empty.
    $self->{LIBS}=[''] unless $self->{LIBS};

    $self->{LIBS}=[$self->{LIBS}] if ref \$self->{LIBS} eq 'SCALAR';
    $self->{LD_RUN_PATH} = "";
    my($libs);
    foreach $libs ( @{$self->{LIBS}} ){
	$libs =~ s/^\s*(.*\S)\s*$/$1/; # remove leading and trailing whitespace
	my(@libs) = $self->extliblist($libs);
	if ($libs[0] or $libs[1] or $libs[2]){
	    # LD_RUN_PATH now computed by ExtUtils::Liblist
	    ($self->{EXTRALIBS}, $self->{BSLOADLIBS}, $self->{LDLOADLIBS}, $self->{LD_RUN_PATH}) = @libs;
	    last;
	}
    }

    if ( $self->{OBJECT} ) {
	$self->{OBJECT} =~ s!\.o(bj)?\b!\$(OBJ_EXT)!g;
    } else {
	# init_dirscan should have found out, if we have C files
	$self->{OBJECT} = "";
	$self->{OBJECT} = '$(BASEEXT)$(OBJ_EXT)' if @{$self->{C}||[]};
    }
    $self->{OBJECT} =~ s/\n+/ \\\n\t/g;
    $self->{BOOTDEP}  = (-f "$self->{BASEEXT}_BS") ? "$self->{BASEEXT}_BS" : "";
    $self->{PERLMAINCC} ||= '$(CC)';
    $self->{LDFROM} = '$(OBJECT)' unless $self->{LDFROM};

    # Sanity check: don't define LINKTYPE = dynamic if we're skipping
    # the 'dynamic' section of MM.  We don't have this problem with
    # 'static', since we either must use it (%Config says we can't
    # use dynamic loading) or the caller asked for it explicitly.
    if (!$self->{LINKTYPE}) {
       $self->{LINKTYPE} = $self->{SKIPHASH}{'dynamic'}
                        ? 'static'
                        : ($Config::Config{usedl} ? 'dynamic' : 'static');
    };

    # These get overridden for VMS and maybe some other systems
    $self->{NOOP}  ||= '$(SHELL) -c true';
    $self->{FIRST_MAKEFILE} ||= "Makefile";
    $self->{MAKEFILE} ||= $self->{FIRST_MAKEFILE};
    $self->{MAKE_APERL_FILE} ||= "Makefile.aperl";
    $self->{NOECHO} = '@' unless defined $self->{NOECHO};
    $self->{RM_F}  ||= "rm -f";
    $self->{RM_RF} ||= "rm -rf";
    $self->{TOUCH} ||= "touch";
    $self->{TEST_F} ||= "test -f";
    $self->{CP} ||= "cp";
    $self->{MV} ||= "mv";
    $self->{CHMOD} ||= "chmod";
    $self->{UMASK_NULL} ||= "umask 0";
    $self->{DEV_NULL} ||= "> /dev/null 2>&1";
}

=item install (o)

Defines the install target.

=cut

sub install {
    my($self, %attribs) = @_;
    my(@m);

    push @m, q{
install :: all pure_install doc_install

install_perl :: all pure_perl_install doc_perl_install

install_site :: all pure_site_install doc_site_install

install_ :: install_site
	@echo INSTALLDIRS not defined, defaulting to INSTALLDIRS=site

pure_install :: pure_$(INSTALLDIRS)_install

doc_install :: doc_$(INSTALLDIRS)_install
	}.$self->{NOECHO}.q{echo Appending installation info to $(INSTALLARCHLIB)/perllocal.pod

pure__install : pure_site_install
	@echo INSTALLDIRS not defined, defaulting to INSTALLDIRS=site

doc__install : doc_site_install
	@echo INSTALLDIRS not defined, defaulting to INSTALLDIRS=site

pure_perl_install ::
	}.$self->{NOECHO}.q{$(MOD_INSTALL) \
		read }.$self->catfile('$(PERL_ARCHLIB)','auto','$(FULLEXT)','.packlist').q{ \
		write }.$self->catfile('$(INSTALLARCHLIB)','auto','$(FULLEXT)','.packlist').q{ \
		$(INST_LIB) $(INSTALLPRIVLIB) \
		$(INST_ARCHLIB) $(INSTALLARCHLIB) \
		$(INST_BIN) $(INSTALLBIN) \
		$(INST_SCRIPT) $(INSTALLSCRIPT) \
		$(INST_MAN1DIR) $(INSTALLMAN1DIR) \
		$(INST_MAN3DIR) $(INSTALLMAN3DIR)
	}.$self->{NOECHO}.q{$(WARN_IF_OLD_PACKLIST) \
		}.$self->catdir('$(SITEARCHEXP)','auto','$(FULLEXT)').q{


pure_site_install ::
	}.$self->{NOECHO}.q{$(MOD_INSTALL) \
		read }.$self->catfile('$(SITEARCHEXP)','auto','$(FULLEXT)','.packlist').q{ \
		write }.$self->catfile('$(INSTALLSITEARCH)','auto','$(FULLEXT)','.packlist').q{ \
		$(INST_LIB) $(INSTALLSITELIB) \
		$(INST_ARCHLIB) $(INSTALLSITEARCH) \
		$(INST_BIN) $(INSTALLBIN) \
		$(INST_SCRIPT) $(INSTALLSCRIPT) \
		$(INST_MAN1DIR) $(INSTALLMAN1DIR) \
		$(INST_MAN3DIR) $(INSTALLMAN3DIR)
	}.$self->{NOECHO}.q{$(WARN_IF_OLD_PACKLIST) \
		}.$self->catdir('$(PERL_ARCHLIB)','auto','$(FULLEXT)').q{

doc_perl_install ::
	-}.$self->{NOECHO}.q{$(DOC_INSTALL) \
		"Module" "$(NAME)" \
		"installed into" "$(INSTALLPRIVLIB)" \
		LINKTYPE "$(LINKTYPE)" \
		VERSION "$(VERSION)" \
		EXE_FILES "$(EXE_FILES)" \
		>> }.$self->catfile('$(INSTALLARCHLIB)','perllocal.pod').q{

doc_site_install ::
	-}.$self->{NOECHO}.q{$(DOC_INSTALL) \
		"Module" "$(NAME)" \
		"installed into" "$(INSTALLSITELIB)" \
		LINKTYPE "$(LINKTYPE)" \
		VERSION "$(VERSION)" \
		EXE_FILES "$(EXE_FILES)" \
		>> }.$self->catfile('$(INSTALLARCHLIB)','perllocal.pod').q{

};

    push @m, q{
uninstall :: uninstall_from_$(INSTALLDIRS)dirs

uninstall_from_perldirs ::
	}.$self->{NOECHO}.
	q{$(UNINSTALL) }.$self->catfile('$(PERL_ARCHLIB)','auto','$(FULLEXT)','.packlist').q{

uninstall_from_sitedirs ::
	}.$self->{NOECHO}.
	q{$(UNINSTALL) }.$self->catfile('$(SITEARCHEXP)','auto','$(FULLEXT)','.packlist').q{
};

    join("",@m);
}

=item installbin (o)

Defines targets to make and to install EXE_FILES.

=cut

sub installbin {
    my($self) = shift;
    return "" unless $self->{EXE_FILES} && ref $self->{EXE_FILES} eq "ARRAY";
    return "" unless @{$self->{EXE_FILES}};
    my(@m, $from, $to, %fromto, @to);
    push @m, $self->dir_target(qw[$(INST_SCRIPT)]);
    for $from (@{$self->{EXE_FILES}}) {
	my($path)= $self->catfile('$(INST_SCRIPT)', basename($from));
	local($_) = $path; # for backwards compatibility
	$to = $self->libscan($path);
	print "libscan($from) => '$to'\n" if ($Verbose >=2);
	$fromto{$from}=$to;
    }
    @to   = values %fromto;
    push(@m, qq{
EXE_FILES = @{$self->{EXE_FILES}}

} . ($Is_Win32
  ? q{FIXIN = $(PERL) -I$(PERL_ARCHLIB) -I$(PERL_LIB) \
    -e "system qq[pl2bat.bat ].shift"
} : q{FIXIN = $(PERL) -I$(PERL_ARCHLIB) -I$(PERL_LIB) -MExtUtils::MakeMaker \
    -e "MY->fixin(shift)"
}).qq{
pure_all :: @to
	$self->{NOECHO}\$(NOOP)

realclean ::
	$self->{RM_F} @to
});

    while (($from,$to) = each %fromto) {
	last unless defined $from;
	my $todir = dirname($to);
	push @m, "
$to: $from $self->{MAKEFILE} " . $self->catdir($todir,'.exists') . "
	$self->{NOECHO}$self->{RM_F} $to
	$self->{CP} $from $to
	\$(FIXIN) $to
	-$self->{NOECHO}\$(CHMOD) \$(PERM_RWX) $to
";
    }
    join "", @m;
}

=item libscan (o)

Takes a path to a file that is found by init_dirscan and returns false
if we don't want to include this file in the library. Mainly used to
exclude RCS, CVS, and SCCS directories from installation.

=cut

# ';

sub libscan {
    my($self,$path) = @_;
    return '' if $path =~ m:\b(RCS|CVS|SCCS)\b: ;
    $path;
}

=item linkext (o)

Defines the linkext target which in turn defines the LINKTYPE.

=cut

sub linkext {
    my($self, %attribs) = @_;
    # LINKTYPE => static or dynamic or ''
    my($linktype) = defined $attribs{LINKTYPE} ?
      $attribs{LINKTYPE} : '$(LINKTYPE)';
    "
linkext :: $linktype
	$self->{NOECHO}\$(NOOP)
";
}

=item lsdir

Takes as arguments a directory name and a regular expression. Returns
all entries in the directory that match the regular expression.

=cut

sub lsdir {
    my($self) = shift;
    my($dir, $regex) = @_;
    my(@ls);
    my $dh = new DirHandle;
    $dh->open($dir || ".") or return ();
    @ls = $dh->read;
    $dh->close;
    @ls = grep(/$regex/, @ls) if $regex;
    @ls;
}

=item macro (o)

Simple subroutine to insert the macros defined by the macro attribute
into the Makefile.

=cut

sub macro {
    my($self,%attribs) = @_;
    my(@m,$key,$val);
    while (($key,$val) = each %attribs){
	last unless defined $key;
	push @m, "$key = $val\n";
    }
    join "", @m;
}

=item makeaperl (o)

Called by staticmake. Defines how to write the Makefile to produce a
static new perl.

By default the Makefile produced includes all the static extensions in
the perl library. (Purified versions of library files, e.g.,
DynaLoader_pure_p1_c0_032.a are automatically ignored to avoid link errors.)

=cut

sub makeaperl {
    my($self, %attribs) = @_;
    my($makefilename, $searchdirs, $static, $extra, $perlinc, $target, $tmp, $libperl) =
	@attribs{qw(MAKE DIRS STAT EXTRA INCL TARGET TMP LIBPERL)};
    my(@m);
    push @m, "
# --- MakeMaker makeaperl section ---
MAP_TARGET    = $target
FULLPERL      = $self->{FULLPERL}
";
    return join '', @m if $self->{PARENT};

    my($dir) = join ":", @{$self->{DIR}};

    unless ($self->{MAKEAPERL}) {
	push @m, q{
$(MAP_TARGET) :: static $(MAKE_APERL_FILE)
	$(MAKE) -f $(MAKE_APERL_FILE) $@

$(MAKE_APERL_FILE) : $(FIRST_MAKEFILE)
	}.$self->{NOECHO}.q{echo Writing \"$(MAKE_APERL_FILE)\" for this $(MAP_TARGET)
	}.$self->{NOECHO}.q{$(PERL) -I$(INST_ARCHLIB) -I$(INST_LIB) -I$(PERL_ARCHLIB) -I$(PERL_LIB) \
		Makefile.PL DIR=}, $dir, q{ \
		MAKEFILE=$(MAKE_APERL_FILE) LINKTYPE=static \
		MAKEAPERL=1 NORECURS=1 CCCDLFLAGS=};

	foreach (@ARGV){
		if( /\s/ ){
			s/=(.*)/='$1'/;
		}
		push @m, " \\\n\t\t$_";
	}
#	push @m, map( " \\\n\t\t$_", @ARGV );
	push @m, "\n";

	return join '', @m;
    }



    my($cccmd, $linkcmd, $lperl);


    $cccmd = $self->const_cccmd($libperl);
    $cccmd =~ s/^CCCMD\s*=\s*//;
    $cccmd =~ s/\$\(INC\)/ -I$self->{PERL_INC} /;
    $cccmd .= " $Config::Config{cccdlflags}"
	if ($Config::Config{useshrplib} eq 'true');
    $cccmd =~ s/\(CC\)/\(PERLMAINCC\)/;

    # The front matter of the linkcommand...
    $linkcmd = join ' ', "\$(CC)",
	    grep($_, @Config{qw(large split ldflags ccdlflags)});
    $linkcmd =~ s/\s+/ /g;
    $linkcmd =~ s,(perl\.exp),\$(PERL_INC)/$1,;

    # Which *.a files could we make use of...
    local(%static);
    require File::Find;
    File::Find::find(sub {
	return unless m/\Q$self->{LIB_EXT}\E$/;
	return if m/^libperl/;
	# Skip purified versions of libraries (e.g., DynaLoader_pure_p1_c0_032.a)
	return if m/_pure_\w+_\w+_\w+\.\w+$/ and -f "$File::Find::dir/.pure";

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

	# don't include the installed version of this extension. I
	# leave this line here, although it is not necessary anymore:
	# I patched minimod.PL instead, so that Miniperl.pm won't
	# enclude duplicates

	# Once the patch to minimod.PL is in the distribution, I can
	# drop it
	return if $File::Find::name =~ m:auto/$self->{FULLEXT}/$self->{BASEEXT}$self->{LIB_EXT}$:;
	use Cwd 'cwd';
	$static{cwd() . "/" . $_}++;
    }, grep( -d $_, @{$searchdirs || []}) );

    # We trust that what has been handed in as argument, will be buildable
    $static = [] unless $static;
    @static{@{$static}} = (1) x @{$static};

    $extra = [] unless $extra && ref $extra eq 'ARRAY';
    for (sort keys %static) {
	next unless /\Q$self->{LIB_EXT}\E$/;
	$_ = dirname($_) . "/extralibs.ld";
	push @$extra, $_;
    }

    grep(s/^/-I/, @{$perlinc || []});

    $target = "perl" unless $target;
    $tmp = "." unless $tmp;

# MAP_STATIC doesn't look into subdirs yet. Once "all" is made and we
# regenerate the Makefiles, MAP_STATIC and the dependencies for
# extralibs.all are computed correctly
    push @m, "
MAP_LINKCMD   = $linkcmd
MAP_PERLINC   = @{$perlinc || []}
MAP_STATIC    = ",
join(" \\\n\t", reverse sort keys %static), "

MAP_PRELIBS   = $Config::Config{libs} $Config::Config{cryptlib}
";

    if (defined $libperl) {
	($lperl = $libperl) =~ s/\$\(A\)/$self->{LIB_EXT}/;
    }
    unless ($libperl && -f $lperl) { # Ilya's code...
	my $dir = $self->{PERL_SRC} || "$self->{PERL_ARCHLIB}/CORE";
	$libperl ||= "libperl$self->{LIB_EXT}";
	$libperl   = "$dir/$libperl";
	$lperl   ||= "libperl$self->{LIB_EXT}";
	$lperl     = "$dir/$lperl";

        if (! -f $libperl and ! -f $lperl) {
          # We did not find a static libperl. Maybe there is a shared one?
          if ($^O eq 'solaris' or $^O eq 'sunos') {
            $lperl  = $libperl = "$dir/$Config::Config{libperl}";
            # SUNOS ld does not take the full path to a shared library
            $libperl = '' if $^O eq 'sunos';
          }
        }

	print STDOUT "Warning: $libperl not found
    If you're going to build a static perl binary, make sure perl is installed
    otherwise ignore this warning\n"
		unless (-f $lperl || defined($self->{PERL_SRC}));
    }

    push @m, "
MAP_LIBPERL = $libperl
";

    push @m, "
\$(INST_ARCHAUTODIR)/extralibs.all: \$(INST_ARCHAUTODIR)/.exists ".join(" \\\n\t", @$extra)."
	$self->{NOECHO}$self->{RM_F} \$\@
	$self->{NOECHO}\$(TOUCH) \$\@
";

    my $catfile;
    foreach $catfile (@$extra){
	push @m, "\tcat $catfile >> \$\@\n";
    }
    # SUNOS ld does not take the full path to a shared library
    my $llibperl = ($libperl)?'$(MAP_LIBPERL)':'-lperl';

    # Brain dead solaris linker does not use LD_RUN_PATH?
    # This fixes dynamic extensions which need shared libs
    my $ldfrom = ($^O eq 'solaris')?
           join(' ', map "-R$_", split /:/, $self->{LD_RUN_PATH}):'';

push @m, "
\$(MAP_TARGET) :: $tmp/perlmain\$(OBJ_EXT) \$(MAP_LIBPERL) \$(MAP_STATIC) \$(INST_ARCHAUTODIR)/extralibs.all
	\$(MAP_LINKCMD) -o \$\@ \$(OPTIMIZE) $tmp/perlmain\$(OBJ_EXT) $ldfrom \$(MAP_STATIC) $llibperl `cat \$(INST_ARCHAUTODIR)/extralibs.all` \$(MAP_PRELIBS)
	$self->{NOECHO}echo 'To install the new \"\$(MAP_TARGET)\" binary, call'
	$self->{NOECHO}echo '    make -f $makefilename inst_perl MAP_TARGET=\$(MAP_TARGET)'
	$self->{NOECHO}echo 'To remove the intermediate files say'
	$self->{NOECHO}echo '    make -f $makefilename map_clean'

$tmp/perlmain\$(OBJ_EXT): $tmp/perlmain.c
";
    push @m, "\tcd $tmp && $cccmd -I\$(PERL_INC) perlmain.c\n";

    push @m, qq{
$tmp/perlmain.c: $makefilename}, q{
	}.$self->{NOECHO}.q{echo Writing $@
	}.$self->{NOECHO}.q{$(PERL) $(MAP_PERLINC) -MExtUtils::Miniperl \\
		-e "writemain(grep s#.*/auto/##, split(q| |, q|$(MAP_STATIC)|))" > $@t && $(MV) $@t $@

};
    push @m, "\t",$self->{NOECHO}.q{$(PERL) $(INSTALLSCRIPT)/fixpmain
} if (defined (&Dos::UseLFN) && Dos::UseLFN()==0);


    push @m, q{
doc_inst_perl:
	}.$self->{NOECHO}.q{echo Appending installation info to $(INSTALLARCHLIB)/perllocal.pod
	-}.$self->{NOECHO}.q{$(DOC_INSTALL) \
		"Perl binary" "$(MAP_TARGET)" \
		MAP_STATIC "$(MAP_STATIC)" \
		MAP_EXTRA "`cat $(INST_ARCHAUTODIR)/extralibs.all`" \
		MAP_LIBPERL "$(MAP_LIBPERL)" \
		>> }.$self->catfile('$(INSTALLARCHLIB)','perllocal.pod').q{

};

    push @m, q{
inst_perl: pure_inst_perl doc_inst_perl

pure_inst_perl: $(MAP_TARGET)
	}.$self->{CP}.q{ $(MAP_TARGET) }.$self->catfile('$(INSTALLBIN)','$(MAP_TARGET)').q{

clean :: map_clean

map_clean :
	}.$self->{RM_F}.qq{ $tmp/perlmain\$(OBJ_EXT) $tmp/perlmain.c \$(MAP_TARGET) $makefilename \$(INST_ARCHAUTODIR)/extralibs.all
};

    join '', @m;
}

=item makefile (o)

Defines how to rewrite the Makefile.

=cut

sub makefile {
    my($self) = shift;
    my @m;
    # We do not know what target was originally specified so we
    # must force a manual rerun to be sure. But as it should only
    # happen very rarely it is not a significant problem.
    push @m, '
$(OBJECT) : $(FIRST_MAKEFILE)
' if $self->{OBJECT};

    push @m, q{
# We take a very conservative approach here, but it\'s worth it.
# We move Makefile to Makefile.old here to avoid gnu make looping.
}.$self->{MAKEFILE}.q{ : Makefile.PL $(CONFIGDEP)
	}.$self->{NOECHO}.q{echo "Makefile out-of-date with respect to $?"
	}.$self->{NOECHO}.q{echo "Cleaning current config before rebuilding Makefile..."
	-}.$self->{NOECHO}.q{$(RM_F) }."$self->{MAKEFILE}.old".q{
	-}.$self->{NOECHO}.q{$(MV) }."$self->{MAKEFILE} $self->{MAKEFILE}.old".q{
	-$(MAKE) -f }.$self->{MAKEFILE}.q{.old clean $(DEV_NULL) || $(NOOP)
	$(PERL) "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" Makefile.PL }.join(" ",map(qq["$_"],@ARGV)).q{
	}.$self->{NOECHO}.q{echo "==> Your Makefile has been rebuilt. <=="
	}.$self->{NOECHO}.q{echo "==> Please rerun the make command.  <=="
	false

# To change behavior to :: would be nice, but would break Tk b9.02
# so you find such a warning below the dist target.
#}.$self->{MAKEFILE}.q{ :: $(VERSION_FROM)
#	}.$self->{NOECHO}.q{echo "Warning: Makefile possibly out of date with $(VERSION_FROM)"
};

    join "", @m;
}

=item manifypods (o)

Defines targets and routines to translate the pods into manpages and
put them into the INST_* directories.

=cut

sub manifypods {
    my($self, %attribs) = @_;
    return "\nmanifypods : pure_all\n\t$self->{NOECHO}\$(NOOP)\n" unless
	%{$self->{MAN3PODS}} or %{$self->{MAN1PODS}};
    my($dist);
    my($pod2man_exe);
    if (defined $self->{PERL_SRC}) {
	$pod2man_exe = $self->catfile($self->{PERL_SRC},'pod','pod2man');
    } else {
	$pod2man_exe = $self->catfile($Config{scriptdirexp},'pod2man');
    }
    unless ($self->perl_script($pod2man_exe)) {
	# No pod2man but some MAN3PODS to be installed
	print <<END;

Warning: I could not locate your pod2man program. Please make sure,
         your pod2man program is in your PATH before you execute 'make'

END
        $pod2man_exe = "-S pod2man";
    }
    my(@m);
    push @m,
qq[POD2MAN_EXE = $pod2man_exe\n],
qq[POD2MAN = \$(PERL) -we '%m=\@ARGV;for (keys %m){' \\\n],
q[-e 'next if -e $$m{$$_} && -M $$m{$$_} < -M $$_ && -M $$m{$$_} < -M "],
 $self->{MAKEFILE}, q[";' \\
-e 'print "Manifying $$m{$$_}\n";' \\
-e 'system(qq[$$^X ].q["-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" $(POD2MAN_EXE) ].qq[$$_>$$m{$$_}])==0 or warn "Couldn\\047t install $$m{$$_}\n";' \\
-e 'chmod(oct($(PERM_RW))), $$m{$$_} or warn "chmod $(PERM_RW) $$m{$$_}: $$!\n";}'
];
    push @m, "\nmanifypods : pure_all ";
    push @m, join " \\\n\t", keys %{$self->{MAN1PODS}}, keys %{$self->{MAN3PODS}};

    push(@m,"\n");
    if (%{$self->{MAN1PODS}} || %{$self->{MAN3PODS}}) {
	push @m, "\t$self->{NOECHO}\$(POD2MAN) \\\n\t";
	push @m, join " \\\n\t", %{$self->{MAN1PODS}}, %{$self->{MAN3PODS}};
    }
    join('', @m);
}

=item maybe_command

Returns true, if the argument is likely to be a command.

=cut

sub maybe_command {
    my($self,$file) = @_;
    return $file if -x $file && ! -d $file;
    return;
}

=item maybe_command_in_dirs

method under development. Not yet used. Ask Ilya :-)

=cut

sub maybe_command_in_dirs {	# $ver is optional argument if looking for perl
# Ilya's suggestion. Not yet used, want to understand it first, but at least the code is here
    my($self, $names, $dirs, $trace, $ver) = @_;
    my($name, $dir);
    foreach $dir (@$dirs){
	next unless defined $dir; # $self->{PERL_SRC} may be undefined
	foreach $name (@$names){
	    my($abs,$tryabs);
	    if ($self->file_name_is_absolute($name)) { # /foo/bar
		$abs = $name;
	    } elsif ($self->canonpath($name) eq $self->canonpath(basename($name))) { # bar
		$abs = $self->catfile($dir, $name);
	    } else { # foo/bar
		$abs = $self->catfile($self->curdir, $name);
	    }
	    print "Checking $abs for $name\n" if ($trace >= 2);
	    next unless $tryabs = $self->maybe_command($abs);
	    print "Substituting $tryabs instead of $abs\n"
		if ($trace >= 2 and $tryabs ne $abs);
	    $abs = $tryabs;
	    if (defined $ver) {
		print "Executing $abs\n" if ($trace >= 2);
		if (`$abs -e 'require $ver; print "VER_OK\n" ' 2>&1` =~ /VER_OK/) {
		    print "Using PERL=$abs\n" if $trace;
		    return $abs;
		}
	    } else { # Do not look for perl
		return $abs;
	    }
	}
    }
}

=item needs_linking (o)

Does this module need linking? Looks into subdirectory objects (see
also has_link_code())

=cut

sub needs_linking {
    my($self) = shift;
    my($child,$caller);
    $caller = (caller(0))[3];
    Carp::confess("Needs_linking called too early") if $caller =~ /^ExtUtils::MakeMaker::/;
    return $self->{NEEDS_LINKING} if defined $self->{NEEDS_LINKING};
    if ($self->has_link_code or $self->{MAKEAPERL}){
	$self->{NEEDS_LINKING} = 1;
	return 1;
    }
    foreach $child (keys %{$self->{CHILDREN}}) {
	if ($self->{CHILDREN}->{$child}->needs_linking) {
	    $self->{NEEDS_LINKING} = 1;
	    return 1;
	}
    }
    return $self->{NEEDS_LINKING} = 0;
}

=item nicetext

misnamed method (will have to be changed). The MM_Unix method just
returns the argument without further processing.

On VMS used to insure that colons marking targets are preceded by
space - most Unix Makes don't need this, but it's necessary under VMS
to distinguish the target delimiter from a colon appearing as part of
a filespec.

=cut

sub nicetext {
    my($self,$text) = @_;
    $text;
}

=item parse_version

parse a file and return what you think is $VERSION in this file set to

=cut

sub parse_version {
    my($self,$parsefile) = @_;
    my $result;
    local *FH;
    local $/ = "\n";
    open(FH,$parsefile) or die "Could not open '$parsefile': $!";
    my $inpod = 0;
    while (<FH>) {
	$inpod = /^=(?!cut)/ ? 1 : /^=cut/ ? 0 : $inpod;
	next if $inpod;
	chop;
	# next unless /\$(([\w\:\']*)\bVERSION)\b.*\=/;
	next unless /([\$*])(([\w\:\']*)\bVERSION)\b.*\=/;
	my $eval = qq{
	    package ExtUtils::MakeMaker::_version;
	    no strict;

	    local $1$2;
	    \$$2=undef; do {
		$_
	    }; \$$2
	};
	local($^W) = 0;
	$result = eval($eval);
	die "Could not eval '$eval' in $parsefile: $@" if $@;
	$result = "undef" unless defined $result;
	last;
    }
    close FH;
    return $result;
}

=item parse_abstract

parse a file and return what you think is the ABSTRACT

=cut

sub parse_abstract {
    my($self,$parsefile) = @_;
    my $result;
    local *FH;
    local $/ = "\n";
    open(FH,$parsefile) or die "Could not open '$parsefile': $!";
    my $inpod = 0;
    my $package = $self->{DISTNAME};
    $package =~ s/-/::/;
    while (<FH>) {
        $inpod = /^=(?!cut)/ ? 1 : /^=cut/ ? 0 : $inpod;
        next if !$inpod;
        chop;
        next unless /^($package\s-\s)(.*)/;
        $result = $2;
        last;
    }
    close FH;
    return $result;
}

=item pasthru (o)

Defines the string that is passed to recursive make calls in
subdirectories.

=cut

sub pasthru {
    my($self) = shift;
    my(@m,$key);

    my(@pasthru);
    my($sep) = $Is_VMS ? ',' : '';
    $sep .= "\\\n\t";

    foreach $key (qw(LIB LIBPERL_A LINKTYPE PREFIX OPTIMIZE)){
	push @pasthru, "$key=\"\$($key)\"";
    }

    push @m, "\nPASTHRU = ", join ($sep, @pasthru), "\n";
    join "", @m;
}

=item path

Takes no argument, returns the environment variable PATH as an array.

=cut

sub path {
    my($self) = @_;
    my $path_sep = ($Is_OS2 || $Is_Dos) ? ";" : ":";
    my $path = $ENV{PATH};
    $path =~ s:\\:/:g if $Is_OS2;
    my @path = split $path_sep, $path;
    foreach(@path) { $_ = '.' if $_ eq '' }
    @path;
}

=item perl_script

Takes one argument, a file name, and returns the file name, if the
argument is likely to be a perl script. On MM_Unix this is true for
any ordinary, readable file.

=cut

sub perl_script {
    my($self,$file) = @_;
    return $file if -r $file && -f _;
    return;
}

=item perldepend (o)

Defines the dependency from all *.h files that come with the perl
distribution.

=cut

sub perldepend {
    my($self) = shift;
    my(@m);
    push @m, q{
# Check for unpropogated config.sh changes. Should never happen.
# We do NOT just update config.h because that is not sufficient.
# An out of date config.h is not fatal but complains loudly!
$(PERL_INC)/config.h: $(PERL_SRC)/config.sh
	-}.$self->{NOECHO}.q{echo "Warning: $(PERL_INC)/config.h out of date with $(PERL_SRC)/config.sh"; false

$(PERL_ARCHLIB)/Config.pm: $(PERL_SRC)/config.sh
	}.$self->{NOECHO}.q{echo "Warning: $(PERL_ARCHLIB)/Config.pm may be out of date with $(PERL_SRC)/config.sh"
	cd $(PERL_SRC) && $(MAKE) lib/Config.pm
} if $self->{PERL_SRC};

    return join "", @m unless $self->needs_linking;

    push @m, q{
PERL_HDRS = \
$(PERL_INC)/EXTERN.h       $(PERL_INC)/gv.h           $(PERL_INC)/pp.h       \
$(PERL_INC)/INTERN.h       $(PERL_INC)/handy.h        $(PERL_INC)/proto.h    \
$(PERL_INC)/XSUB.h         $(PERL_INC)/hv.h           $(PERL_INC)/regcomp.h  \
$(PERL_INC)/av.h           $(PERL_INC)/keywords.h     $(PERL_INC)/regexp.h   \
$(PERL_INC)/config.h       $(PERL_INC)/mg.h           $(PERL_INC)/scope.h    \
$(PERL_INC)/cop.h          $(PERL_INC)/op.h           $(PERL_INC)/sv.h	     \
$(PERL_INC)/cv.h           $(PERL_INC)/opcode.h       $(PERL_INC)/unixish.h  \
$(PERL_INC)/dosish.h       $(PERL_INC)/patchlevel.h   $(PERL_INC)/util.h     \
$(PERL_INC)/embed.h        $(PERL_INC)/perl.h         $(PERL_INC)/iperlsys.h \
$(PERL_INC)/form.h         $(PERL_INC)/perly.h

$(OBJECT) : $(PERL_HDRS)
} if $self->{OBJECT};

    push @m, join(" ", values %{$self->{XS}})." : \$(XSUBPPDEPS)\n"  if %{$self->{XS}};

    join "\n", @m;
}

=item ppd

Defines target that creates a PPD (Perl Package Description) file
for a binary distribution.

=cut

sub ppd {
    my($self) = @_;
    my(@m);
    if ($self->{ABSTRACT_FROM}){
        $self->{ABSTRACT} = $self->parse_abstract($self->{ABSTRACT_FROM}) or
            Carp::carp "WARNING: Setting ABSTRACT via file '$self->{ABSTRACT_FROM}' failed\n";
    }
    my ($pack_ver) = join ",", (split (/\./, $self->{VERSION}), (0) x 4) [0 .. 3];
    push(@m, "# Creates a PPD (Perl Package Description) for a binary distribution.\n");
    push(@m, "ppd:\n");
    push(@m, "\t\@\$(PERL) -e \"print qq{<SOFTPKG NAME=\\\"$self->{DISTNAME}\\\" VERSION=\\\"$pack_ver\\\">\\n}");
    push(@m, ". qq{\\t<TITLE>$self->{DISTNAME}</TITLE>\\n}");
    my $abstract = $self->{ABSTRACT};
    $abstract =~ s/\n/\\n/sg;
    $abstract =~ s/</&lt;/g;
    $abstract =~ s/>/&gt;/g;
    push(@m, ". qq{\\t<ABSTRACT>$abstract</ABSTRACT>\\n}");
    my ($author) = $self->{AUTHOR};
    $author =~ s/</&lt;/g;
    $author =~ s/>/&gt;/g;
    $author =~ s/@/\\@/g;
    push(@m, ". qq{\\t<AUTHOR>$author</AUTHOR>\\n}");
    push(@m, ". qq{\\t<IMPLEMENTATION>\\n}");
    my ($prereq);
    foreach $prereq (sort keys %{$self->{PREREQ_PM}}) {
        my $pre_req = $prereq;
        $pre_req =~ s/::/-/g;
        my ($dep_ver) = join ",", (split (/\./, $self->{PREREQ_PM}{$prereq}), (0) x 4) [0 .. 3];
        push(@m, ". qq{\\t\\t<DEPENDENCY NAME=\\\"$pre_req\\\" VERSION=\\\"$dep_ver\\\" />\\n}");
    }
    push(@m, ". qq{\\t\\t<OS NAME=\\\"\$(OSNAME)\\\" />\\n}");
    push(@m, ". qq{\\t\\t<ARCHITECTURE NAME=\\\"$Config{'archname'}\\\" />\\n}");
    my ($bin_location) = $self->{BINARY_LOCATION};
    $bin_location =~ s/\\/\\\\/g;
    if ($self->{PPM_INSTALL_SCRIPT}) {
        if ($self->{PPM_INSTALL_EXEC}) {
            push(@m, " . qq{\\t\\t<INSTALL EXEC=\\\"$self->{PPM_INSTALL_EXEC}\\\">$self->{PPM_INSTALL_SCRIPT}</INSTALL>\\n}");
        }
        else {
            push(@m, " . qq{\\t\\t<INSTALL>$self->{PPM_INSTALL_SCRIPT}</INSTALL>\\n}");
        }
    }
    push(@m, ". qq{\\t\\t<CODEBASE HREF=\\\"$bin_location\\\" />\\n}");
    push(@m, ". qq{\\t</IMPLEMENTATION>\\n}");
    push(@m, ". qq{</SOFTPKG>\\n}\" > $self->{DISTNAME}.ppd");

    join("", @m);   
}

=item perm_rw (o)

Returns the attribute C<PERM_RW> or the string C<644>.
Used as the string that is passed
to the C<chmod> command to set the permissions for read/writeable files.
MakeMaker chooses C<644> because it has turned out in the past that
relying on the umask provokes hard-to-track bug reports.
When the return value is used by the perl function C<chmod>, it is
interpreted as an octal value.

=cut

sub perm_rw {
    shift->{PERM_RW} || "644";
}

=item perm_rwx (o)

Returns the attribute C<PERM_RWX> or the string C<755>,
i.e. the string that is passed
to the C<chmod> command to set the permissions for executable files.
See also perl_rw.

=cut

sub perm_rwx {
    shift->{PERM_RWX} || "755";
}

=item pm_to_blib

Defines target that copies all files in the hash PM to their
destination and autosplits them. See L<ExtUtils::Install/DESCRIPTION>

=cut

sub pm_to_blib {
    my $self = shift;
    my($autodir) = $self->catdir('$(INST_LIB)','auto');
    return q{
pm_to_blib: $(TO_INST_PM)
	}.$self->{NOECHO}.q{$(PERL) "-I$(INST_ARCHLIB)" "-I$(INST_LIB)" \
	"-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" -MExtUtils::Install \
        -e "pm_to_blib({qw{$(PM_TO_BLIB)}},'}.$autodir.q{')"
	}.$self->{NOECHO}.q{$(TOUCH) $@
};
}

=item post_constants (o)

Returns an empty string per default. Dedicated to overrides from
within Makefile.PL after all constants have been defined.

=cut

sub post_constants{
    my($self) = shift;
    "";
}

=item post_initialize (o)

Returns an empty string per default. Used in Makefile.PLs to add some
chunk of text to the Makefile after the object is initialized.

=cut

sub post_initialize {
    my($self) = shift;
    "";
}

=item postamble (o)

Returns an empty string. Can be used in Makefile.PLs to write some
text to the Makefile at the end.

=cut

sub postamble {
    my($self) = shift;
    "";
}

=item prefixify

Check a path variable in $self from %Config, if it contains a prefix,
and replace it with another one.

Takes as arguments an attribute name, a search prefix and a
replacement prefix. Changes the attribute in the object.

=cut

sub prefixify {
    my($self,$var,$sprefix,$rprefix) = @_;
    $self->{uc $var} ||= $Config{lc $var};
    $self->{uc $var} = VMS::Filespec::unixpath($self->{uc $var}) if $Is_VMS;
    $self->{uc $var} =~ s/\Q$sprefix\E/$rprefix/;
}

=item processPL (o)

Defines targets to run *.PL files.

=cut

sub processPL {
    my($self) = shift;
    return "" unless $self->{PL_FILES};
    my(@m, $plfile);
    foreach $plfile (sort keys %{$self->{PL_FILES}}) {
        my $list = ref($self->{PL_FILES}->{$plfile})
		? $self->{PL_FILES}->{$plfile}
		: [$self->{PL_FILES}->{$plfile}];
	foreach $target (@$list) {
	push @m, "
all :: $target
	$self->{NOECHO}\$(NOOP)

$target :: $plfile
	\$(PERL) -I\$(INST_ARCHLIB) -I\$(INST_LIB) -I\$(PERL_ARCHLIB) -I\$(PERL_LIB) $plfile $target
";
	}
    }
    join "", @m;
}

=item realclean (o)

Defines the realclean target.

=cut

sub realclean {
    my($self, %attribs) = @_;
    my(@m);
    push(@m,'
# Delete temporary files (via clean) and also delete installed files
realclean purge ::  clean
');
    # realclean subdirectories first (already cleaned)
    my $sub = "\t-cd %s && \$(TEST_F) %s && \$(MAKE) %s realclean\n";
    foreach(@{$self->{DIR}}){
	push(@m, sprintf($sub,$_,"$self->{MAKEFILE}.old","-f $self->{MAKEFILE}.old"));
	push(@m, sprintf($sub,$_,"$self->{MAKEFILE}",''));
    }
    push(@m, "	$self->{RM_RF} \$(INST_AUTODIR) \$(INST_ARCHAUTODIR)\n");
    if( $self->has_link_code ){
        push(@m, "	$self->{RM_F} \$(INST_DYNAMIC) \$(INST_BOOT)\n");
        push(@m, "	$self->{RM_F} \$(INST_STATIC)\n");
    }
    push(@m, "	$self->{RM_F} " . join(" ", values %{$self->{PM}}) . "\n")
	if keys %{$self->{PM}};
    my(@otherfiles) = ($self->{MAKEFILE},
		       "$self->{MAKEFILE}.old"); # Makefiles last
    push(@otherfiles, $attribs{FILES}) if $attribs{FILES};
    push(@m, "	$self->{RM_RF} @otherfiles\n") if @otherfiles;
    push(@m, "	$attribs{POSTOP}\n")       if $attribs{POSTOP};
    join("", @m);
}

=item replace_manpage_separator

Takes the name of a package, which may be a nested package, in the
form Foo/Bar and replaces the slash with C<::>. Returns the replacement.

=cut

sub replace_manpage_separator {
    my($self,$man) = @_;
	if ($^O eq 'uwin') {
		$man =~ s,/+,.,g;
	} else {
		$man =~ s,/+,::,g;
	}
    $man;
}

=item static (o)

Defines the static target.

=cut

sub static {
# --- Static Loading Sections ---

    my($self) = shift;
    '
## $(INST_PM) has been moved to the all: target.
## It remains here for awhile to allow for old usage: "make static"
#static :: '.$self->{MAKEFILE}.' $(INST_STATIC) $(INST_PM)
static :: '.$self->{MAKEFILE}.' $(INST_STATIC)
	'.$self->{NOECHO}.'$(NOOP)
';
}

=item static_lib (o)

Defines how to produce the *.a (or equivalent) files.

=cut

sub static_lib {
    my($self) = @_;
# Come to think of it, if there are subdirs with linkcode, we still have no INST_STATIC
#    return '' unless $self->needs_linking(); #might be because of a subdir

    return '' unless $self->has_link_code;

    my(@m);
    push(@m, <<'END');
$(INST_STATIC): $(OBJECT) $(MYEXTLIB) $(INST_ARCHAUTODIR)/.exists
	$(RM_RF) $@
END
    # If this extension has it's own library (eg SDBM_File)
    # then copy that to $(INST_STATIC) and add $(OBJECT) into it.
    push(@m, "\t$self->{CP} \$(MYEXTLIB) \$\@\n") if $self->{MYEXTLIB};

    push @m,
q{	$(AR) $(AR_STATIC_ARGS) $@ $(OBJECT) && $(RANLIB) $@
	$(CHMOD) $(PERM_RWX) $@
	}.$self->{NOECHO}.q{echo "$(EXTRALIBS)" > $(INST_ARCHAUTODIR)/extralibs.ld
};
    # Old mechanism - still available:
    push @m,
"\t$self->{NOECHO}".q{echo "$(EXTRALIBS)" >> $(PERL_SRC)/ext.libs
}	if $self->{PERL_SRC} && $self->{EXTRALIBS};
    push @m, "\n";

    push @m, $self->dir_target('$(INST_ARCHAUTODIR)');
    join('', "\n",@m);
}

=item staticmake (o)

Calls makeaperl.

=cut

sub staticmake {
    my($self, %attribs) = @_;
    my(@static);

    my(@searchdirs)=($self->{PERL_ARCHLIB}, $self->{SITEARCHEXP},  $self->{INST_ARCHLIB});

    # And as it's not yet built, we add the current extension
    # but only if it has some C code (or XS code, which implies C code)
    if (@{$self->{C}}) {
	@static = $self->catfile($self->{INST_ARCHLIB},
				 "auto",
				 $self->{FULLEXT},
				 "$self->{BASEEXT}$self->{LIB_EXT}"
				);
    }

    # Either we determine now, which libraries we will produce in the
    # subdirectories or we do it at runtime of the make.

    # We could ask all subdir objects, but I cannot imagine, why it
    # would be necessary.

    # Instead we determine all libraries for the new perl at
    # runtime.
    my(@perlinc) = ($self->{INST_ARCHLIB}, $self->{INST_LIB}, $self->{PERL_ARCHLIB}, $self->{PERL_LIB});

    $self->makeaperl(MAKE	=> $self->{MAKEFILE},
		     DIRS	=> \@searchdirs,
		     STAT	=> \@static,
		     INCL	=> \@perlinc,
		     TARGET	=> $self->{MAP_TARGET},
		     TMP	=> "",
		     LIBPERL	=> $self->{LIBPERL_A}
		    );
}

=item subdir_x (o)

Helper subroutine for subdirs

=cut

sub subdir_x {
    my($self, $subdir) = @_;
    my(@m);
    qq{

subdirs ::
	$self->{NOECHO}cd $subdir && \$(MAKE) all \$(PASTHRU)

};
}

=item subdirs (o)

Defines targets to process subdirectories.

=cut

sub subdirs {
# --- Sub-directory Sections ---
    my($self) = shift;
    my(@m,$dir);
    # This method provides a mechanism to automatically deal with
    # subdirectories containing further Makefile.PL scripts.
    # It calls the subdir_x() method for each subdirectory.
    foreach $dir (@{$self->{DIR}}){
	push(@m, $self->subdir_x($dir));
####	print "Including $dir subdirectory\n";
    }
    if (@m){
	unshift(@m, "
# The default clean, realclean and test targets in this Makefile
# have automatically been given entries for each subdir.

");
    } else {
	push(@m, "\n# none")
    }
    join('',@m);
}

=item test (o)

Defines the test targets.

=cut

sub test {
# --- Test and Installation Sections ---

    my($self, %attribs) = @_;
    my $tests = $attribs{TESTS};
    if (!$tests && -d 't') {
	$tests = $Is_Win32 ? join(' ', <t\\*.t>) : 't/*.t';
    }
    # note: 'test.pl' name is also hardcoded in init_dirscan()
    my(@m);
    push(@m,"
TEST_VERBOSE=0
TEST_TYPE=test_\$(LINKTYPE)
TEST_FILE = test.pl
TEST_FILES = $tests
TESTDB_SW = -d

testdb :: testdb_\$(LINKTYPE)

test :: \$(TEST_TYPE)
");
    push(@m, map("\t$self->{NOECHO}cd $_ && \$(TEST_F) $self->{MAKEFILE} && \$(MAKE) test \$(PASTHRU)\n",
		 @{$self->{DIR}}));
    push(@m, "\t$self->{NOECHO}echo 'No tests defined for \$(NAME) extension.'\n")
	unless $tests or -f "test.pl" or @{$self->{DIR}};
    push(@m, "\n");

    push(@m, "test_dynamic :: pure_all\n");
    push(@m, $self->test_via_harness('$(FULLPERL)', '$(TEST_FILES)')) if $tests;
    push(@m, $self->test_via_script('$(FULLPERL)', '$(TEST_FILE)')) if -f "test.pl";
    push(@m, "\n");

    push(@m, "testdb_dynamic :: pure_all\n");
    push(@m, $self->test_via_script('$(FULLPERL) $(TESTDB_SW)', '$(TEST_FILE)'));
    push(@m, "\n");

    # Occasionally we may face this degenerate target:
    push @m, "test_ : test_dynamic\n\n";

    if ($self->needs_linking()) {
	push(@m, "test_static :: pure_all \$(MAP_TARGET)\n");
	push(@m, $self->test_via_harness('./$(MAP_TARGET)', '$(TEST_FILES)')) if $tests;
	push(@m, $self->test_via_script('./$(MAP_TARGET)', '$(TEST_FILE)')) if -f "test.pl";
	push(@m, "\n");
	push(@m, "testdb_static :: pure_all \$(MAP_TARGET)\n");
	push(@m, $self->test_via_script('./$(MAP_TARGET) $(TESTDB_SW)', '$(TEST_FILE)'));
	push(@m, "\n");
    } else {
	push @m, "test_static :: test_dynamic\n";
	push @m, "testdb_static :: testdb_dynamic\n";
    }
    join("", @m);
}

=item test_via_harness (o)

Helper method to write the test targets

=cut

sub test_via_harness {
    my($self, $perl, $tests) = @_;
    $perl = "PERL_DL_NONLAZY=1 $perl" unless $Is_Win32;
    "\t$perl".q! -I$(INST_ARCHLIB) -I$(INST_LIB) -I$(PERL_ARCHLIB) -I$(PERL_LIB) -e 'use Test::Harness qw(&runtests $$verbose); $$verbose=$(TEST_VERBOSE); runtests @ARGV;' !."$tests\n";
}

=item test_via_script (o)

Other helper method for test.

=cut

sub test_via_script {
    my($self, $perl, $script) = @_;
    $perl = "PERL_DL_NONLAZY=1 $perl" unless $Is_Win32;
    qq{\t$perl}.q{ -I$(INST_ARCHLIB) -I$(INST_LIB) -I$(PERL_ARCHLIB) -I$(PERL_LIB) }.qq{$script
};
}

=item tool_autosplit (o)

Defines a simple perl call that runs autosplit. May be deprecated by
pm_to_blib soon.

=cut

sub tool_autosplit {
# --- Tool Sections ---

    my($self, %attribs) = @_;
    my($asl) = "";
    $asl = "\$AutoSplit::Maxlen=$attribs{MAXLEN};" if $attribs{MAXLEN};
    q{
# Usage: $(AUTOSPLITFILE) FileToSplit AutoDirToSplitInto
AUTOSPLITFILE = $(PERL) "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" -e 'use AutoSplit;}.$asl.q{autosplit($$ARGV[0], $$ARGV[1], 0, 1, 1) ;'
};
}

=item tools_other (o)

Defines SHELL, LD, TOUCH, CP, MV, RM_F, RM_RF, CHMOD, UMASK_NULL in
the Makefile. Also defines the perl programs MKPATH,
WARN_IF_OLD_PACKLIST, MOD_INSTALL. DOC_INSTALL, and UNINSTALL.

=cut

sub tools_other {
    my($self) = shift;
    my @m;
    my $bin_sh = $Config{sh} || '/bin/sh';
    push @m, qq{
SHELL = $bin_sh
};

    for (qw/ CHMOD CP LD MV NOOP RM_F RM_RF TEST_F TOUCH UMASK_NULL DEV_NULL/ ) {
	push @m, "$_ = $self->{$_}\n";
    }

    push @m, q{
# The following is a portable way to say mkdir -p
# To see which directories are created, change the if 0 to if 1
MKPATH = $(PERL) -I$(PERL_ARCHLIB) -I$(PERL_LIB) -MExtUtils::Command -e mkpath

# This helps us to minimize the effect of the .exists files A yet
# better solution would be to have a stable file in the perl
# distribution with a timestamp of zero. But this solution doesn't
# need any changes to the core distribution and works with older perls
EQUALIZE_TIMESTAMP = $(PERL) -I$(PERL_ARCHLIB) -I$(PERL_LIB) -MExtUtils::Command -e eqtime
};


    return join "", @m if $self->{PARENT};

    push @m, q{
# Here we warn users that an old packlist file was found somewhere,
# and that they should call some uninstall routine
WARN_IF_OLD_PACKLIST = $(PERL) -we 'exit unless -f $$ARGV[0];' \\
-e 'print "WARNING: I have found an old package in\n";' \\
-e 'print "\t$$ARGV[0].\n";' \\
-e 'print "Please make sure the two installations are not conflicting\n";'

UNINST=0
VERBINST=1

MOD_INSTALL = $(PERL) -I$(INST_LIB) -I$(PERL_LIB) -MExtUtils::Install \
-e "install({@ARGV},'$(VERBINST)',0,'$(UNINST)');"

DOC_INSTALL = $(PERL) -e '$$\="\n\n";' \
-e 'print "=head2 ", scalar(localtime), ": C<", shift, ">", " L<", shift, ">";' \
-e 'print "=over 4";' \
-e 'while (defined($$key = shift) and defined($$val = shift)){print "=item *";print "C<$$key: $$val>";}' \
-e 'print "=back";'

UNINSTALL =   $(PERL) -MExtUtils::Install \
-e 'uninstall($$ARGV[0],1,1); print "\nUninstall is deprecated. Please check the";' \
-e 'print " packlist above carefully.\n  There may be errors. Remove the";' \
-e 'print " appropriate files manually.\n  Sorry for the inconveniences.\n"'
};

    return join "", @m;
}

=item tool_xsubpp (o)

Determines typemaps, xsubpp version, prototype behaviour.

=cut

sub tool_xsubpp {
    my($self) = shift;
    return "" unless $self->needs_linking;
    my($xsdir)  = $self->catdir($self->{PERL_LIB},"ExtUtils");
    my(@tmdeps) = $self->catdir('$(XSUBPPDIR)','typemap');
    if( $self->{TYPEMAPS} ){
	my $typemap;
	foreach $typemap (@{$self->{TYPEMAPS}}){
		if( ! -f  $typemap ){
			warn "Typemap $typemap not found.\n";
		}
		else{
			push(@tmdeps,  $typemap);
		}
	}
    }
    push(@tmdeps, "typemap") if -f "typemap";
    my(@tmargs) = map("-typemap $_", @tmdeps);
    if( exists $self->{XSOPT} ){
 	unshift( @tmargs, $self->{XSOPT} );
    }


    my $xsubpp_version = $self->xsubpp_version($self->catfile($xsdir,"xsubpp"));

    # What are the correct thresholds for version 1 && 2 Paul?
    if ( $xsubpp_version > 1.923 ){
	$self->{XSPROTOARG} = "" unless defined $self->{XSPROTOARG};
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

    my $xsubpp = $self->{CAPI} ? "xsubpp -object_capi" : "xsubpp";

    return qq{
XSUBPPDIR = $xsdir
XSUBPP = \$(XSUBPPDIR)/$xsubpp
XSPROTOARG = $self->{XSPROTOARG}
XSUBPPDEPS = @tmdeps
XSUBPPARGS = @tmargs
};
};

sub xsubpp_version
{
    my($self,$xsubpp) = @_;
    return $Xsubpp_Version if defined $Xsubpp_Version; # global variable

    my ($version) ;

    # try to figure out the version number of the xsubpp on the system

    # first try the -v flag, introduced in 1.921 & 2.000a2

    return "" unless $self->needs_linking;

    my $command = "$self->{PERL} -I$self->{PERL_LIB} $xsubpp -v 2>&1";
    print "Running $command\n" if $Verbose >= 2;
    $version = `$command` ;
    warn "Running '$command' exits with status " . ($?>>8) if $?;
    chop $version ;

    return $Xsubpp_Version = $1 if $version =~ /^xsubpp version (.*)/ ;

    # nope, then try something else

    my $counter = '000';
    my ($file) = 'temp' ;
    $counter++ while -e "$file$counter"; # don't overwrite anything
    $file .= $counter;

    open(F, ">$file") or die "Cannot open file '$file': $!\n" ;
    print F <<EOM ;
MODULE = fred PACKAGE = fred

int
fred(a)
        int     a;
EOM

    close F ;

    $command = "$self->{PERL} $xsubpp $file 2>&1";
    print "Running $command\n" if $Verbose >= 2;
    my $text = `$command` ;
    warn "Running '$command' exits with status " . ($?>>8) if $?;
    unlink $file ;

    # gets 1.2 -> 1.92 and 2.000a1
    return $Xsubpp_Version = $1 if $text =~ /automatically by xsubpp version ([\S]+)\s*/  ;

    # it is either 1.0 or 1.1
    return $Xsubpp_Version = 1.1 if $text =~ /^Warning: ignored semicolon/ ;

    # none of the above, so 1.0
    return $Xsubpp_Version = "1.0" ;
}

=item top_targets (o)

Defines the targets all, subdirs, config, and O_FILES

=cut

sub top_targets {
# --- Target Sections ---

    my($self) = shift;
    my(@m);
    push @m, '
#all ::	config $(INST_PM) subdirs linkext manifypods
';

    push @m, '
all :: pure_all manifypods
	'.$self->{NOECHO}.'$(NOOP)
' 
	  unless $self->{SKIPHASH}{'all'};
    
    push @m, '
pure_all :: config pm_to_blib subdirs linkext
	'.$self->{NOECHO}.'$(NOOP)

subdirs :: $(MYEXTLIB)
	'.$self->{NOECHO}.'$(NOOP)

config :: '.$self->{MAKEFILE}.' $(INST_LIBDIR)/.exists
	'.$self->{NOECHO}.'$(NOOP)

config :: $(INST_ARCHAUTODIR)/.exists
	'.$self->{NOECHO}.'$(NOOP)

config :: $(INST_AUTODIR)/.exists
	'.$self->{NOECHO}.'$(NOOP)
';

    push @m, qq{
config :: Version_check
	$self->{NOECHO}\$(NOOP)

} unless $self->{PARENT} or ($self->{PERL_SRC} && $self->{INSTALLDIRS} eq "perl") or $self->{NO_VC};

    push @m, $self->dir_target(qw[$(INST_AUTODIR) $(INST_LIBDIR) $(INST_ARCHAUTODIR)]);

    if (%{$self->{MAN1PODS}}) {
	push @m, qq[
config :: \$(INST_MAN1DIR)/.exists
	$self->{NOECHO}\$(NOOP)

];
	push @m, $self->dir_target(qw[$(INST_MAN1DIR)]);
    }
    if (%{$self->{MAN3PODS}}) {
	push @m, qq[
config :: \$(INST_MAN3DIR)/.exists
	$self->{NOECHO}\$(NOOP)

];
	push @m, $self->dir_target(qw[$(INST_MAN3DIR)]);
    }

    push @m, '
$(O_FILES): $(H_FILES)
' if @{$self->{O_FILES} || []} && @{$self->{H} || []};

    push @m, q{
help:
	perldoc ExtUtils::MakeMaker
};

    push @m, q{
Version_check:
	}.$self->{NOECHO}.q{$(PERL) -I$(PERL_ARCHLIB) -I$(PERL_LIB) \
		-MExtUtils::MakeMaker=Version_check \
		-e "Version_check('$(MM_VERSION)')"
};

    join('',@m);
}

=item writedoc

Obsolete, deprecated method. Not used since Version 5.21.

=cut

sub writedoc {
# --- perllocal.pod section ---
    my($self,$what,$name,@attribs)=@_;
    my $time = localtime;
    print "=head2 $time: $what C<$name>\n\n=over 4\n\n=item *\n\n";
    print join "\n\n=item *\n\n", map("C<$_>",@attribs);
    print "\n\n=back\n\n";
}

=item xs_c (o)

Defines the suffix rules to compile XS files to C.

=cut

sub xs_c {
    my($self) = shift;
    return '' unless $self->needs_linking();
    '
.xs.c:
	$(PERL) -I$(PERL_ARCHLIB) -I$(PERL_LIB) $(XSUBPP) $(XSPROTOARG) $(XSUBPPARGS) $*.xs >xstmp.c && $(MV) xstmp.c $*.c
';
}

=item xs_cpp (o)

Defines the suffix rules to compile XS files to C++.

=cut

sub xs_cpp {
    my($self) = shift;
    return '' unless $self->needs_linking();
    '
.xs.cpp:
	$(PERL) -I$(PERL_ARCHLIB) -I$(PERL_LIB) $(XSUBPP) $(XSPROTOARG) $(XSUBPPARGS) $*.xs >xstmp.c && $(MV) xstmp.c $*.cpp
';
}

=item xs_o (o)

Defines suffix rules to go from XS to object files directly. This is
only intended for broken make implementations.

=cut

sub xs_o {	# many makes are too dumb to use xs_c then c_o
    my($self) = shift;
    return '' unless $self->needs_linking();
    '
.xs$(OBJ_EXT):
	$(PERL) -I$(PERL_ARCHLIB) -I$(PERL_LIB) $(XSUBPP) $(XSPROTOARG) $(XSUBPPARGS) $*.xs >xstmp.c && $(MV) xstmp.c $*.c
	$(CCCMD) $(CCCDLFLAGS) -I$(PERL_INC) $(DEFINE) $*.c
';
}

=item perl_archive

This is internal method that returns path to libperl.a equivalent
to be linked to dynamic extensions. UNIX does not have one but OS2
and Win32 do.

=cut 

sub perl_archive
{
 return '$(PERL_INC)' . "/$Config{libperl}" if $^O eq "beos";
 return "";
}

=item export_list

This is internal method that returns name of a file that is
passed to linker to define symbols to be exported.
UNIX does not have one but OS2 and Win32 do.

=cut 

sub export_list
{
 return "";
}


1;

=back

=head1 SEE ALSO

L<ExtUtils::MakeMaker>

=cut

__END__

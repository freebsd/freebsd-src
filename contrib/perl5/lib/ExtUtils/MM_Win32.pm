package ExtUtils::MM_Win32;

=head1 NAME

ExtUtils::MM_Win32 - methods to override UN*X behaviour in ExtUtils::MakeMaker

=head1 SYNOPSIS

 use ExtUtils::MM_Win32; # Done internally by ExtUtils::MakeMaker if needed

=head1 DESCRIPTION

See ExtUtils::MM_Unix for a documentation of the methods provided
there. This package overrides the implementation of these methods, not
the semantics.

=over

=cut 

use Config;
#use Cwd;
use File::Basename;
require Exporter;

Exporter::import('ExtUtils::MakeMaker',
       qw( $Verbose &neatvalue));

$ENV{EMXSHELL} = 'sh'; # to run `commands`
unshift @MM::ISA, 'ExtUtils::MM_Win32';

$BORLAND = 1 if $Config{'cc'} =~ /^bcc/i;
$GCC     = 1 if $Config{'cc'} =~ /^gcc/i;
$DMAKE = 1 if $Config{'make'} =~ /^dmake/i;
$NMAKE = 1 if $Config{'make'} =~ /^nmake/i;
$PERLMAKE = 1 if $Config{'make'} =~ /^pmake/i;
$OBJ   = 1 if $Config{'ccflags'} =~ /PERL_OBJECT/i;

sub dlsyms {
    my($self,%attribs) = @_;

    my($funcs) = $attribs{DL_FUNCS} || $self->{DL_FUNCS} || {};
    my($vars)  = $attribs{DL_VARS} || $self->{DL_VARS} || [];
    my($funclist) = $attribs{FUNCLIST} || $self->{FUNCLIST} || [];
    my($imports)  = $attribs{IMPORTS} || $self->{IMPORTS} || {};
    my(@m);
    (my $boot = $self->{NAME}) =~ s/:/_/g;

    if (not $self->{SKIPHASH}{'dynamic'}) {
	push(@m,"
$self->{BASEEXT}.def: Makefile.PL
",
     q!	$(PERL) "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" -MExtUtils::Mksymlists \\
     -e "Mksymlists('NAME' => '!, $self->{NAME},
     q!', 'DLBASE' => '!,$self->{DLBASE},
     q!', 'DL_FUNCS' => !,neatvalue($funcs),
     q!, 'FUNCLIST' => !,neatvalue($funclist),
     q!, 'IMPORTS' => !,neatvalue($imports),
     q!, 'DL_VARS' => !, neatvalue($vars), q!);"
!);
    }
    join('',@m);
}

sub replace_manpage_separator {
    my($self,$man) = @_;
    $man =~ s,/+,.,g;
    $man;
}

sub maybe_command {
    my($self,$file) = @_;
    my @e = exists($ENV{'PATHEXT'})
          ? split(/;/, $ENV{PATHEXT})
	  : qw(.com .exe .bat .cmd);
    my $e = '';
    for (@e) { $e .= "\Q$_\E|" }
    chop $e;
    # see if file ends in one of the known extensions
    if ($file =~ /($e)$/i) {
	return $file if -e $file;
    }
    else {
	for (@e) {
	    return "$file$_" if -e "$file$_";
	}
    }
    return;
}

sub file_name_is_absolute {
    my($self,$file) = @_;
    $file =~ m{^([a-z]:)?[\\/]}i ;
}

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
	    $val = `$abs -e "require $ver;" 2>&1`;
	    if ($? == 0) {
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

sub catdir {
    my $self = shift;
    my @args = @_;
    for (@args) {
	# append a slash to each argument unless it has one there
	$_ .= "\\" if $_ eq '' or substr($_,-1) ne "\\";
    }
    my $result = $self->canonpath(join('', @args));
    $result;
}

=item catfile

Concatenate one or more directory names and a filename to form a
complete path ending with a filename

=cut

sub catfile {
    my $self = shift @_;
    my $file = pop @_;
    return $file unless @_;
    my $dir = $self->catdir(@_);
    $dir =~ s/(\\\.)$//;
    $dir .= "\\" unless substr($dir,length($dir)-1,1) eq "\\";
    return $dir.$file;
}

sub init_others
{
 my ($self) = @_;
 &ExtUtils::MM_Unix::init_others;
 $self->{'TOUCH'}  = '$(PERL) -I$(PERL_ARCHLIB) -I$(PERL_LIB) -MExtUtils::Command -e touch';
 $self->{'CHMOD'}  = '$(PERL) -I$(PERL_ARCHLIB) -I$(PERL_LIB) -MExtUtils::Command -e chmod'; 
 $self->{'CP'}     = '$(PERL) -I$(PERL_ARCHLIB) -I$(PERL_LIB) -MExtUtils::Command -e cp';
 $self->{'RM_F'}   = '$(PERL) -I$(PERL_ARCHLIB) -I$(PERL_LIB) -MExtUtils::Command -e rm_f';
 $self->{'RM_RF'}  = '$(PERL) -I$(PERL_ARCHLIB) -I$(PERL_LIB) -MExtUtils::Command -e rm_rf';
 $self->{'MV'}     = '$(PERL) -I$(PERL_ARCHLIB) -I$(PERL_LIB) -MExtUtils::Command -e mv';
 $self->{'NOOP'}   = 'rem';
 $self->{'TEST_F'} = '$(PERL) -I$(PERL_ARCHLIB) -I$(PERL_LIB) -MExtUtils::Command -e test_f';
 $self->{'LD'}     = $Config{'ld'} || 'link';
 $self->{'AR'}     = $Config{'ar'} || 'lib';
 $self->{'LDLOADLIBS'} ||= $Config{'libs'};
 # -Lfoo must come first for Borland, so we put it in LDDLFLAGS
 if ($BORLAND) {
     my $libs = $self->{'LDLOADLIBS'};
     my $libpath = '';
     while ($libs =~ s/(?:^|\s)(("?)-L.+?\2)(?:\s|$)/ /) {
         $libpath .= ' ' if length $libpath;
         $libpath .= $1;
     }
     $self->{'LDLOADLIBS'} = $libs;
     $self->{'LDDLFLAGS'} ||= $Config{'lddlflags'};
     $self->{'LDDLFLAGS'} .= " $libpath";
 }
 $self->{'DEV_NULL'} = '> NUL';
 # $self->{'NOECHO'} = ''; # till we have it working
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
MAKEMAKER = $INC{'ExtUtils\MakeMaker.pm'}
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

    push @m, qq{
.USESHELL :
} if $DMAKE;

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
CONFIGDEP = \$(PERL_ARCHLIB)\\Config.pm \$(PERL_INC)\\config.h
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
INST_STATIC  = $(INST_ARCHAUTODIR)\$(BASEEXT)$(LIB_EXT)
INST_DYNAMIC = $(INST_ARCHAUTODIR)\$(DLBASE).$(DLEXT)
INST_BOOT    = $(INST_ARCHAUTODIR)\$(BASEEXT).bs
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


sub path {
    local $^W = 1;
    my($self) = @_;
    my $path = $ENV{'PATH'} || $ENV{'Path'} || $ENV{'path'};
    my @path = split(';',$path);
    foreach(@path) { $_ = '.' if $_ eq '' }
    @path;
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
$(INST_STATIC): $(OBJECT) $(MYEXTLIB) $(INST_ARCHAUTODIR)\.exists
	$(RM_RF) $@
END
    # If this extension has it's own library (eg SDBM_File)
    # then copy that to $(INST_STATIC) and add $(OBJECT) into it.
    push(@m, "\t$self->{CP} \$(MYEXTLIB) \$\@\n") if $self->{MYEXTLIB};

    push @m,
q{	$(AR) }.($BORLAND ? '$@ $(OBJECT:^"+")'
			  : ($GCC ? '-ru $@ $(OBJECT)'
			          : '-out:$@ $(OBJECT)')).q{
	}.$self->{NOECHO}.q{echo "$(EXTRALIBS)" > $(INST_ARCHAUTODIR)\extralibs.ld
	$(CHMOD) 755 $@
};

# Old mechanism - still available:

    push @m, "\t$self->{NOECHO}".q{echo "$(EXTRALIBS)" >> $(PERL_SRC)\ext.libs}."\n\n"
	if $self->{PERL_SRC};

    push @m, $self->dir_target('$(INST_ARCHAUTODIR)');
    join('', "\n",@m);
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
$(BOOTSTRAP): '."$self->{MAKEFILE} $self->{BOOTDEP}".' $(INST_ARCHAUTODIR)\.exists
	'.$self->{NOECHO}.'echo "Running Mkbootstrap for $(NAME) ($(BSLOADLIBS))"
	'.$self->{NOECHO}.'$(PERL) "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" \
		-MExtUtils::Mkbootstrap \
		-e "Mkbootstrap(\'$(BASEEXT)\',\'$(BSLOADLIBS)\');"
	'.$self->{NOECHO}.'$(TOUCH) $(BOOTSTRAP)
	$(CHMOD) 644 $@

$(INST_BOOT): $(BOOTSTRAP) $(INST_ARCHAUTODIR)\.exists
	'."$self->{NOECHO}$self->{RM_RF}".' $(INST_BOOT)
	-'.$self->{CP}.' $(BOOTSTRAP) $(INST_BOOT)
	$(CHMOD) 644 $@
';
}

=item dynamic_lib (o)

Defines how to produce the *.so (or equivalent) files.

=cut

sub dynamic_lib {
    my($self, %attribs) = @_;
    return '' unless $self->needs_linking(); #might be because of a subdir

    return '' unless $self->has_link_code;

    my($otherldflags) = $attribs{OTHERLDFLAGS} || ($BORLAND ? 'c0d32.obj': '');
    my($inst_dynamic_dep) = $attribs{INST_DYNAMIC_DEP} || "";
    my($ldfrom) = '$(LDFROM)';
    my(@m);
    push(@m,'
# This section creates the dynamically loadable $(INST_DYNAMIC)
# from $(OBJECT) and possibly $(MYEXTLIB).
OTHERLDFLAGS = '.$otherldflags.'
INST_DYNAMIC_DEP = '.$inst_dynamic_dep.'

$(INST_DYNAMIC): $(OBJECT) $(MYEXTLIB) $(BOOTSTRAP) $(INST_ARCHAUTODIR)\.exists $(EXPORT_LIST) $(PERL_ARCHIVE) $(INST_DYNAMIC_DEP)
');
    if ($GCC) {
      push(@m,  
       q{	dlltool --def $(EXPORT_LIST) --output-exp dll.exp
	$(LD) -o $@ -Wl,--base-file -Wl,dll.base $(LDDLFLAGS) }.$ldfrom.q{ $(OTHERLDFLAGS) $(MYEXTLIB) $(PERL_ARCHIVE) $(LDLOADLIBS) dll.exp
	dlltool --def $(EXPORT_LIST) --base-file dll.base --output-exp dll.exp
	$(LD) -o $@ $(LDDLFLAGS) }.$ldfrom.q{ $(OTHERLDFLAGS) $(MYEXTLIB) $(PERL_ARCHIVE) $(LDLOADLIBS) dll.exp });
    } elsif ($BORLAND) {
      push(@m,
       q{	$(LD) $(LDDLFLAGS) $(OTHERLDFLAGS) }.$ldfrom.q{,$@,,}
       .($DMAKE ? q{$(PERL_ARCHIVE:s,/,\,) $(LDLOADLIBS:s,/,\,) }
		 .q{$(MYEXTLIB:s,/,\,),$(EXPORT_LIST:s,/,\,)}
		: q{$(subst /,\,$(PERL_ARCHIVE)) $(subst /,\,$(LDLOADLIBS)) }
		 .q{$(subst /,\,$(MYEXTLIB)),$(subst /,\,$(EXPORT_LIST))})
       .q{,$(RESFILES)});
    } else {	# VC
      push(@m,
       q{	$(LD) -out:$@ $(LDDLFLAGS) }.$ldfrom.q{ $(OTHERLDFLAGS) }
      .q{$(MYEXTLIB) $(PERL_ARCHIVE) $(LDLOADLIBS) -def:$(EXPORT_LIST)});
    }
    push @m, '
	$(CHMOD) 755 $@
';

    push @m, $self->dir_target('$(INST_ARCHAUTODIR)');
    join('',@m);
}

sub perl_archive
{
    my ($self) = @_;
    if($OBJ) {
        if ($self->{CAPI}) {
            return '$(PERL_INC)\perlCAPI$(LIB_EXT)';
        }
    }
    return '$(PERL_INC)\\'.$Config{'libperl'};
}

sub export_list
{
 my ($self) = @_;
 return "$self->{BASEEXT}.def";
}

=item canonpath

No physical check on the filesystem, but a logical cleanup of a
path. On UNIX eliminated successive slashes and successive "/.".

=cut

sub canonpath {
    my($self,$path) = @_;
    $path =~ s/^([a-z]:)/\u$1/;
    $path =~ s|/|\\|g;
    $path =~ s|(.)\\+|$1\\|g ;                     # xx////xx  -> xx/xx
    $path =~ s|(\\\.)+\\|\\|g ;                    # xx/././xx -> xx/xx
    $path =~ s|^(\.\\)+|| unless $path eq ".\\";   # ./xx      -> xx
    $path =~ s|\\$|| 
             unless $path =~ m#^([a-z]:)?\\#;      # xx/       -> xx
    $path .= '.' if $path =~ m#\\$#;
    $path;
}

=item perl_script

Takes one argument, a file name, and returns the file name, if the
argument is likely to be a perl script. On MM_Unix this is true for
any ordinary, readable file.

=cut

sub perl_script {
    my($self,$file) = @_;
    return "$file.pl" if -r "$file.pl" && -f _;
    return;
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
        -e "pm_to_blib(}.
	($NMAKE ? 'qw[ <<pmfiles.dat ],'
	        : $DMAKE ? 'qw[ $(mktmp,pmfiles.dat $(PM_TO_BLIB:s,\\,\\\\,)\n) ],'
			 : '{ qw[$(PM_TO_BLIB)] },'
	 ).q{'}.$autodir.q{')"
	}. ($NMAKE ? q{
$(PM_TO_BLIB)
<<
	} : '') . $self->{NOECHO}.q{$(TOUCH) $@
};
}

=item test_via_harness (o)

Helper method to write the test targets

=cut

sub test_via_harness {
    my($self, $perl, $tests) = @_;
    "\t$perl".q! -Mblib -I$(PERL_ARCHLIB) -I$(PERL_LIB) -e "use Test::Harness qw(&runtests $$verbose); $$verbose=$(TEST_VERBOSE); runtests @ARGV;" !."$tests\n";
}


=item tool_autosplit (override)

Use Win32 quoting on command line.

=cut

sub tool_autosplit{
    my($self, %attribs) = @_;
    my($asl) = "";
    $asl = "\$AutoSplit::Maxlen=$attribs{MAXLEN};" if $attribs{MAXLEN};
    q{
# Usage: $(AUTOSPLITFILE) FileToSplit AutoDirToSplitInto
AUTOSPLITFILE = $(PERL) "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" -MAutoSplit }.$asl.q{ -e "autosplit($$ARGV[0], $$ARGV[1], 0, 1, 1);"
};
}

=item tools_other (o)

Win32 overrides.

Defines SHELL, LD, TOUCH, CP, MV, RM_F, RM_RF, CHMOD, UMASK_NULL in
the Makefile. Also defines the perl programs MKPATH,
WARN_IF_OLD_PACKLIST, MOD_INSTALL. DOC_INSTALL, and UNINSTALL.

=cut

sub tools_other {
    my($self) = shift;
    my @m;
    my $bin_sh = $Config{sh} || 'cmd /c';
    push @m, qq{
SHELL = $bin_sh
} unless $DMAKE;  # dmake determines its own shell 

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
WARN_IF_OLD_PACKLIST = $(PERL) -lwe "exit unless -f $$ARGV[0];" \\
-e "print 'WARNING: I have found an old package in';" \\
-e "print '	', $$ARGV[0], '.';" \\
-e "print 'Please make sure the two installations are not conflicting';"

UNINST=0
VERBINST=1

MOD_INSTALL = $(PERL) -I$(INST_LIB) -I$(PERL_LIB) -MExtUtils::Install \
-e "install({ @ARGV },'$(VERBINST)',0,'$(UNINST)');"

DOC_INSTALL = $(PERL) -e "$$\=\"\n\n\";" \
-e "print '=head2 ', scalar(localtime), ': C<', shift, '>', ' L<', shift, '>';" \
-e "print '=over 4';" \
-e "while (defined($$key = shift) and defined($$val = shift)) { print '=item *';print 'C<', \"$$key: $$val\", '>'; }" \
-e "print '=back';"

UNINSTALL =   $(PERL) -MExtUtils::Install \
-e "uninstall($$ARGV[0],1,1); print \"\nUninstall is deprecated. Please check the";" \
-e "print \" packlist above carefully.\n  There may be errors. Remove the\";" \
-e "print \" appropriate files manually.\n  Sorry for the inconveniences.\n\""
};

    return join "", @m;
}

=item xs_o (o)

Defines suffix rules to go from XS to object files directly. This is
only intended for broken make implementations.

=cut

sub xs_o {	# many makes are too dumb to use xs_c then c_o
    my($self) = shift;
    return ''
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

config :: '.$self->{MAKEFILE}.' $(INST_LIBDIR)\.exists
	'.$self->{NOECHO}.'$(NOOP)

config :: $(INST_ARCHAUTODIR)\.exists
	'.$self->{NOECHO}.'$(NOOP)

config :: $(INST_AUTODIR)\.exists
	'.$self->{NOECHO}.'$(NOOP)
';

    push @m, qq{
config :: Version_check
	$self->{NOECHO}\$(NOOP)

} unless $self->{PARENT} or ($self->{PERL_SRC} && $self->{INSTALLDIRS} eq "perl") or $self->{NO_VC};

    push @m, $self->dir_target(qw[$(INST_AUTODIR) $(INST_LIBDIR) $(INST_ARCHAUTODIR)]);

    if (%{$self->{MAN1PODS}}) {
	push @m, qq[
config :: \$(INST_MAN1DIR)\\.exists
	$self->{NOECHO}\$(NOOP)

];
	push @m, $self->dir_target(qw[$(INST_MAN1DIR)]);
    }
    if (%{$self->{MAN3PODS}}) {
	push @m, qq[
config :: \$(INST_MAN3DIR)\\.exists
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

=item manifypods (o)

We don't want manpage process.  XXX add pod2html support later.

=cut

sub manifypods {
    my($self) = shift;
    return "\nmanifypods :\n\t$self->{NOECHO}\$(NOOP)\n";
}

=item dist_ci (o)

Same as MM_Unix version (changes command-line quoting).

=cut

sub dist_ci {
    my($self) = shift;
    my @m;
    push @m, q{
ci :
	$(PERL) -I$(PERL_ARCHLIB) -I$(PERL_LIB) -MExtUtils::Manifest=maniread \\
		-e "@all = keys %{ maniread() };" \\
		-e "print(\"Executing $(CI) @all\n\"); system(\"$(CI) @all\");" \\
		-e "print(\"Executing $(RCS_LABEL) ...\n\"); system(\"$(RCS_LABEL) @all\");"
};
    join "", @m;
}

=item dist_core (o)

Same as MM_Unix version (changes command-line quoting).

=cut

sub dist_core {
    my($self) = shift;
    my @m;
    push @m, q{
dist : $(DIST_DEFAULT)
	}.$self->{NOECHO}.q{$(PERL) -le "print \"Warning: Makefile possibly out of date with $$vf\" if " \
	    -e "-e ($$vf=\"$(VERSION_FROM)\") and -M $$vf < -M \"}.$self->{MAKEFILE}.q{\";"

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

=item pasthru (o)

Defines the string that is passed to recursive make calls in
subdirectories.

=cut

sub pasthru {
    my($self) = shift;
    return "PASTHRU = " . ($NMAKE ? "-nologo" : "");
}



1;
__END__

=back

=cut 



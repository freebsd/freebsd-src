BEGIN {require 5.002;} # MakeMaker 5.17 was the last MakeMaker that was compatible with perl5.001m

package ExtUtils::MakeMaker;

$VERSION = "5.45";
$Version_OK = "5.17";	# Makefiles older than $Version_OK will die
			# (Will be checked from MakeMaker version 4.13 onwards)
($Revision = substr(q$Revision: 1.222 $, 10)) =~ s/\s+$//;



require Exporter;
use Config;
use Carp ();
#use FileHandle ();

use vars qw(

	    @ISA @EXPORT @EXPORT_OK $AUTOLOAD
	    $ISA_TTY $Is_Mac $Is_OS2 $Is_VMS $Revision
	    $VERSION $Verbose $Version_OK %Config %Keep_after_flush
	    %MM_Sections %Prepend_dot_dot %Recognized_Att_Keys
	    @Get_from_Config @MM_Sections @Overridable @Parent

	   );
# use strict;

# &DynaLoader::mod2fname should be available to miniperl, thus 
# should be a pseudo-builtin (cmp. os2.c).
#eval {require DynaLoader;};

#
# Set up the inheritance before we pull in the MM_* packages, because they
# import variables and functions from here
#
@ISA = qw(Exporter);
@EXPORT = qw(&WriteMakefile &writeMakefile $Verbose &prompt);
@EXPORT_OK = qw($VERSION &Version_check &neatvalue &mkbootstrap &mksymlists);

#
# Dummy package MM inherits actual methods from OS-specific
# default packages.  We use this intermediate package so
# MY::XYZ->func() can call MM->func() and get the proper
# default routine without having to know under what OS
# it's running.
#
@MM::ISA = qw[ExtUtils::MM_Unix ExtUtils::Liblist::Kid ExtUtils::MakeMaker];

#
# Setup dummy package:
# MY exists for overriding methods to be defined within
#
{
    package MY;
    @MY::ISA = qw(MM);
###    sub AUTOLOAD { use Devel::Symdump; print Devel::Symdump->rnew->as_string; Carp::confess "hey why? $AUTOLOAD" }
    package MM;
    sub DESTROY {}
}

# "predeclare the package: we only load it via AUTOLOAD
# but we have already mentioned it in @ISA
package ExtUtils::Liblist::Kid;

package ExtUtils::MakeMaker;
#
# Now we can pull in the friends
#
$Is_VMS   = $^O eq 'VMS';
$Is_OS2   = $^O eq 'os2';
$Is_Mac   = $^O eq 'MacOS';
$Is_Win32 = $^O eq 'MSWin32';
$Is_Cygwin= $^O eq 'cygwin';

require ExtUtils::MM_Unix;

if ($Is_VMS) {
    require ExtUtils::MM_VMS;
    require VMS::Filespec; # is a noop as long as we require it within MM_VMS
}
if ($Is_OS2) {
    require ExtUtils::MM_OS2;
}
if ($Is_Mac) {
    require ExtUtils::MM_MacOS;
}
if ($Is_Win32) {
    require ExtUtils::MM_Win32;
}
if ($Is_Cygwin) {
    require ExtUtils::MM_Cygwin;
}

full_setup();

# The use of the Version_check target has been dropped between perl
# 5.5.63 and 5.5.64. We must keep the subroutine for a while so that
# old Makefiles can satisfy the Version_check target.

sub Version_check {
    my($checkversion) = @_;
    die "Your Makefile was built with ExtUtils::MakeMaker v $checkversion.
Current Version is $ExtUtils::MakeMaker::VERSION. There have been considerable
changes in the meantime.
Please rerun 'perl Makefile.PL' to regenerate the Makefile.\n"
    if $checkversion < $Version_OK;
    printf STDOUT "%s %s %s %s.\n", "Makefile built with ExtUtils::MakeMaker v",
    $checkversion, "Current Version is", $VERSION
	unless $checkversion == $VERSION;
}

sub warnhandler {
    $_[0] =~ /^Use of uninitialized value/ && return;
    $_[0] =~ /used only once/ && return;
    $_[0] =~ /^Subroutine\s+[\w:]+\s+redefined/ && return;
    warn @_;
}

sub WriteMakefile {
    Carp::croak "WriteMakefile: Need even number of args" if @_ % 2;
    local $SIG{__WARN__} = \&warnhandler;

    my %att = @_;
    MM->new(\%att)->flush;
}

sub prompt ($;$) {
    my($mess,$def)=@_;
    $ISA_TTY = -t STDIN && (-t STDOUT || !(-f STDOUT || -c STDOUT)) ;	# Pipe?
    Carp::confess("prompt function called without an argument") unless defined $mess;
    my $dispdef = defined $def ? "[$def] " : " ";
    $def = defined $def ? $def : "";
    my $ans;
    local $|=1;
    print "$mess $dispdef";
    if ($ISA_TTY) {
	chomp($ans = <STDIN>);
    } else {
	print "$def\n";
    }
    return ($ans ne '') ? $ans : $def;
}

sub eval_in_subdirs {
    my($self) = @_;
    my($dir);
    use Cwd 'cwd';
    my $pwd = cwd();

    foreach $dir (@{$self->{DIR}}){
	my($abs) = $self->catdir($pwd,$dir);
	$self->eval_in_x($abs);
    }
    chdir $pwd;
}

sub eval_in_x {
    my($self,$dir) = @_;
    package main;
    chdir $dir or Carp::carp("Couldn't change to directory $dir: $!");
#    use FileHandle ();
#    my $fh = new FileHandle;
#    $fh->open("Makefile.PL") or Carp::carp("Couldn't open Makefile.PL in $dir");
    local *FH;
    open(FH,"Makefile.PL") or Carp::carp("Couldn't open Makefile.PL in $dir");
#    my $eval = join "", <$fh>;
    my $eval = join "", <FH>;
#    $fh->close;
    close FH;
    eval $eval;
    if ($@) {
# 	  if ($@ =~ /prerequisites/) {
# 	      die "MakeMaker WARNING: $@";
# 	  } else {
# 	      warn "WARNING from evaluation of $dir/Makefile.PL: $@";
# 	  }
	warn "WARNING from evaluation of $dir/Makefile.PL: $@";
    }
}

sub full_setup {
    $Verbose ||= 0;

    # package name for the classes into which the first object will be blessed
    $PACKNAME = "PACK000";

    @Attrib_help = qw/

    AUTHOR ABSTRACT ABSTRACT_FROM BINARY_LOCATION
    C CAPI CCFLAGS CONFIG CONFIGURE DEFINE DIR DISTNAME DL_FUNCS DL_VARS
    EXCLUDE_EXT EXE_FILES FIRST_MAKEFILE FULLPERL FUNCLIST H 
    HTMLLIBPODS HTMLSCRIPTPODS IMPORTS
    INC INCLUDE_EXT INSTALLARCHLIB INSTALLBIN INSTALLDIRS INSTALLHTMLPRIVLIBDIR
    INSTALLHTMLSCRIPTDIR INSTALLHTMLSITELIBDIR INSTALLMAN1DIR
    INSTALLMAN3DIR INSTALLPRIVLIB INSTALLSCRIPT INSTALLSITEARCH
    INSTALLSITELIB INST_ARCHLIB INST_BIN INST_EXE INST_LIB
    INST_HTMLLIBDIR INST_HTMLSCRIPTDIR
    INST_MAN1DIR INST_MAN3DIR INST_SCRIPT LDFROM LIB LIBPERL_A LIBS
    LINKTYPE MAKEAPERL MAKEFILE MAN1PODS MAN3PODS MAP_TARGET MYEXTLIB
    PERL_MALLOC_OK
    NAME NEEDS_LINKING NOECHO NORECURS NO_VC OBJECT OPTIMIZE PERL PERLMAINCC
    PERL_ARCHLIB PERL_LIB PERL_SRC PERM_RW PERM_RWX
    PL_FILES PM PM_FILTER PMLIBDIRS POLLUTE PPM_INSTALL_EXEC
	PPM_INSTALL_SCRIPT PREFIX
    PREREQ_PM SKIP TYPEMAPS VERSION VERSION_FROM XS XSOPT XSPROTOARG
    XS_VERSION clean depend dist dynamic_lib linkext macro realclean
    tool_autosplit

    MACPERL_SRC MACPERL_LIB MACLIBS_68K MACLIBS_PPC MACLIBS_SC MACLIBS_MRC
    MACLIBS_ALL_68K MACLIBS_ALL_PPC MACLIBS_SHARED
	/;

    # IMPORTS is used under OS/2 and Win32

    # @Overridable is close to @MM_Sections but not identical.  The
    # order is important. Many subroutines declare macros. These
    # depend on each other. Let's try to collect the macros up front,
    # then pasthru, then the rules.

    # MM_Sections are the sections we have to call explicitly
    # in Overridable we have subroutines that are used indirectly


    @MM_Sections = 
	qw(

 post_initialize const_config constants tool_autosplit tool_xsubpp
 tools_other dist macro depend cflags const_loadlibs const_cccmd
 post_constants

 pasthru

 c_o xs_c xs_o top_targets linkext dlsyms dynamic dynamic_bs
 dynamic_lib static static_lib htmlifypods manifypods processPL
 installbin subdirs
 clean realclean dist_basics dist_core dist_dir dist_test dist_ci
 install force perldepend makefile staticmake test ppd

	  ); # loses section ordering

    @Overridable = @MM_Sections;
    push @Overridable, qw[

 dir_target libscan makeaperl needs_linking perm_rw perm_rwx
 subdir_x test_via_harness test_via_script
			 ];

    push @MM_Sections, qw[

 pm_to_blib selfdocument

			 ];

    # Postamble needs to be the last that was always the case
    push @MM_Sections, "postamble";
    push @Overridable, "postamble";

    # All sections are valid keys.
    @Recognized_Att_Keys{@MM_Sections} = (1) x @MM_Sections;

    # we will use all these variables in the Makefile
    @Get_from_Config = 
	qw(
	   ar cc cccdlflags ccdlflags dlext dlsrc ld lddlflags ldflags libc
	   lib_ext obj_ext osname osvers ranlib sitelibexp sitearchexp so
	   exe_ext full_ar
	  );

    my $item;
    foreach $item (@Attrib_help){
	$Recognized_Att_Keys{$item} = 1;
    }
    foreach $item (@Get_from_Config) {
	$Recognized_Att_Keys{uc $item} = $Config{$item};
	print "Attribute '\U$item\E' => '$Config{$item}'\n"
	    if ($Verbose >= 2);
    }

    #
    # When we eval a Makefile.PL in a subdirectory, that one will ask
    # us (the parent) for the values and will prepend "..", so that
    # all files to be installed end up below OUR ./blib
    #
    %Prepend_dot_dot = 
	qw(

	   INST_BIN 1 INST_EXE 1 INST_LIB 1 INST_ARCHLIB 1 INST_SCRIPT 1
	   MAP_TARGET 1 INST_HTMLLIBDIR 1 INST_HTMLSCRIPTDIR 1 
	   INST_MAN1DIR 1 INST_MAN3DIR 1 PERL_SRC 1 PERL 1 FULLPERL 1

	  );

    my @keep = qw/
	NEEDS_LINKING HAS_LINK_CODE
	/;
    @Keep_after_flush{@keep} = (1) x @keep;
}

sub writeMakefile {
    die <<END;

The extension you are trying to build apparently is rather old and
most probably outdated. We detect that from the fact, that a
subroutine "writeMakefile" is called, and this subroutine is not
supported anymore since about October 1994.

Please contact the author or look into CPAN (details about CPAN can be
found in the FAQ and at http:/www.perl.com) for a more recent version
of the extension. If you're really desperate, you can try to change
the subroutine name from writeMakefile to WriteMakefile and rerun
'perl Makefile.PL', but you're most probably left alone, when you do
so.

The MakeMaker team

END
}

sub ExtUtils::MakeMaker::new {
    my($class,$self) = @_;
    my($key);

    print STDOUT "MakeMaker (v$VERSION)\n" if $Verbose;
    if (-f "MANIFEST" && ! -f "Makefile"){
	check_manifest();
    }

    $self = {} unless (defined $self);

    check_hints($self);

    my(%initial_att) = %$self; # record initial attributes

    my($prereq);
    foreach $prereq (sort keys %{$self->{PREREQ_PM}}) {
	my $eval = "require $prereq";
	eval $eval;

	if ($@) {
	    warn "Warning: prerequisite $prereq failed to load: $@";
	}
	elsif ($prereq->VERSION < $self->{PREREQ_PM}->{$prereq} ){
	    warn "Warning: prerequisite $prereq $self->{PREREQ_PM}->{$prereq} not found";
# Why is/was this 'delete' here?  We need PREREQ_PM later to make PPDs.
#	} else {
#	    delete $self->{PREREQ_PM}{$prereq};
	}
    }
#    if (@unsatisfied){
# 	  unless (defined $ExtUtils::MakeMaker::useCPAN) {
# 	      print qq{MakeMaker WARNING: prerequisites not found (@unsatisfied)
# Please install these modules first and rerun 'perl Makefile.PL'.\n};
# 	      if ($ExtUtils::MakeMaker::hasCPAN) {
# 		  $ExtUtils::MakeMaker::useCPAN = prompt(qq{Should I try to use the CPAN module to fetch them for you?},"yes");
# 	      } else {
# 		  print qq{Hint: You may want to install the CPAN module to autofetch the needed modules\n};
# 		  $ExtUtils::MakeMaker::useCPAN=0;
# 	      }
# 	  }
# 	  if ($ExtUtils::MakeMaker::useCPAN) {
# 	      require CPAN;
# 	      CPAN->import(@unsatisfied);
# 	  } else {
# 	      die qq{prerequisites not found (@unsatisfied)};
# 	  }
#	warn qq{WARNING: prerequisites not found (@unsatisfied)};
#    }

    if (defined $self->{CONFIGURE}) {
	if (ref $self->{CONFIGURE} eq 'CODE') {
	    $self = { %$self, %{&{$self->{CONFIGURE}}}};
	} else {
	    Carp::croak "Attribute 'CONFIGURE' to WriteMakefile() not a code reference\n";
	}
    }

    # This is for old Makefiles written pre 5.00, will go away
    if ( Carp::longmess("") =~ /runsubdirpl/s ){
	Carp::carp("WARNING: Please rerun 'perl Makefile.PL' to regenerate your Makefiles\n");
    }

    my $newclass = ++$PACKNAME;
    local @Parent = @Parent;	# Protect against non-local exits
    {
#	no strict;
	print "Blessing Object into class [$newclass]\n" if $Verbose>=2;
	mv_all_methods("MY",$newclass);
	bless $self, $newclass;
	push @Parent, $self;
	@{"$newclass\:\:ISA"} = 'MM';
    }

    if (defined $Parent[-2]){
	$self->{PARENT} = $Parent[-2];
	my $key;
	for $key (keys %Prepend_dot_dot) {
	    next unless defined $self->{PARENT}{$key};
	    $self->{$key} = $self->{PARENT}{$key};
		# PERL and FULLPERL may be command verbs instead of full
		# file specifications under VMS.  If so, don't turn them
		# into a filespec.
	    $self->{$key} = $self->catdir("..",$self->{$key})
		unless $self->file_name_is_absolute($self->{$key})
		|| ($^O eq 'VMS' and ($key =~ /PERL$/ && $self->{$key} =~ /^[\w\-\$]+$/));
	}
	if ($self->{PARENT}) {
	    $self->{PARENT}->{CHILDREN}->{$newclass} = $self;
	    foreach my $opt (qw(CAPI POLLUTE)) {
		if (exists $self->{PARENT}->{$opt}
		    and not exists $self->{$opt})
		    {
			# inherit, but only if already unspecified
			$self->{$opt} = $self->{PARENT}->{$opt};
		    }
	    }
	}
    } else {
	parse_args($self,split(' ', $ENV{PERL_MM_OPT} || ''),@ARGV);
    }

    $self->{NAME} ||= $self->guess_name;

    ($self->{NAME_SYM} = $self->{NAME}) =~ s/\W+/_/g;

    $self->init_main();

    if (! $self->{PERL_SRC} ) {
	my($pthinks) = $self->canonpath($INC{'Config.pm'});
	my($cthinks) = $self->catfile($Config{'archlibexp'},'Config.pm');
	$pthinks = VMS::Filespec::vmsify($pthinks) if $Is_VMS;
	if ($pthinks ne $cthinks &&
	    !($Is_Win32 and lc($pthinks) eq lc($cthinks))) {
            print "Have $pthinks expected $cthinks\n";
	    if ($Is_Win32) {
		$pthinks =~ s![/\\]Config\.pm$!!i; $pthinks =~ s!.*[/\\]!!;
	    }
	    else {
		$pthinks =~ s!/Config\.pm$!!; $pthinks =~ s!.*/!!;
	    }
	    print STDOUT <<END unless $self->{UNINSTALLED_PERL};
Your perl and your Config.pm seem to have different ideas about the architecture
they are running on.
Perl thinks: [$pthinks]
Config says: [$Config{archname}]
This may or may not cause problems. Please check your installation of perl if you
have problems building this extension.
END
	}
    }

    $self->init_dirscan();
    $self->init_others();
    my($argv) = neatvalue(\@ARGV);
    $argv =~ s/^\[/(/;
    $argv =~ s/\]$/)/;

    push @{$self->{RESULT}}, <<END;
# This Makefile is for the $self->{NAME} extension to perl.
#
# It was generated automatically by MakeMaker version
# $VERSION (Revision: $Revision) from the contents of
# Makefile.PL. Don't edit this file, edit Makefile.PL instead.
#
#	ANY CHANGES MADE HERE WILL BE LOST!
#
#   MakeMaker ARGV: $argv
#
#   MakeMaker Parameters:
END

    foreach $key (sort keys %initial_att){
	my($v) = neatvalue($initial_att{$key});
	$v =~ s/(CODE|HASH|ARRAY|SCALAR)\([\dxa-f]+\)/$1\(...\)/;
	$v =~ tr/\n/ /s;
	push @{$self->{RESULT}}, "#	$key => $v";
    }

    # turn the SKIP array into a SKIPHASH hash
    my (%skip,$skip);
    for $skip (@{$self->{SKIP} || []}) {
	$self->{SKIPHASH}{$skip} = 1;
    }
    delete $self->{SKIP}; # free memory

    if ($self->{PARENT}) {
	for (qw/install dist dist_basics dist_core dist_dir dist_test dist_ci/) {
	    $self->{SKIPHASH}{$_} = 1;
	}
    }

    # We run all the subdirectories now. They don't have much to query
    # from the parent, but the parent has to query them: if they need linking!
    unless ($self->{NORECURS}) {
	$self->eval_in_subdirs if @{$self->{DIR}};
    }

    my $section;
    foreach $section ( @MM_Sections ){
	print "Processing Makefile '$section' section\n" if ($Verbose >= 2);
	my($skipit) = $self->skipcheck($section);
	if ($skipit){
	    push @{$self->{RESULT}}, "\n# --- MakeMaker $section section $skipit.";
	} else {
	    my(%a) = %{$self->{$section} || {}};
	    push @{$self->{RESULT}}, "\n# --- MakeMaker $section section:";
	    push @{$self->{RESULT}}, "# " . join ", ", %a if $Verbose && %a;
	    push @{$self->{RESULT}}, $self->nicetext($self->$section( %a ));
	}
    }

    push @{$self->{RESULT}}, "\n# End.";

    $self;
}

sub WriteEmptyMakefile {
  if (-f 'Makefile.old') {
    chmod 0666, 'Makefile.old';
    unlink 'Makefile.old' or warn "unlink Makefile.old: $!";
  }
  rename 'Makefile', 'Makefile.old' or warn "rename Makefile Makefile.old: $!"
    if -f 'Makefile';
  open MF, '> Makefile' or die "open Makefile for write: $!";
  print MF <<'EOP';
all:

clean:

install:

makemakerdflt:

test:

EOP
  close MF or die "close Makefile for write: $!";
}

sub check_manifest {
    print STDOUT "Checking if your kit is complete...\n";
    require ExtUtils::Manifest;
    $ExtUtils::Manifest::Quiet=$ExtUtils::Manifest::Quiet=1; #avoid warning
    my(@missed)=ExtUtils::Manifest::manicheck();
    if (@missed){
	print STDOUT "Warning: the following files are missing in your kit:\n";
	print "\t", join "\n\t", @missed;
	print STDOUT "\n";
	print STDOUT "Please inform the author.\n";
    } else {
	print STDOUT "Looks good\n";
    }
}

sub parse_args{
    my($self, @args) = @_;
    foreach (@args){
	unless (m/(.*?)=(.*)/){
	    help(),exit 1 if m/^help$/;
	    ++$Verbose if m/^verb/;
	    next;
	}
	my($name, $value) = ($1, $2);
	if ($value =~ m/^~(\w+)?/){ # tilde with optional username
	    $value =~ s [^~(\w*)]
		[$1 ?
		 ((getpwnam($1))[7] || "~$1") :
		 (getpwuid($>))[7]
		 ]ex;
	}
	$self->{uc($name)} = $value;
    }

    # catch old-style 'potential_libs' and inform user how to 'upgrade'
    if (defined $self->{potential_libs}){
	my($msg)="'potential_libs' => '$self->{potential_libs}' should be";
	if ($self->{potential_libs}){
	    print STDOUT "$msg changed to:\n\t'LIBS' => ['$self->{potential_libs}']\n";
	} else {
	    print STDOUT "$msg deleted.\n";
	}
	$self->{LIBS} = [$self->{potential_libs}];
	delete $self->{potential_libs};
    }
    # catch old-style 'ARMAYBE' and inform user how to 'upgrade'
    if (defined $self->{ARMAYBE}){
	my($armaybe) = $self->{ARMAYBE};
	print STDOUT "ARMAYBE => '$armaybe' should be changed to:\n",
			"\t'dynamic_lib' => {ARMAYBE => '$armaybe'}\n";
	my(%dl) = %{$self->{dynamic_lib} || {}};
	$self->{dynamic_lib} = { %dl, ARMAYBE => $armaybe};
	delete $self->{ARMAYBE};
    }
    if (defined $self->{LDTARGET}){
	print STDOUT "LDTARGET should be changed to LDFROM\n";
	$self->{LDFROM} = $self->{LDTARGET};
	delete $self->{LDTARGET};
    }
    # Turn a DIR argument on the command line into an array
    if (defined $self->{DIR} && ref \$self->{DIR} eq 'SCALAR') {
	# So they can choose from the command line, which extensions they want
	# the grep enables them to have some colons too much in case they
	# have to build a list with the shell
	$self->{DIR} = [grep $_, split ":", $self->{DIR}];
    }
    # Turn a INCLUDE_EXT argument on the command line into an array
    if (defined $self->{INCLUDE_EXT} && ref \$self->{INCLUDE_EXT} eq 'SCALAR') {
	$self->{INCLUDE_EXT} = [grep $_, split '\s+', $self->{INCLUDE_EXT}];
    }
    # Turn a EXCLUDE_EXT argument on the command line into an array
    if (defined $self->{EXCLUDE_EXT} && ref \$self->{EXCLUDE_EXT} eq 'SCALAR') {
	$self->{EXCLUDE_EXT} = [grep $_, split '\s+', $self->{EXCLUDE_EXT}];
    }
    my $mmkey;
    foreach $mmkey (sort keys %$self){
	print STDOUT "	$mmkey => ", neatvalue($self->{$mmkey}), "\n" if $Verbose;
	print STDOUT "'$mmkey' is not a known MakeMaker parameter name.\n"
	    unless exists $Recognized_Att_Keys{$mmkey};
    }
    $| = 1 if $Verbose;
}

sub check_hints {
    my($self) = @_;
    # We allow extension-specific hints files.

    return unless -d "hints";

    # First we look for the best hintsfile we have
    my(@goodhints);
    my($hint)="${^O}_$Config{osvers}";
    $hint =~ s/\./_/g;
    $hint =~ s/_$//;
    return unless $hint;

    # Also try without trailing minor version numbers.
    while (1) {
	last if -f "hints/$hint.pl";      # found
    } continue {
	last unless $hint =~ s/_[^_]*$//; # nothing to cut off
    }
    return unless -f "hints/$hint.pl";    # really there

    # execute the hintsfile:
#    use FileHandle ();
#    my $fh = new FileHandle;
#    $fh->open("hints/$hint.pl");
    local *FH;
    open(FH,"hints/$hint.pl");
#    @goodhints = <$fh>;
    @goodhints = <FH>;
#    $fh->close;
    close FH;
    print STDOUT "Processing hints file hints/$hint.pl\n";
    eval join('',@goodhints);
    print STDOUT $@ if $@;
}

sub mv_all_methods {
    my($from,$to) = @_;
    my($method);
    my($symtab) = \%{"${from}::"};
#    no strict;

    # Here you see the *current* list of methods that are overridable
    # from Makefile.PL via MY:: subroutines. As of VERSION 5.07 I'm
    # still trying to reduce the list to some reasonable minimum --
    # because I want to make it easier for the user. A.K.

    foreach $method (@Overridable) {

	# We cannot say "next" here. Nick might call MY->makeaperl
	# which isn't defined right now

	# Above statement was written at 4.23 time when Tk-b8 was
	# around. As Tk-b9 only builds with 5.002something and MM 5 is
	# standard, we try to enable the next line again. It was
	# commented out until MM 5.23

	next unless defined &{"${from}::$method"};

	*{"${to}::$method"} = \&{"${from}::$method"};

	# delete would do, if we were sure, nobody ever called
	# MY->makeaperl directly
	
	# delete $symtab->{$method};
	
	# If we delete a method, then it will be undefined and cannot
	# be called.  But as long as we have Makefile.PLs that rely on
	# %MY:: being intact, we have to fill the hole with an
	# inheriting method:

	eval "package MY; sub $method { shift->SUPER::$method(\@_); }";
    }

    # We have to clean out %INC also, because the current directory is
    # changed frequently and Graham Barr prefers to get his version
    # out of a History.pl file which is "required" so woudn't get
    # loaded again in another extension requiring a History.pl

    # With perl5.002_01 the deletion of entries in %INC caused Tk-b11
    # to core dump in the middle of a require statement. The required
    # file was Tk/MMutil.pm.  The consequence is, we have to be
    # extremely careful when we try to give perl a reason to reload a
    # library with same name.  The workaround prefers to drop nothing
    # from %INC and teach the writers not to use such libraries.

#    my $inc;
#    foreach $inc (keys %INC) {
#	#warn "***$inc*** deleted";
#	delete $INC{$inc};
#    }
}

sub skipcheck {
    my($self) = shift;
    my($section) = @_;
    if ($section eq 'dynamic') {
	print STDOUT "Warning (non-fatal): Target 'dynamic' depends on targets ",
	"in skipped section 'dynamic_bs'\n"
            if $self->{SKIPHASH}{dynamic_bs} && $Verbose;
        print STDOUT "Warning (non-fatal): Target 'dynamic' depends on targets ",
	"in skipped section 'dynamic_lib'\n"
            if $self->{SKIPHASH}{dynamic_lib} && $Verbose;
    }
    if ($section eq 'dynamic_lib') {
        print STDOUT "Warning (non-fatal): Target '\$(INST_DYNAMIC)' depends on ",
	"targets in skipped section 'dynamic_bs'\n"
            if $self->{SKIPHASH}{dynamic_bs} && $Verbose;
    }
    if ($section eq 'static') {
        print STDOUT "Warning (non-fatal): Target 'static' depends on targets ",
	"in skipped section 'static_lib'\n"
            if $self->{SKIPHASH}{static_lib} && $Verbose;
    }
    return 'skipped' if $self->{SKIPHASH}{$section};
    return '';
}

sub flush {
    my $self = shift;
    my($chunk);
#    use FileHandle ();
#    my $fh = new FileHandle;
    local *FH;
    print STDOUT "Writing $self->{MAKEFILE} for $self->{NAME}\n";

    unlink($self->{MAKEFILE}, "MakeMaker.tmp", $Is_VMS ? 'Descrip.MMS' : '');
#    $fh->open(">MakeMaker.tmp") or die "Unable to open MakeMaker.tmp: $!";
    open(FH,">MakeMaker.tmp") or die "Unable to open MakeMaker.tmp: $!";

    for $chunk (@{$self->{RESULT}}) {
#	print $fh "$chunk\n";
	print FH "$chunk\n";
    }

#    $fh->close;
    close FH;
    my($finalname) = $self->{MAKEFILE};
    rename("MakeMaker.tmp", $finalname);
    chmod 0644, $finalname unless $Is_VMS;

    if ($self->{PARENT}) {
	foreach (keys %$self) { # safe memory
	    delete $self->{$_} unless $Keep_after_flush{$_};
	}
    }

    system("$Config::Config{eunicefix} $finalname") unless $Config::Config{eunicefix} eq ":";
}

# The following mkbootstrap() is only for installations that are calling
# the pre-4.1 mkbootstrap() from their old Makefiles. This MakeMaker
# writes Makefiles, that use ExtUtils::Mkbootstrap directly.
sub mkbootstrap {
    die <<END;
!!! Your Makefile has been built such a long time ago, !!!
!!! that is unlikely to work with current MakeMaker.   !!!
!!! Please rebuild your Makefile                       !!!
END
}

# Ditto for mksymlists() as of MakeMaker 5.17
sub mksymlists {
    die <<END;
!!! Your Makefile has been built such a long time ago, !!!
!!! that is unlikely to work with current MakeMaker.   !!!
!!! Please rebuild your Makefile                       !!!
END
}

sub neatvalue {
    my($v) = @_;
    return "undef" unless defined $v;
    my($t) = ref $v;
    return "q[$v]" unless $t;
    if ($t eq 'ARRAY') {
	my(@m, $elem, @neat);
	push @m, "[";
	foreach $elem (@$v) {
	    push @neat, "q[$elem]";
	}
	push @m, join ", ", @neat;
	push @m, "]";
	return join "", @m;
    }
    return "$v" unless $t eq 'HASH';
    my(@m, $key, $val);
    while (($key,$val) = each %$v){
	last unless defined $key; # cautious programming in case (undef,undef) is true
	push(@m,"$key=>".neatvalue($val)) ;
    }
    return "{ ".join(', ',@m)." }";
}

sub selfdocument {
    my($self) = @_;
    my(@m);
    if ($Verbose){
	push @m, "\n# Full list of MakeMaker attribute values:";
	foreach $key (sort keys %$self){
	    next if $key eq 'RESULT' || $key =~ /^[A-Z][a-z]/;
	    my($v) = neatvalue($self->{$key});
	    $v =~ s/(CODE|HASH|ARRAY|SCALAR)\([\dxa-f]+\)/$1\(...\)/;
	    $v =~ tr/\n/ /s;
	    push @m, "#	$key => $v";
	}
    }
    join "\n", @m;
}

package ExtUtils::MakeMaker;
1;

__END__

=head1 NAME

ExtUtils::MakeMaker - create an extension Makefile

=head1 SYNOPSIS

C<use ExtUtils::MakeMaker;>

C<WriteMakefile( ATTRIBUTE =E<gt> VALUE [, ...] );>

which is really

C<MM-E<gt>new(\%att)-E<gt>flush;>

=head1 DESCRIPTION

This utility is designed to write a Makefile for an extension module
from a Makefile.PL. It is based on the Makefile.SH model provided by
Andy Dougherty and the perl5-porters.

It splits the task of generating the Makefile into several subroutines
that can be individually overridden.  Each subroutine returns the text
it wishes to have written to the Makefile.

MakeMaker is object oriented. Each directory below the current
directory that contains a Makefile.PL. Is treated as a separate
object. This makes it possible to write an unlimited number of
Makefiles with a single invocation of WriteMakefile().

=head2 How To Write A Makefile.PL

The short answer is: Don't.

        Always begin with h2xs.
        Always begin with h2xs!
        ALWAYS BEGIN WITH H2XS!

even if you're not building around a header file, and even if you
don't have an XS component.

Run h2xs(1) before you start thinking about writing a module. For so
called pm-only modules that consist of C<*.pm> files only, h2xs has
the C<-X> switch. This will generate dummy files of all kinds that are
useful for the module developer.

The medium answer is:

    use ExtUtils::MakeMaker;
    WriteMakefile( NAME => "Foo::Bar" );

The long answer is the rest of the manpage :-)

=head2 Default Makefile Behaviour

The generated Makefile enables the user of the extension to invoke

  perl Makefile.PL # optionally "perl Makefile.PL verbose"
  make
  make test        # optionally set TEST_VERBOSE=1
  make install     # See below

The Makefile to be produced may be altered by adding arguments of the
form C<KEY=VALUE>. E.g.

  perl Makefile.PL PREFIX=/tmp/myperl5

Other interesting targets in the generated Makefile are

  make config     # to check if the Makefile is up-to-date
  make clean      # delete local temp files (Makefile gets renamed)
  make realclean  # delete derived files (including ./blib)
  make ci         # check in all the files in the MANIFEST file
  make dist       # see below the Distribution Support section

=head2 make test

MakeMaker checks for the existence of a file named F<test.pl> in the
current directory and if it exists it adds commands to the test target
of the generated Makefile that will execute the script with the proper
set of perl C<-I> options.

MakeMaker also checks for any files matching glob("t/*.t"). It will
add commands to the test target of the generated Makefile that execute
all matching files via the L<Test::Harness> module with the C<-I>
switches set correctly.

=head2 make testdb

A useful variation of the above is the target C<testdb>. It runs the
test under the Perl debugger (see L<perldebug>). If the file
F<test.pl> exists in the current directory, it is used for the test.

If you want to debug some other testfile, set C<TEST_FILE> variable
thusly:

  make testdb TEST_FILE=t/mytest.t

By default the debugger is called using C<-d> option to perl. If you
want to specify some other option, set C<TESTDB_SW> variable:

  make testdb TESTDB_SW=-Dx

=head2 make install

make alone puts all relevant files into directories that are named by
the macros INST_LIB, INST_ARCHLIB, INST_SCRIPT, INST_HTMLLIBDIR,
INST_HTMLSCRIPTDIR, INST_MAN1DIR, and INST_MAN3DIR.  All these default
to something below ./blib if you are I<not> building below the perl
source directory. If you I<are> building below the perl source,
INST_LIB and INST_ARCHLIB default to ../../lib, and INST_SCRIPT is not
defined.

The I<install> target of the generated Makefile copies the files found
below each of the INST_* directories to their INSTALL*
counterparts. Which counterparts are chosen depends on the setting of
INSTALLDIRS according to the following table:

		       	         INSTALLDIRS set to
       	       	              perl   	          site

    INST_ARCHLIB	INSTALLARCHLIB        INSTALLSITEARCH
    INST_LIB		INSTALLPRIVLIB        INSTALLSITELIB
    INST_HTMLLIBDIR	INSTALLHTMLPRIVLIBDIR INSTALLHTMLSITELIBDIR
    INST_HTMLSCRIPTDIR            INSTALLHTMLSCRIPTDIR
    INST_BIN			  INSTALLBIN
    INST_SCRIPT                   INSTALLSCRIPT
    INST_MAN1DIR                  INSTALLMAN1DIR
    INST_MAN3DIR                  INSTALLMAN3DIR

The INSTALL... macros in turn default to their %Config
($Config{installprivlib}, $Config{installarchlib}, etc.) counterparts.

You can check the values of these variables on your system with

    perl '-V:install.*'

And to check the sequence in which the library directories are
searched by perl, run

    perl -le 'print join $/, @INC'


=head2 PREFIX and LIB attribute

PREFIX and LIB can be used to set several INSTALL* attributes in one
go. The quickest way to install a module in a non-standard place might
be

    perl Makefile.PL LIB=~/lib

This will install the module's architecture-independent files into
~/lib, the architecture-dependent files into ~/lib/$archname.

Another way to specify many INSTALL directories with a single
parameter is PREFIX.

    perl Makefile.PL PREFIX=~

This will replace the string specified by C<$Config{prefix}> in all
C<$Config{install*}> values.

Note, that in both cases the tilde expansion is done by MakeMaker, not
by perl by default, nor by make.

Conflicts between parameters LIB,
PREFIX and the various INSTALL* arguments are resolved so that:

=over 4

=item *

setting LIB overrides any setting of INSTALLPRIVLIB, INSTALLARCHLIB,
INSTALLSITELIB, INSTALLSITEARCH (and they are not affected by PREFIX);

=item *

without LIB, setting PREFIX replaces the initial C<$Config{prefix}>
part of those INSTALL* arguments, even if the latter are explicitly
set (but are set to still start with C<$Config{prefix}>).

=back

If the user has superuser privileges, and is not working on AFS
or relatives, then the defaults for
INSTALLPRIVLIB, INSTALLARCHLIB, INSTALLSCRIPT, etc. will be appropriate,
and this incantation will be the best:

    perl Makefile.PL; make; make test
    make install

make install per default writes some documentation of what has been
done into the file C<$(INSTALLARCHLIB)/perllocal.pod>. This feature
can be bypassed by calling make pure_install.

=head2 AFS users

will have to specify the installation directories as these most
probably have changed since perl itself has been installed. They will
have to do this by calling

    perl Makefile.PL INSTALLSITELIB=/afs/here/today \
	INSTALLSCRIPT=/afs/there/now INSTALLMAN3DIR=/afs/for/manpages
    make

Be careful to repeat this procedure every time you recompile an
extension, unless you are sure the AFS installation directories are
still valid.

=head2 Static Linking of a new Perl Binary

An extension that is built with the above steps is ready to use on
systems supporting dynamic loading. On systems that do not support
dynamic loading, any newly created extension has to be linked together
with the available resources. MakeMaker supports the linking process
by creating appropriate targets in the Makefile whenever an extension
is built. You can invoke the corresponding section of the makefile with

    make perl

That produces a new perl binary in the current directory with all
extensions linked in that can be found in INST_ARCHLIB , SITELIBEXP,
and PERL_ARCHLIB. To do that, MakeMaker writes a new Makefile, on
UNIX, this is called Makefile.aperl (may be system dependent). If you
want to force the creation of a new perl, it is recommended, that you
delete this Makefile.aperl, so the directories are searched-through
for linkable libraries again.

The binary can be installed into the directory where perl normally
resides on your machine with

    make inst_perl

To produce a perl binary with a different name than C<perl>, either say

    perl Makefile.PL MAP_TARGET=myperl
    make myperl
    make inst_perl

or say

    perl Makefile.PL
    make myperl MAP_TARGET=myperl
    make inst_perl MAP_TARGET=myperl

In any case you will be prompted with the correct invocation of the
C<inst_perl> target that installs the new binary into INSTALLBIN.

make inst_perl per default writes some documentation of what has been
done into the file C<$(INSTALLARCHLIB)/perllocal.pod>. This
can be bypassed by calling make pure_inst_perl.

Warning: the inst_perl: target will most probably overwrite your
existing perl binary. Use with care!

Sometimes you might want to build a statically linked perl although
your system supports dynamic loading. In this case you may explicitly
set the linktype with the invocation of the Makefile.PL or make:

    perl Makefile.PL LINKTYPE=static    # recommended

or

    make LINKTYPE=static                # works on most systems

=head2 Determination of Perl Library and Installation Locations

MakeMaker needs to know, or to guess, where certain things are
located.  Especially INST_LIB and INST_ARCHLIB (where to put the files
during the make(1) run), PERL_LIB and PERL_ARCHLIB (where to read
existing modules from), and PERL_INC (header files and C<libperl*.*>).

Extensions may be built either using the contents of the perl source
directory tree or from the installed perl library. The recommended way
is to build extensions after you have run 'make install' on perl
itself. You can do that in any directory on your hard disk that is not
below the perl source tree. The support for extensions below the ext
directory of the perl distribution is only good for the standard
extensions that come with perl.

If an extension is being built below the C<ext/> directory of the perl
source then MakeMaker will set PERL_SRC automatically (e.g.,
C<../..>).  If PERL_SRC is defined and the extension is recognized as
a standard extension, then other variables default to the following:

  PERL_INC     = PERL_SRC
  PERL_LIB     = PERL_SRC/lib
  PERL_ARCHLIB = PERL_SRC/lib
  INST_LIB     = PERL_LIB
  INST_ARCHLIB = PERL_ARCHLIB

If an extension is being built away from the perl source then MakeMaker
will leave PERL_SRC undefined and default to using the installed copy
of the perl library. The other variables default to the following:

  PERL_INC     = $archlibexp/CORE
  PERL_LIB     = $privlibexp
  PERL_ARCHLIB = $archlibexp
  INST_LIB     = ./blib/lib
  INST_ARCHLIB = ./blib/arch

If perl has not yet been installed then PERL_SRC can be defined on the
command line as shown in the previous section.


=head2 Which architecture dependent directory?

If you don't want to keep the defaults for the INSTALL* macros,
MakeMaker helps you to minimize the typing needed: the usual
relationship between INSTALLPRIVLIB and INSTALLARCHLIB is determined
by Configure at perl compilation time. MakeMaker supports the user who
sets INSTALLPRIVLIB. If INSTALLPRIVLIB is set, but INSTALLARCHLIB not,
then MakeMaker defaults the latter to be the same subdirectory of
INSTALLPRIVLIB as Configure decided for the counterparts in %Config ,
otherwise it defaults to INSTALLPRIVLIB. The same relationship holds
for INSTALLSITELIB and INSTALLSITEARCH.

MakeMaker gives you much more freedom than needed to configure
internal variables and get different results. It is worth to mention,
that make(1) also lets you configure most of the variables that are
used in the Makefile. But in the majority of situations this will not
be necessary, and should only be done if the author of a package
recommends it (or you know what you're doing).

=head2 Using Attributes and Parameters

The following attributes can be specified as arguments to WriteMakefile()
or as NAME=VALUE pairs on the command line:

=over 2

=item ABSTRACT

One line description of the module. Will be included in PPD file.

=item ABSTRACT_FROM

Name of the file that contains the package description. MakeMaker looks
for a line in the POD matching /^($package\s-\s)(.*)/. This is typically
the first line in the "=head1 NAME" section. $2 becomes the abstract.

=item AUTHOR

String containing name (and email address) of package author(s). Is used
in PPD (Perl Package Description) files for PPM (Perl Package Manager).

=item BINARY_LOCATION

Used when creating PPD files for binary packages.  It can be set to a
full or relative path or URL to the binary archive for a particular
architecture.  For example:

	perl Makefile.PL BINARY_LOCATION=x86/Agent.tar.gz

builds a PPD package that references a binary of the C<Agent> package,
located in the C<x86> directory relative to the PPD itself.

=item C

Ref to array of *.c file names. Initialised from a directory scan
and the values portion of the XS attribute hash. This is not
currently used by MakeMaker but may be handy in Makefile.PLs.

=item CAPI

[This attribute is obsolete in Perl 5.6.  PERL_OBJECT builds are C-compatible
by default.]

Switch to force usage of the Perl C API even when compiling for PERL_OBJECT.

Note that this attribute is passed through to any recursive build,
but if and only if the submodule's Makefile.PL itself makes no mention
of the 'CAPI' attribute.

=item CCFLAGS

String that will be included in the compiler call command line between
the arguments INC and OPTIMIZE.

=item CONFIG

Arrayref. E.g. [qw(archname manext)] defines ARCHNAME & MANEXT from
config.sh. MakeMaker will add to CONFIG the following values anyway:
ar
cc
cccdlflags
ccdlflags
dlext
dlsrc
ld
lddlflags
ldflags
libc
lib_ext
obj_ext
ranlib
sitelibexp
sitearchexp
so

=item CONFIGURE

CODE reference. The subroutine should return a hash reference. The
hash may contain further attributes, e.g. {LIBS =E<gt> ...}, that have to
be determined by some evaluation method.

=item DEFINE

Something like C<"-DHAVE_UNISTD_H">

=item DIR

Ref to array of subdirectories containing Makefile.PLs e.g. [ 'sdbm'
] in ext/SDBM_File

=item DISTNAME

Your name for distributing the package (by tar file). This defaults to
NAME above.

=item DL_FUNCS

Hashref of symbol names for routines to be made available as universal
symbols.  Each key/value pair consists of the package name and an
array of routine names in that package.  Used only under AIX, OS/2,
VMS and Win32 at present.  The routine names supplied will be expanded
in the same way as XSUB names are expanded by the XS() macro.
Defaults to

  {"$(NAME)" => ["boot_$(NAME)" ] }

e.g.

  {"RPC" => [qw( boot_rpcb rpcb_gettime getnetconfigent )],
   "NetconfigPtr" => [ 'DESTROY'] }

Please see the L<ExtUtils::Mksymlists> documentation for more information
about the DL_FUNCS, DL_VARS and FUNCLIST attributes.

=item DL_VARS

Array of symbol names for variables to be made available as universal symbols.
Used only under AIX, OS/2, VMS and Win32 at present.  Defaults to [].
(e.g. [ qw(Foo_version Foo_numstreams Foo_tree ) ])

=item EXCLUDE_EXT

Array of extension names to exclude when doing a static build.  This
is ignored if INCLUDE_EXT is present.  Consult INCLUDE_EXT for more
details.  (e.g.  [ qw( Socket POSIX ) ] )

This attribute may be most useful when specified as a string on the
command line:  perl Makefile.PL EXCLUDE_EXT='Socket Safe'

=item EXE_FILES

Ref to array of executable files. The files will be copied to the
INST_SCRIPT directory. Make realclean will delete them from there
again.

=item FIRST_MAKEFILE

The name of the Makefile to be produced. Defaults to the contents of
MAKEFILE, but can be overridden. This is used for the second Makefile
that will be produced for the MAP_TARGET.

=item FULLPERL

Perl binary able to run this extension.

=item FUNCLIST

This provides an alternate means to specify function names to be
exported from the extension.  Its value is a reference to an
array of function names to be exported by the extension.  These
names are passed through unaltered to the linker options file.

=item H

Ref to array of *.h file names. Similar to C.

=item HTMLLIBPODS

Hashref of .pm and .pod files.  MakeMaker will default this to all
 .pod and any .pm files that include POD directives.  The files listed
here will be converted to HTML format and installed as was requested
at Configure time.

=item HTMLSCRIPTPODS

Hashref of pod-containing files.  MakeMaker will default this to all
EXE_FILES files that include POD directives.  The files listed
here will be converted to HTML format and installed as was requested
at Configure time.

=item IMPORTS

This attribute is used to specify names to be imported into the
extension. It is only used on OS/2 and Win32.

=item INC

Include file dirs eg: C<"-I/usr/5include -I/path/to/inc">

=item INCLUDE_EXT

Array of extension names to be included when doing a static build.
MakeMaker will normally build with all of the installed extensions when
doing a static build, and that is usually the desired behavior.  If
INCLUDE_EXT is present then MakeMaker will build only with those extensions
which are explicitly mentioned. (e.g.  [ qw( Socket POSIX ) ])

It is not necessary to mention DynaLoader or the current extension when
filling in INCLUDE_EXT.  If the INCLUDE_EXT is mentioned but is empty then
only DynaLoader and the current extension will be included in the build.

This attribute may be most useful when specified as a string on the
command line:  perl Makefile.PL INCLUDE_EXT='POSIX Socket Devel::Peek'

=item INSTALLARCHLIB

Used by 'make install', which copies files from INST_ARCHLIB to this
directory if INSTALLDIRS is set to perl.

=item INSTALLBIN

Directory to install binary files (e.g. tkperl) into.

=item INSTALLDIRS

Determines which of the two sets of installation directories to
choose: installprivlib and installarchlib versus installsitelib and
installsitearch. The first pair is chosen with INSTALLDIRS=perl, the
second with INSTALLDIRS=site. Default is site.

=item INSTALLHTMLPRIVLIBDIR

This directory gets the HTML pages at 'make install' time. Defaults to
$Config{installhtmlprivlibdir}.

=item INSTALLHTMLSCRIPTDIR

This directory gets the HTML pages at 'make install' time. Defaults to
$Config{installhtmlscriptdir}.

=item INSTALLHTMLSITELIBDIR

This directory gets the HTML pages at 'make install' time. Defaults to
$Config{installhtmlsitelibdir}.


=item INSTALLMAN1DIR

This directory gets the man pages at 'make install' time. Defaults to
$Config{installman1dir}.

=item INSTALLMAN3DIR

This directory gets the man pages at 'make install' time. Defaults to
$Config{installman3dir}.

=item INSTALLPRIVLIB

Used by 'make install', which copies files from INST_LIB to this
directory if INSTALLDIRS is set to perl.

=item INSTALLSCRIPT

Used by 'make install' which copies files from INST_SCRIPT to this
directory.

=item INSTALLSITEARCH

Used by 'make install', which copies files from INST_ARCHLIB to this
directory if INSTALLDIRS is set to site (default).

=item INSTALLSITELIB

Used by 'make install', which copies files from INST_LIB to this
directory if INSTALLDIRS is set to site (default).

=item INST_ARCHLIB

Same as INST_LIB for architecture dependent files.

=item INST_BIN

Directory to put real binary files during 'make'. These will be copied
to INSTALLBIN during 'make install'

=item INST_EXE

Old name for INST_SCRIPT. Deprecated. Please use INST_SCRIPT if you
need to use it.

=item INST_HTMLLIBDIR

Directory to hold the man pages in HTML format at 'make' time

=item INST_HTMLSCRIPTDIR

Directory to hold the man pages in HTML format at 'make' time

=item INST_LIB

Directory where we put library files of this extension while building
it.

=item INST_MAN1DIR

Directory to hold the man pages at 'make' time

=item INST_MAN3DIR

Directory to hold the man pages at 'make' time

=item INST_SCRIPT

Directory, where executable files should be installed during
'make'. Defaults to "./blib/script", just to have a dummy location during
testing. make install will copy the files in INST_SCRIPT to
INSTALLSCRIPT.

=item LDFROM

defaults to "$(OBJECT)" and is used in the ld command to specify
what files to link/load from (also see dynamic_lib below for how to
specify ld flags)

=item LIB

LIB should only be set at C<perl Makefile.PL> time but is allowed as a
MakeMaker argument. It has the effect of
setting both INSTALLPRIVLIB and INSTALLSITELIB to that value regardless any
explicit setting of those arguments (or of PREFIX).  
INSTALLARCHLIB and INSTALLSITEARCH are set to the corresponding 
architecture subdirectory.

=item LIBPERL_A

The filename of the perllibrary that will be used together with this
extension. Defaults to libperl.a.

=item LIBS

An anonymous array of alternative library
specifications to be searched for (in order) until
at least one library is found. E.g.

  'LIBS' => ["-lgdbm", "-ldbm -lfoo", "-L/path -ldbm.nfs"]

Mind, that any element of the array
contains a complete set of arguments for the ld
command. So do not specify

  'LIBS' => ["-ltcl", "-ltk", "-lX11"]

See ODBM_File/Makefile.PL for an example, where an array is needed. If
you specify a scalar as in

  'LIBS' => "-ltcl -ltk -lX11"

MakeMaker will turn it into an array with one element.

=item LINKTYPE

'static' or 'dynamic' (default unless usedl=undef in
config.sh). Should only be used to force static linking (also see
linkext below).

=item MAKEAPERL

Boolean which tells MakeMaker, that it should include the rules to
make a perl. This is handled automatically as a switch by
MakeMaker. The user normally does not need it.

=item MAKEFILE

The name of the Makefile to be produced.

=item MAN1PODS

Hashref of pod-containing files. MakeMaker will default this to all
EXE_FILES files that include POD directives. The files listed
here will be converted to man pages and installed as was requested
at Configure time.

=item MAN3PODS

Hashref of .pm and .pod files. MakeMaker will default this to all
 .pod and any .pm files that include POD directives. The files listed
here will be converted to man pages and installed as was requested
at Configure time.

=item MAP_TARGET

If it is intended, that a new perl binary be produced, this variable
may hold a name for that binary. Defaults to perl

=item MYEXTLIB

If the extension links to a library that it builds set this to the
name of the library (see SDBM_File)

=item NAME

Perl module name for this extension (DBD::Oracle). This will default
to the directory name but should be explicitly defined in the
Makefile.PL.

=item NEEDS_LINKING

MakeMaker will figure out if an extension contains linkable code
anywhere down the directory tree, and will set this variable
accordingly, but you can speed it up a very little bit if you define
this boolean variable yourself.

=item NOECHO

Defaults to C<@>. By setting it to an empty string you can generate a
Makefile that echos all commands. Mainly used in debugging MakeMaker
itself.

=item NORECURS

Boolean.  Attribute to inhibit descending into subdirectories.

=item NO_VC

In general, any generated Makefile checks for the current version of
MakeMaker and the version the Makefile was built under. If NO_VC is
set, the version check is neglected. Do not write this into your
Makefile.PL, use it interactively instead.

=item OBJECT

List of object files, defaults to '$(BASEEXT)$(OBJ_EXT)', but can be a long
string containing all object files, e.g. "tkpBind.o
tkpButton.o tkpCanvas.o"

(Where BASEEXT is the last component of NAME, and OBJ_EXT is $Config{obj_ext}.)

=item OPTIMIZE

Defaults to C<-O>. Set it to C<-g> to turn debugging on. The flag is
passed to subdirectory makes.

=item PERL

Perl binary for tasks that can be done by miniperl

=item PERLMAINCC

The call to the program that is able to compile perlmain.c. Defaults
to $(CC).

=item PERL_ARCHLIB

Same as below, but for architecture dependent files.

=item PERL_LIB

Directory containing the Perl library to use.

=item PERL_MALLOC_OK

defaults to 0.  Should be set to TRUE if the extension can work with
the memory allocation routines substituted by the Perl malloc() subsystem.
This should be applicable to most extensions with exceptions of those

=over 4

=item *

with bugs in memory allocations which are caught by Perl's malloc();

=item *

which interact with the memory allocator in other ways than via
malloc(), realloc(), free(), calloc(), sbrk() and brk();

=item *

which rely on special alignment which is not provided by Perl's malloc().

=back

B<NOTE.>  Negligence to set this flag in I<any one> of loaded extension
nullifies many advantages of Perl's malloc(), such as better usage of
system resources, error detection, memory usage reporting, catchable failure
of memory allocations, etc.

=item PERL_SRC

Directory containing the Perl source code (use of this should be
avoided, it may be undefined)

=item PERM_RW

Desired permission for read/writable files. Defaults to C<644>.
See also L<MM_Unix/perm_rw>.

=item PERM_RWX

Desired permission for executable files. Defaults to C<755>.
See also L<MM_Unix/perm_rwx>.

=item PL_FILES

Ref to hash of files to be processed as perl programs. MakeMaker
will default to any found *.PL file (except Makefile.PL) being keys
and the basename of the file being the value. E.g.

  {'foobar.PL' => 'foobar'}

The *.PL files are expected to produce output to the target files
themselves. If multiple files can be generated from the same *.PL
file then the value in the hash can be a reference to an array of
target file names. E.g.

  {'foobar.PL' => ['foobar1','foobar2']}

=item PM

Hashref of .pm files and *.pl files to be installed.  e.g.

  {'name_of_file.pm' => '$(INST_LIBDIR)/install_as.pm'}

By default this will include *.pm and *.pl and the files found in
the PMLIBDIRS directories.  Defining PM in the
Makefile.PL will override PMLIBDIRS.

=item PMLIBDIRS

Ref to array of subdirectories containing library files.  Defaults to
[ 'lib', $(BASEEXT) ]. The directories will be scanned and I<any> files
they contain will be installed in the corresponding location in the
library.  A libscan() method can be used to alter the behaviour.
Defining PM in the Makefile.PL will override PMLIBDIRS.

(Where BASEEXT is the last component of NAME.)

=item PM_FILTER

A filter program, in the traditional Unix sense (input from stdin, output
to stdout) that is passed on each .pm file during the build (in the
pm_to_blib() phase).  It is empty by default, meaning no filtering is done.

Great care is necessary when defining the command if quoting needs to be
done.  For instance, you would need to say:

  {'PM_FILTER' => 'grep -v \\"^\\#\\"'}

to remove all the leading coments on the fly during the build.  The
extra \\ are necessary, unfortunately, because this variable is interpolated
within the context of a Perl program built on the command line, and double
quotes are what is used with the -e switch to build that command line.  The
# is escaped for the Makefile, since what is going to be generated will then
be:

  PM_FILTER = grep -v \"^\#\"

Without the \\ before the #, we'd have the start of a Makefile comment,
and the macro would be incorrectly defined.

=item POLLUTE

Release 5.005 grandfathered old global symbol names by providing preprocessor
macros for extension source compatibility.  As of release 5.6, these
preprocessor definitions are not available by default.  The POLLUTE flag
specifies that the old names should still be defined:

  perl Makefile.PL POLLUTE=1

Please inform the module author if this is necessary to successfully install
a module under 5.6 or later.

=item PPM_INSTALL_EXEC

Name of the executable used to run C<PPM_INSTALL_SCRIPT> below. (e.g. perl)

=item PPM_INSTALL_SCRIPT

Name of the script that gets executed by the Perl Package Manager after
the installation of a package.

=item PREFIX

Can be used to set the three INSTALL* attributes in one go (except for
probably INSTALLMAN1DIR, if it is not below PREFIX according to
%Config).  They will have PREFIX as a common directory node and will
branch from that node into lib/, lib/ARCHNAME or whatever Configure
decided at the build time of your perl (unless you override one of
them, of course).

=item PREREQ_PM

Hashref: Names of modules that need to be available to run this
extension (e.g. Fcntl for SDBM_File) are the keys of the hash and the
desired version is the value. If the required version number is 0, we
only check if any version is installed already.

=item SKIP

Arryref. E.g. [qw(name1 name2)] skip (do not write) sections of the
Makefile. Caution! Do not use the SKIP attribute for the negligible
speedup. It may seriously damage the resulting Makefile. Only use it
if you really need it.

=item TYPEMAPS

Ref to array of typemap file names.  Use this when the typemaps are
in some directory other than the current directory or when they are
not named B<typemap>.  The last typemap in the list takes
precedence.  A typemap in the current directory has highest
precedence, even if it isn't listed in TYPEMAPS.  The default system
typemap has lowest precedence.

=item VERSION

Your version number for distributing the package.  This defaults to
0.1.

=item VERSION_FROM

Instead of specifying the VERSION in the Makefile.PL you can let
MakeMaker parse a file to determine the version number. The parsing
routine requires that the file named by VERSION_FROM contains one
single line to compute the version number. The first line in the file
that contains the regular expression

    /([\$*])(([\w\:\']*)\bVERSION)\b.*\=/

will be evaluated with eval() and the value of the named variable
B<after> the eval() will be assigned to the VERSION attribute of the
MakeMaker object. The following lines will be parsed o.k.:

    $VERSION = '1.00';
    *VERSION = \'1.01';
    ( $VERSION ) = '$Revision: 1.222 $ ' =~ /\$Revision:\s+([^\s]+)/;
    $FOO::VERSION = '1.10';
    *FOO::VERSION = \'1.11';
    our $VERSION = 1.2.3;	# new for perl5.6.0 

but these will fail:

    my $VERSION = '1.01';
    local $VERSION = '1.02';
    local $FOO::VERSION = '1.30';

(Putting C<my> or C<local> on the preceding line will work o.k.)

The file named in VERSION_FROM is not added as a dependency to
Makefile. This is not really correct, but it would be a major pain
during development to have to rewrite the Makefile for any smallish
change in that file. If you want to make sure that the Makefile
contains the correct VERSION macro after any change of the file, you
would have to do something like

    depend => { Makefile => '$(VERSION_FROM)' }

See attribute C<depend> below.

=item XS

Hashref of .xs files. MakeMaker will default this.  e.g.

  {'name_of_file.xs' => 'name_of_file.c'}

The .c files will automatically be included in the list of files
deleted by a make clean.

=item XSOPT

String of options to pass to xsubpp.  This might include C<-C++> or
C<-extern>.  Do not include typemaps here; the TYPEMAP parameter exists for
that purpose.

=item XSPROTOARG

May be set to an empty string, which is identical to C<-prototypes>, or
C<-noprototypes>. See the xsubpp documentation for details. MakeMaker
defaults to the empty string.

=item XS_VERSION

Your version number for the .xs file of this package.  This defaults
to the value of the VERSION attribute.

=back

=head2 Additional lowercase attributes

can be used to pass parameters to the methods which implement that
part of the Makefile.

=over 2

=item clean

  {FILES => "*.xyz foo"}

=item depend

  {ANY_TARGET => ANY_DEPENDECY, ...}

(ANY_TARGET must not be given a double-colon rule by MakeMaker.)

=item dist

  {TARFLAGS => 'cvfF', COMPRESS => 'gzip', SUFFIX => '.gz',
  SHAR => 'shar -m', DIST_CP => 'ln', ZIP => '/bin/zip',
  ZIPFLAGS => '-rl', DIST_DEFAULT => 'private tardist' }

If you specify COMPRESS, then SUFFIX should also be altered, as it is
needed to tell make the target file of the compression. Setting
DIST_CP to ln can be useful, if you need to preserve the timestamps on
your files. DIST_CP can take the values 'cp', which copies the file,
'ln', which links the file, and 'best' which copies symbolic links and
links the rest. Default is 'best'.

=item dynamic_lib

  {ARMAYBE => 'ar', OTHERLDFLAGS => '...', INST_DYNAMIC_DEP => '...'}

=item linkext

  {LINKTYPE => 'static', 'dynamic' or ''}

NB: Extensions that have nothing but *.pm files had to say

  {LINKTYPE => ''}

with Pre-5.0 MakeMakers. Since version 5.00 of MakeMaker such a line
can be deleted safely. MakeMaker recognizes when there's nothing to
be linked.

=item macro

  {ANY_MACRO => ANY_VALUE, ...}

=item realclean

  {FILES => '$(INST_ARCHAUTODIR)/*.xyz'}

=item test

  {TESTS => 't/*.t'}

=item tool_autosplit

  {MAXLEN => 8}

=back

=head2 Overriding MakeMaker Methods

If you cannot achieve the desired Makefile behaviour by specifying
attributes you may define private subroutines in the Makefile.PL.
Each subroutines returns the text it wishes to have written to
the Makefile. To override a section of the Makefile you can
either say:

	sub MY::c_o { "new literal text" }

or you can edit the default by saying something like:

	sub MY::c_o {
	    package MY;	# so that "SUPER" works right
	    my $inherited = shift->SUPER::c_o(@_);
	    $inherited =~ s/old text/new text/;
	    $inherited;
	}

If you are running experiments with embedding perl as a library into
other applications, you might find MakeMaker is not sufficient. You'd
better have a look at ExtUtils::Embed which is a collection of utilities
for embedding.

If you still need a different solution, try to develop another
subroutine that fits your needs and submit the diffs to
F<perl5-porters@perl.org> or F<comp.lang.perl.moderated> as appropriate.

For a complete description of all MakeMaker methods see L<ExtUtils::MM_Unix>.

Here is a simple example of how to add a new target to the generated
Makefile:

    sub MY::postamble {
	'
    $(MYEXTLIB): sdbm/Makefile
	    cd sdbm && $(MAKE) all
    ';
    }


=head2 Hintsfile support

MakeMaker.pm uses the architecture specific information from
Config.pm. In addition it evaluates architecture specific hints files
in a C<hints/> directory. The hints files are expected to be named
like their counterparts in C<PERL_SRC/hints>, but with an C<.pl> file
name extension (eg. C<next_3_2.pl>). They are simply C<eval>ed by
MakeMaker within the WriteMakefile() subroutine, and can be used to
execute commands as well as to include special variables. The rules
which hintsfile is chosen are the same as in Configure.

The hintsfile is eval()ed immediately after the arguments given to
WriteMakefile are stuffed into a hash reference $self but before this
reference becomes blessed. So if you want to do the equivalent to
override or create an attribute you would say something like

    $self->{LIBS} = ['-ldbm -lucb -lc'];

=head2 Distribution Support

For authors of extensions MakeMaker provides several Makefile
targets. Most of the support comes from the ExtUtils::Manifest module,
where additional documentation can be found.

=over 4

=item    make distcheck

reports which files are below the build directory but not in the
MANIFEST file and vice versa. (See ExtUtils::Manifest::fullcheck() for
details)

=item    make skipcheck

reports which files are skipped due to the entries in the
C<MANIFEST.SKIP> file (See ExtUtils::Manifest::skipcheck() for
details)

=item    make distclean

does a realclean first and then the distcheck. Note that this is not
needed to build a new distribution as long as you are sure that the
MANIFEST file is ok.

=item    make manifest

rewrites the MANIFEST file, adding all remaining files found (See
ExtUtils::Manifest::mkmanifest() for details)

=item    make distdir

Copies all the files that are in the MANIFEST file to a newly created
directory with the name C<$(DISTNAME)-$(VERSION)>. If that directory
exists, it will be removed first.

=item	make disttest

Makes a distdir first, and runs a C<perl Makefile.PL>, a make, and
a make test in that directory.

=item    make tardist

First does a distdir. Then a command $(PREOP) which defaults to a null
command, followed by $(TOUNIX), which defaults to a null command under
UNIX, and will convert files in distribution directory to UNIX format
otherwise. Next it runs C<tar> on that directory into a tarfile and
deletes the directory. Finishes with a command $(POSTOP) which
defaults to a null command.

=item    make dist

Defaults to $(DIST_DEFAULT) which in turn defaults to tardist.

=item    make uutardist

Runs a tardist first and uuencodes the tarfile.

=item    make shdist

First does a distdir. Then a command $(PREOP) which defaults to a null
command. Next it runs C<shar> on that directory into a sharfile and
deletes the intermediate directory again. Finishes with a command
$(POSTOP) which defaults to a null command.  Note: For shdist to work
properly a C<shar> program that can handle directories is mandatory.

=item    make zipdist

First does a distdir. Then a command $(PREOP) which defaults to a null
command. Runs C<$(ZIP) $(ZIPFLAGS)> on that directory into a
zipfile. Then deletes that directory. Finishes with a command
$(POSTOP) which defaults to a null command.

=item    make ci

Does a $(CI) and a $(RCS_LABEL) on all files in the MANIFEST file.

=back

Customization of the dist targets can be done by specifying a hash
reference to the dist attribute of the WriteMakefile call. The
following parameters are recognized:

    CI           ('ci -u')
    COMPRESS     ('gzip --best')
    POSTOP       ('@ :')
    PREOP        ('@ :')
    TO_UNIX      (depends on the system)
    RCS_LABEL    ('rcs -q -Nv$(VERSION_SYM):')
    SHAR         ('shar')
    SUFFIX       ('.gz')
    TAR          ('tar')
    TARFLAGS     ('cvf')
    ZIP          ('zip')
    ZIPFLAGS     ('-r')

An example:

    WriteMakefile( 'dist' => { COMPRESS=>"bzip2", SUFFIX=>".bz2" })

=head2 Disabling an extension

If some events detected in F<Makefile.PL> imply that there is no way
to create the Module, but this is a normal state of things, then you
can create a F<Makefile> which does nothing, but succeeds on all the
"usual" build targets.  To do so, use

   ExtUtils::MakeMaker::WriteEmptyMakefile();

instead of WriteMakefile().

This may be useful if other modules expect this module to be I<built>
OK, as opposed to I<work> OK (say, this system-dependent module builds
in a subdirectory of some other distribution, or is listed as a
dependency in a CPAN::Bundle, but the functionality is supported by
different means on the current architecture).

=head1 ENVIRONMENT

=over 8

=item PERL_MM_OPT

Command line options used by C<MakeMaker-E<gt>new()>, and thus by
C<WriteMakefile()>.  The string is split on whitespace, and the result
is processed before any actual command line arguments are processed.

=back

=head1 SEE ALSO

ExtUtils::MM_Unix, ExtUtils::Manifest, ExtUtils::testlib,
ExtUtils::Install, ExtUtils::Embed

=head1 AUTHORS

Andy Dougherty <F<doughera@lafcol.lafayette.edu>>, Andreas KE<ouml>nig
<F<A.Koenig@franz.ww.TU-Berlin.DE>>, Tim Bunce <F<Tim.Bunce@ig.co.uk>>.
VMS support by Charles Bailey <F<bailey@newman.upenn.edu>>.  OS/2
support by Ilya Zakharevich <F<ilya@math.ohio-state.edu>>.  Contact the
makemaker mailing list C<mailto:makemaker@franz.ww.tu-berlin.de>, if
you have any questions.

=cut

package ExtUtils::MM_OS2;

#use Config;
#use Cwd;
#use File::Basename;
require Exporter;

Exporter::import('ExtUtils::MakeMaker',
       qw( $Verbose &neatvalue));

unshift @MM::ISA, 'ExtUtils::MM_OS2';

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
     '	$(PERL) "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" -e \'use ExtUtils::Mksymlists; \\
     Mksymlists("NAME" => "$(NAME)", "DLBASE" => "$(DLBASE)", ',
     '"VERSION" => "$(VERSION)", "DISTNAME" => "$(DISTNAME)", ',
     '"INSTALLDIRS" => "$(INSTALLDIRS)", ',
     '"DL_FUNCS" => ',neatvalue($funcs),
     ', "FUNCLIST" => ',neatvalue($funclist),
     ', "IMPORTS" => ',neatvalue($imports),
     ', "DL_VARS" => ', neatvalue($vars), ');\'
');
    }
    if (%{$self->{IMPORTS}}) {
	# Make import files (needed for static build)
	-d 'tmp_imp' or mkdir 'tmp_imp', 0777 or die "Can't mkdir tmp_imp";
	open IMP, '>tmpimp.imp' or die "Can't open tmpimp.imp";
	my ($name, $exp);
	while (($name, $exp)= each %{$self->{IMPORTS}}) {
	    my ($lib, $id) = ($exp =~ /(.*)\.(.*)/) or die "Malformed IMPORT `$exp'";
	    print IMP "$name $lib $id ?\n";
	}
	close IMP or die "Can't close tmpimp.imp";
	# print "emximp -o tmpimp$Config::Config{lib_ext} tmpimp.imp\n";
	system "emximp -o tmpimp$Config::Config{lib_ext} tmpimp.imp" 
	    and die "Cannot make import library: $!, \$?=$?";
	unlink <tmp_imp/*>;
	system "cd tmp_imp; $Config::Config{ar} x ../tmpimp$Config::Config{lib_ext}" 
	    and die "Cannot extract import objects: $!, \$?=$?";      
    }
    join('',@m);
}

sub static_lib {
    my($self) = @_;
    my $old = $self->ExtUtils::MM_Unix::static_lib();
    return $old unless %{$self->{IMPORTS}};
    
    my @chunks = split /\n{2,}/, $old;
    shift @chunks unless length $chunks[0]; # Empty lines at the start
    $chunks[0] .= <<'EOC';

	$(AR) $(AR_STATIC_ARGS) $@ tmp_imp/* && $(RANLIB) $@
EOC
    return join "\n\n". '', @chunks;
}

sub replace_manpage_separator {
    my($self,$man) = @_;
    $man =~ s,/+,.,g;
    $man;
}

sub maybe_command {
    my($self,$file) = @_;
    $file =~ s,[/\\]+,/,g;
    return $file if -x $file && ! -d _;
    return "$file.exe" if -x "$file.exe" && ! -d _;
    return "$file.cmd" if -x "$file.cmd" && ! -d _;
    return;
}

sub file_name_is_absolute {
    my($self,$file) = @_;
    $file =~ m{^([a-z]:)?[\\/]}i ;
}

sub perl_archive
{
 return "\$(PERL_INC)/libperl\$(LIB_EXT)";
}

=item perl_archive_after

This is an internal method that returns path to a library which
should be put on the linker command line I<after> the external libraries
to be linked to dynamic extensions.  This may be needed if the linker
is one-pass, and Perl includes some overrides for C RTL functions,
such as malloc().

=cut 

sub perl_archive_after
{
 return "\$(PERL_INC)/libperl_override\$(LIB_EXT)" unless $OS2::is_aout;
 return "";
}

sub export_list
{
 my ($self) = @_;
 return "$self->{BASEEXT}.def";
}

1;
__END__

=head1 NAME

ExtUtils::MM_OS2 - methods to override UN*X behaviour in ExtUtils::MakeMaker

=head1 SYNOPSIS

 use ExtUtils::MM_OS2; # Done internally by ExtUtils::MakeMaker if needed

=head1 DESCRIPTION

See ExtUtils::MM_Unix for a documentation of the methods provided
there. This package overrides the implementation of these methods, not
the semantics.


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
    my($imports)  = $attribs{IMPORTS} || $self->{IMPORTS} || {};
    my(@m);
    (my $boot = $self->{NAME}) =~ s/:/_/g;

    if (not $self->{SKIPHASH}{'dynamic'}) {
	push(@m,"
$self->{BASEEXT}.def: Makefile.PL
",
     '	$(PERL) "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" -e \'use ExtUtils::Mksymlists; \\
     Mksymlists("NAME" => "', $self->{NAME},
     '", "DLBASE" => "',$self->{DLBASE},
     '", "DL_FUNCS" => ',neatvalue($funcs),
     ', "IMPORTS" => ',neatvalue($imports),
     ', "VERSION" => "',$self->{VERSION},
     '", "DL_VARS" => ', neatvalue($vars), ');\'
');
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


package ExtUtils::MM_Cygwin;

use Config;
#use Cwd;
#use File::Basename;
require Exporter;

Exporter::import('ExtUtils::MakeMaker',
       qw( $Verbose &neatvalue));

unshift @MM::ISA, 'ExtUtils::MM_Cygwin';

sub canonpath {
    my($self,$path) = @_;
    $path =~ s|\\|/|g;
    return $self->ExtUtils::MM_Unix::canonpath($path);
}

sub cflags {
    my($self,$libperl)=@_;
    return $self->{CFLAGS} if $self->{CFLAGS};
    my $base =$self->ExtUtils::MM_Unix::cflags($libperl);
    foreach (split /\n/, $base) {
      / *= */ and $self->{$`} = $';
    };
    $self->{CCFLAGS} .= " -DUSEIMPORTLIB" if ($Config{useshrplib} eq 'true');

    return $self->{CFLAGS} = qq{
CCFLAGS = $self->{CCFLAGS}
OPTIMIZE = $self->{OPTIMIZE}
PERLTYPE = $self->{PERLTYPE}
LARGE = $self->{LARGE}
SPLIT = $self->{SPLIT}
};

}

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
-e 'print "Manifying $$m{$$_}\n"; $$m{$$_} =~ s/::/./g;' \\
-e 'system(qq[$$^X ].q["-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" $(POD2MAN_EXE) ].qq[$$_>$$m{$$_}])==0 or warn "Couldn\\047t install $$m{$$_}\n";' \\
-e 'chmod(oct($(PERM_RW))), $$m{$$_} or warn "chmod $(PERM_RW) $$m{$$_}: $$!\n";}'
];
    push @m, "\nmanifypods : pure_all ";
    push @m, join " \\\n\t", keys %{$self->{MAN1PODS}}, keys %{$self->{MAN3PODS}};

    push(@m,"\n");
    if (%{$self->{MAN1PODS}} || %{$self->{MAN3PODS}}) {
        grep { $self->{MAN1PODS}{$_} =~ s/::/./g } keys %{$self->{MAN1PODS}};
        grep { $self->{MAN3PODS}{$_} =~ s/::/./g } keys %{$self->{MAN3PODS}};
        push @m, "\t$self->{NOECHO}\$(POD2MAN) \\\n\t";
        push @m, join " \\\n\t", %{$self->{MAN1PODS}}, %{$self->{MAN3PODS}};
    }
    join('', @m);
}

sub perl_archive
{
 return '$(PERL_INC)' .'/'. ("$Config{libperl}" or "libperl.a");
}

1;
__END__

=head1 NAME

ExtUtils::MM_Cygwin - methods to override UN*X behaviour in ExtUtils::MakeMaker

=head1 SYNOPSIS

 use ExtUtils::MM_Cygwin; # Done internally by ExtUtils::MakeMaker if needed

=head1 DESCRIPTION

See ExtUtils::MM_Unix for a documentation of the methods provided there.

=over

=item canonpath

replaces backslashes with forward ones.  then acts as *nixish.

=item cflags

if configured for dynamic loading, triggers #define EXT in EXTERN.h

=item manifypods

replaces strings '::' with '.' in man page names

=item perl_archive

points to libperl.a

=back

=cut


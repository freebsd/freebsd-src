use ExtUtils::MakeMaker;
use Config;

my $e = $Config{'exe_ext'};
my $o = $Config{'obj_ext'};
my $exeout_flag = '-o ';
if ($^O eq 'MSWin32') {
    if ($Config{'cc'} =~ /^cl/i) {
	$exeout_flag = '-Fe';
    }
    elsif ($Config{'cc'} =~ /^bcc/i) {
	$exeout_flag = '-e';
    }
}

WriteMakefile(
    NAME	=> "B",
    VERSION	=> "a5",
    MAN3PODS => {},
    clean	=> {
	FILES		=> "perl$e byteperl$e *$o B.c *~"
    }
);

sub MY::post_constants {
    "\nLIBS = $Config{libs}\n"
}

# Leave out doing byteperl for now. Probably should be built in the
# core directory or somewhere else rather than here
#sub MY::top_targets {
#    my $self = shift;
#    my $targets = $self->MM::top_targets();
#    $targets =~ s/^(all ::.*)$/$1 byteperl$e/m;
#    return <<"EOT" . $targets;

#
# byteperl is *not* a standard perl+XSUB executable. It's a special
# program for running standalone bytecode executables. It isn't an XSUB
# at the moment because a standlone Perl program needs to set up curpad
# which is overwritten on exit from an XSUB.
#
#byteperl$e : byteperl$o B$o \$(PERL_SRC)/byterun$o
#	\$(CC) ${exeout_flag}byteperl$e byteperl$o B$o byterun$o \$(LDFLAGS) \$(PERL_ARCHLIB)/CORE/$Config{libperl} \$(LIBS)
#EOT
#}

use ExtUtils::MakeMaker;

$define = '-DSDBM -DDUFF';
$define .= ' -DWIN32 -DPERL_STATIC_SYMS' if ($^O eq 'MSWin32');

if ($^O eq 'VMS') {  # Old VAXC compiler can't handle Duff's device
    require Config;
    $define =~ s/\s+-DDUFF// if $Config::Config{'vms_cc_type'} eq 'vaxc';
}

WriteMakefile(
    NAME      => 'sdbm', # (doesn't matter what the name is here) oh yes it does
#    LINKTYPE  => 'static',
    DEFINE    => $define,
    INC       => '-I$(PERL_INC)', # force PERL_INC dir ahead of system -I's
    INST_ARCHLIB => '.',
    SKIP      => [qw(dynamic dynamic_lib dlsyms)],
    OBJECT    => '$(O_FILES)',
    clean     => {'FILES' => 'dbu libsdbm.a dbd dba dbe x-dbu *.dir *.pag'},
    H         => [qw(tune.h sdbm.h pair.h $(PERL_INC)/config.h)],
    C         => [qw(sdbm.c pair.c hash.c)]
);

sub MY::constants {
  package MY;
  my $r = shift->SUPER::constants();
  if ($^O eq 'VMS') {
    $r =~ s/^INST_STATIC =.*$/INST_STATIC = libsdbm\$(LIB_EXT)/m
  }
  return $r;
}

sub MY::post_constants {
  package MY;
  if ($^O eq 'VMS') {
    shift->SUPER::post_constants();
  } else {
'
INST_STATIC = libsdbm$(LIB_EXT)
'
  }
}

sub MY::top_targets {
    my $noecho = shift->{NOECHO};
    
    my $r = '
all :: static
	' . $noecho . '$(NOOP)

config ::
	' . $noecho . '$(NOOP)

lint:
	lint -abchx $(LIBSRCS)

';
    $r .= '
# This is a workaround, the problem is that our old GNU make exports
# variables into the environment so $(MYEXTLIB) is set in here to this
# value which can not be built.
sdbm/libsdbm.a:
	' . $noecho . '$(NOOP)
' unless $^O eq 'VMS';

    return $r;
}

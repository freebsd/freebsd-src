use ExtUtils::MakeMaker;
WriteMakefile(
    NAME	=> 'POSIX',
    ($^O eq 'MSWin32' ? () : (LIBS	=> ["-lm -lposix -lcposix"])),
    MAN3PODS 	=> {}, 	# Pods will be built by installman.
    XSPROTOARG => '-noprototypes', 		# XXX remove later?
    VERSION_FROM => 'POSIX.pm', 
);

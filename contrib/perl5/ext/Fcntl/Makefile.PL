use ExtUtils::MakeMaker;
WriteMakefile(
    NAME 	=> 'Fcntl',
    MAN3PODS 	=> {}, 	# Pods will be built by installman.
    XSPROTOARG => '-noprototypes', 		# XXX remove later?
    VERSION_FROM => 'Fcntl.pm',
);


use ExtUtils::MakeMaker;
WriteMakefile(
    NAME	=> 'Socket',
    VERSION_FROM => 'Socket.pm',
    MAN3PODS 	=> {}, 	# Pods will be built by installman.
    XSPROTOARG => '-noprototypes', 		# XXX remove later?
);

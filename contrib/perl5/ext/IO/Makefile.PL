use ExtUtils::MakeMaker;
WriteMakefile(
    NAME => 'IO',
    MAN3PODS   => {},			# Pods will be built by installman.
    XSPROTOARG => '-noprototypes', 	# XXX remove later?
    VERSION_FROM => 'lib/IO/Handle.pm',
    XS_VERSION => 1.15
);

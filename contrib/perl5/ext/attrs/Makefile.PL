use ExtUtils::MakeMaker;
WriteMakefile(
    NAME	=> 'attrs',
    VERSION_FROM => 'attrs.pm',
    MAN3PODS 	=> {}, 	# Pods will be built by installman.
    XSPROTOARG => '-noprototypes'
);

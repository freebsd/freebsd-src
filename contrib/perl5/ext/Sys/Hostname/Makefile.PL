use ExtUtils::MakeMaker;

WriteMakefile(
    NAME                => 'Sys::Hostname',
    VERSION_FROM        => 'Hostname.pm', 
    MAN3PODS		=> {}, 	# Pods will be built by installman.
    XSPROTOARG          => '-noprototypes',
);

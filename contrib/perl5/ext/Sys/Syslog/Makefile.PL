use ExtUtils::MakeMaker;

WriteMakefile(
    NAME		=> 'Sys::Syslog',
    VERSION_FROM	=> 'Syslog.pm', 
    MAN3PODS		=> {}, 	# Pods will be built by installman.
    XSPROTOARG          => '-noprototypes',
);

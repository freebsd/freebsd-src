use ExtUtils::MakeMaker;
WriteMakefile(
    NAME	=> 'ODBM_File',
    LIBS => ["-ldbm -lucb"],
    MAN3PODS 	=> {}, 	# Pods will be built by installman.
    XSPROTOARG => '-noprototypes', 		# XXX remove later?
    VERSION_FROM => 'ODBM_File.pm', 
);

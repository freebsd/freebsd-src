use ExtUtils::MakeMaker;
WriteMakefile(
    NAME	=> 'NDBM_File',
    LIBS => ["-L/usr/local/lib -lndbm", "-ldbm -lucb"],
    MAN3PODS 	=> {}, 	# Pods will be built by installman.
    XSPROTOARG => '-noprototypes', 		# XXX remove later?
    VERSION_FROM => 'NDBM_File.pm',
    INC => ($^O eq "MacOS" ? "-i ::::db:include" : "")
);

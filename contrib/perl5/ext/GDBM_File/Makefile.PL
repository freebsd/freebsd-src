use ExtUtils::MakeMaker;
WriteMakefile(
    NAME => 'GDBM_File',
    LIBS => ["-L/usr/local/lib -lgdbm", "-ldbm"],
    MAN3PODS 	=> {}, 	# Pods will be built by installman.
    XSPROTOARG => '-noprototypes', 		# XXX remove later?
    VERSION_FROM => 'GDBM_File.pm',
);

use ExtUtils::MakeMaker;
WriteMakefile(
	NAME		=> "Devel::Peek",
	VERSION_FROM	=> 'Peek.pm',
        XSPROTOARG      => '-noprototypes',
	'dist'		=> {
			     COMPRESS	=> 'gzip -9f',
	      		     SUFFIX	=> 'gz',
	      		     DIST_DEFAULT => 'all tardist',
			   },
	MAN3PODS	=> {},
);

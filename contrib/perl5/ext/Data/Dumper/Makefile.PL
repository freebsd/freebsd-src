use ExtUtils::MakeMaker;
WriteMakefile(
	NAME		=> "Data::Dumper",
	VERSION_FROM	=> 'Dumper.pm',
	'dist'		=> {
			     COMPRESS	=> 'gzip -9f',
	      		     SUFFIX	=> 'gz',
	      		     DIST_DEFAULT => 'all tardist',
			   },
	MAN3PODS	=> {},
);

use ExtUtils::MakeMaker;
use Config qw(%Config);

WriteMakefile(
	VERSION_FROM	=> "IO.pm",
	NAME      	=> "IO",
	OBJECT		=> '$(O_FILES)', 
	MAN3PODS	=> {},		# Pods will be built by installman.
);

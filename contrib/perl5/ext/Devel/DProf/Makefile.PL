use ExtUtils::MakeMaker;

WriteMakefile(
	NAME		=> 'Devel::DProf',
	DISTNAME	=> 'DProf',
	VERSION_FROM	=> 'DProf.pm',
	clean		=> { 'FILES' => 'tmon.out t/tmon.out t/err'},
	XSPROTOARG	=> '-noprototypes',
	DEFINE		=> '-DPERLDBf_NONAME=0x40 -DPERLDBf_GOTO=0x80 '
			  .'-DG_NODEBUG=32 -DPL_NEEDED',
	dist		=> {
			     COMPRESS => 'gzip -9f',
			     SUFFIX => 'gz',
			     DIST_DEFAULT => 'all tardist',
			   },
	MAN3PODS	=> {},
);

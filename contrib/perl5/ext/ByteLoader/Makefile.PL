use ExtUtils::MakeMaker;

WriteMakefile(
    NAME		=> 'ByteLoader',
    VERSION_FROM	=> 'ByteLoader.pm',
    XSPROTOARG		=> '-noprototypes',
    MAN3PODS		=> {},     # Pods will be built by installman.
    OBJECT		=> 'byterun$(OBJ_EXT) ByteLoader$(OBJ_EXT)',
);

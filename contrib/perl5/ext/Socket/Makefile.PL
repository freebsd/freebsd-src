use ExtUtils::MakeMaker;
use Config;
WriteMakefile(
    NAME	 => 'Socket',
    VERSION_FROM => 'Socket.pm',
   ($Config{libs} =~ /(-lsocks\S*)/ ? (LIBS => [ "$1" ] ) : ()),
    MAN3PODS  	 => {}, 	# Pods will be built by installman.
    XSPROTOARG   => '-noprototypes', 		# XXX remove later?
);

# $FreeBSD$
use ExtUtils::MakeMaker;
use Config;
my @libs;
if ($^O ne 'MSWin32') {
    @libs = ('LIBS' => ["-lm -lposix -lcposix"]);
}
WriteMakefile(
    NAME	=> 'POSIX',
    @libs,
    MAN3PODS 	=> {}, 	# Pods will be built by installman.
    XSPROTOARG => '-noprototypes', 		# XXX remove later?
    VERSION_FROM => 'POSIX.pm', 
);

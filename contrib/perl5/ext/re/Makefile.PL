use ExtUtils::MakeMaker;
use File::Spec;

WriteMakefile(
    NAME		=> 're',
    VERSION_FROM	=> 're.pm',
    MAN3PODS		=> {}, 	# Pods will be built by installman.
    XSPROTOARG		=> '-noprototypes',
    OBJECT		=> 're_exec$(OBJ_EXT) re_comp$(OBJ_EXT) re$(OBJ_EXT)',
    DEFINE		=> '-DPERL_EXT_RE_BUILD -DPERL_EXT_RE_DEBUG',
    clean		=> { FILES => '*$(OBJ_EXT) *.c ../../lib/re.pm' },
);

package MY;

sub upupfile {
    File::Spec->catfile(File::Spec->updir, File::Spec->updir, $_[0]);
}

sub postamble {
    my $regcomp_c = upupfile('regcomp.c');
    my $regexec_c = upupfile('regexec.c');

    <<EOF;
re_comp.c : $regcomp_c
	- \$(RM_F) re_comp.c
	\$(CP) $regcomp_c re_comp.c

re_comp\$(OBJ_EXT) : re_comp.c

re_exec.c : $regexec_c
	- \$(RM_F) re_exec.c
	\$(CP) $regexec_c re_exec.c

re_exec\$(OBJ_EXT) : re_exec.c

EOF
}

use ExtUtils::MakeMaker;
WriteMakefile(
    NAME		=> 're',
    VERSION_FROM	=> 're.pm',
    MAN3PODS		=> {}, 	# Pods will be built by installman.
    XSPROTOARG		=> '-noprototypes',
    OBJECT		=> 're_exec$(OBJ_EXT) re_comp$(OBJ_EXT) re$(OBJ_EXT)',
    DEFINE		=> '-DPERL_EXT_RE_BUILD',
    clean		=> { FILES => '*$(OBJ_EXT) *.c ../../lib/re.pm' },
);

sub MY::postamble {
  if ($^O eq 'VMS') {
    return <<'VMS_EOF';
re_comp.c : [--]regcomp.c
	- $(RM_F) $(MMS$TARGET_NAME)
	$(CP) [--]regcomp.c $(MMS$TARGET_NAME)

re_comp$(OBJ_EXT) : re_comp.c

re_exec.c : [--]regexec.c
	- $(RM_F) $(MMS$TARGET_NAME)
	$(CP) [--]regexec.c $(MMS$TARGET_NAME)

re_exec$(OBJ_EXT) : re_exec.c


VMS_EOF
  } else {
    return <<'EOF';
re_comp.c: ../../regcomp.c
	-$(RM_F) $@
	$(CP) ../../regcomp.c $@

re_exec.c: ../../regexec.c
	-$(RM_F) $@
	$(CP) ../../regexec.c $@

EOF
  }
}

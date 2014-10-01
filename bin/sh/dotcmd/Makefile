# $NetBSD: Makefile,v 1.2 2014/07/27 14:24:17 apb Exp $
#

.include <bsd.own.mk>

TESTSDIR = ${TESTSBASE}/bin/sh/dotcmd

TESTS_SH = t_dotcmd

FILESDIR = ${TESTSDIR}/out

# Testing scripts: dotcmd in various scopes includes a file with
# return / break / continue in various scopes.
#
.for cmd_scope in case compound file for func subshell until while
. for cmd in return break continue
FILES += ${cmd}_${cmd_scope}
FILESDIR_${cmd}_${cmd_scope} = ${TESTSDIR}
FILESBUILD_${cmd}_${cmd_scope} = yes

${cmd}_${cmd_scope}: scoped_command
	${HOST_SH} ${.CURDIR}/scoped_command '${cmd_scope}' '${cmd}' '${cmd}' \
		>'${.TARGET}'

.  for dot_scope in case compound file for func subshell until while
FILES += \
	${dot_scope}_${cmd}_${cmd_scope} \
	out/${dot_scope}_${cmd}_${cmd_scope}.out
FILESDIR_${dot_scope}_${cmd}_${cmd_scope} = ${TESTSDIR}
FILESBUILD_${dot_scope}_${cmd}_${cmd_scope} = yes
FILESMODE_${dot_scope}_${cmd}_${cmd_scope} = ${BINMODE}

${dot_scope}_${cmd}_${cmd_scope}: scoped_command
	${HOST_SH} ${.CURDIR}/scoped_command '${dot_scope}' \
		'. "${cmd}_${cmd_scope}"' 'dotcmd' 'dotcmd' >'${.TARGET}'
.  endfor
. endfor
.endfor

.include <bsd.test.mk>

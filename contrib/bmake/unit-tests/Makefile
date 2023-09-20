# $Id: Makefile,v 1.207 2023/09/09 16:44:03 sjg Exp $
#
# $NetBSD: Makefile,v 1.341 2023/09/09 16:41:04 sjg Exp $
#
# Unit tests for make(1)
#
# The main targets are:
#
# all:
#	run all the tests
# test:
#	run 'all', and compare to expected results
# accept:
#	move generated output to expected results
#
# Settable variables
#
# TEST_MAKE
#	The make program to be tested.
#
#
# Adding a test case
#
# Each feature should get its own set of tests in its own suitably
# named makefile (*.mk), with its own set of expected results (*.exp),
# and it should be added to the TESTS list.
#

.MAIN: all

# we use these below but we might be an older make
.MAKE.OS?= ${uname -s:L:sh}
.MAKE.UID?= ${id -u:L:sh}

# for many tests we need a TMPDIR that will not collide
# with other users.
.if ${.OBJDIR} != ${.CURDIR}
# easy
TMPDIR:=	${.OBJDIR}/tmp
.elif defined(TMPDIR)
TMPDIR:=	${TMPDIR}/uid${.MAKE.UID}
.else
TMPDIR:=	/tmp/uid${.MAKE.UID}
.endif
# make sure it exists
.if !exist(${TMPDIR})
_!= mkdir -p ${TMPDIR}
.endif

# Each test is in a sub-makefile.
# Keep the list sorted.
# Any test that is commented out must be ignored in
# src/tests/usr.bin/make/t_make.sh as well.
#TESTS+=		archive
#TESTS+=		archive-suffix
TESTS+=		cmd-errors
TESTS+=		cmd-errors-jobs
TESTS+=		cmd-errors-lint
TESTS+=		cmd-interrupt
TESTS+=		cmdline
TESTS+=		cmdline-redirect-stdin
TESTS+=		cmdline-undefined
TESTS+=		comment
TESTS+=		compat-error
TESTS+=		cond-cmp-numeric
TESTS+=		cond-cmp-numeric-eq
TESTS+=		cond-cmp-numeric-ge
TESTS+=		cond-cmp-numeric-gt
TESTS+=		cond-cmp-numeric-le
TESTS+=		cond-cmp-numeric-lt
TESTS+=		cond-cmp-numeric-ne
TESTS+=		cond-cmp-string
TESTS+=		cond-cmp-unary
TESTS+=		cond-eof
TESTS+=		cond-func
TESTS+=		cond-func-commands
TESTS+=		cond-func-defined
TESTS+=		cond-func-empty
TESTS+=		cond-func-exists
TESTS+=		cond-func-make
TESTS+=		cond-func-make-main
TESTS+=		cond-func-target
TESTS+=		cond-late
TESTS+=		cond-op
TESTS+=		cond-op-and
TESTS+=		cond-op-and-lint
TESTS+=		cond-op-not
TESTS+=		cond-op-or
TESTS+=		cond-op-or-lint
TESTS+=		cond-op-parentheses
TESTS+=		cond-short
TESTS+=		cond-token-number
TESTS+=		cond-token-plain
TESTS+=		cond-token-string
TESTS+=		cond-token-var
TESTS+=		cond-undef-lint
TESTS+=		counter
TESTS+=		counter-append
TESTS+=		dep
TESTS+=		dep-colon
TESTS+=		dep-colon-bug-cross-file
TESTS+=		dep-double-colon
TESTS+=		dep-double-colon-indep
TESTS+=		dep-duplicate
TESTS+=		dep-exclam
TESTS+=		dep-none
TESTS+=		dep-op-missing
TESTS+=		dep-percent
TESTS+=		dep-var
TESTS+=		dep-wildcards
TESTS+=		depsrc
TESTS+=		depsrc-end
TESTS+=		depsrc-exec
TESTS+=		depsrc-ignore
TESTS+=		depsrc-made
TESTS+=		depsrc-make
TESTS+=		depsrc-meta
TESTS+=		depsrc-nometa
TESTS+=		depsrc-nometa_cmp
TESTS+=		depsrc-nopath
TESTS+=		depsrc-notmain
TESTS+=		depsrc-optional
TESTS+=		depsrc-phony
TESTS+=		depsrc-precious
TESTS+=		depsrc-recursive
TESTS+=		depsrc-silent
TESTS+=		depsrc-use
TESTS+=		depsrc-usebefore
TESTS+=		depsrc-usebefore-double-colon
TESTS+=		depsrc-wait
TESTS+=		deptgt
TESTS+=		deptgt-begin
TESTS+=		deptgt-begin-fail
TESTS+=		deptgt-begin-fail-indirect
TESTS+=		deptgt-default
TESTS+=		deptgt-delete_on_error
TESTS+=		deptgt-end
TESTS+=		deptgt-end-fail
TESTS+=		deptgt-end-fail-all
TESTS+=		deptgt-end-fail-indirect
TESTS+=		deptgt-end-jobs
TESTS+=		deptgt-error
TESTS+=		deptgt-ignore
TESTS+=		deptgt-interrupt
TESTS+=		deptgt-main
TESTS+=		deptgt-makeflags
TESTS+=		deptgt-no_parallel
TESTS+=		deptgt-nopath
TESTS+=		deptgt-notparallel
TESTS+=		deptgt-objdir
TESTS+=		deptgt-order
TESTS+=		deptgt-path
TESTS+=		deptgt-path-suffix
TESTS+=		deptgt-phony
TESTS+=		deptgt-posix
TESTS+=		deptgt-precious
TESTS+=		deptgt-shell
TESTS+=		deptgt-silent
TESTS+=		deptgt-silent-jobs
TESTS+=		deptgt-stale
TESTS+=		deptgt-suffixes
TESTS+=		dir
TESTS+=		dir-expand-path
TESTS+=		directive
TESTS+=		directive-dinclude
TESTS+=		directive-elif
TESTS+=		directive-elifdef
TESTS+=		directive-elifmake
TESTS+=		directive-elifndef
TESTS+=		directive-elifnmake
TESTS+=		directive-else
TESTS+=		directive-endfor
TESTS+=		directive-endif
TESTS+=		directive-error
TESTS+=		directive-export
TESTS+=		directive-export-env
TESTS+=		directive-export-impl
TESTS+=		directive-export-gmake
TESTS+=		directive-export-literal
TESTS+=		directive-for
TESTS+=		directive-for-break
TESTS+=		directive-for-empty
TESTS+=		directive-for-errors
TESTS+=		directive-for-escape
TESTS+=		directive-for-generating-endif
TESTS+=		directive-for-if
TESTS+=		directive-for-lines
TESTS+=		directive-for-null
TESTS+=		directive-hyphen-include
TESTS+=		directive-if
TESTS+=		directive-if-nested
TESTS+=		directive-ifdef
TESTS+=		directive-ifmake
TESTS+=		directive-ifndef
TESTS+=		directive-ifnmake
TESTS+=		directive-include
TESTS+=		directive-include-fatal
TESTS+=		directive-include-guard
TESTS+=		directive-info
TESTS+=		directive-misspellings
TESTS+=		directive-sinclude
TESTS+=		directive-undef
TESTS+=		directive-unexport
TESTS+=		directive-unexport-env
TESTS+=		directive-warning
TESTS+=		dollar
TESTS+=		doterror
TESTS+=		dotwait
TESTS+=		error
TESTS+=		# escape	# broken by reverting POSIX changes
TESTS+=		export
TESTS+=		export-all
TESTS+=		export-env
TESTS+=		export-variants
TESTS+=		gnode-submake
TESTS+=		hanoi-include
TESTS+=		impsrc
TESTS+=		include-main
TESTS+=		job-flags
#TESTS+=		job-output-long-lines
TESTS+=		job-output-null
TESTS+=		jobs-empty-commands
TESTS+=		jobs-empty-commands-error
TESTS+=		jobs-error-indirect
TESTS+=		jobs-error-nested
TESTS+=		jobs-error-nested-make
TESTS+=		lint
TESTS+=		make-exported
TESTS+=		meta-cmd-cmp
TESTS+=		moderrs
TESTS+=		modmisc
.if ${.MAKE.UID} > 0
TESTS+=		objdir-writable
.endif
TESTS+=		opt
TESTS+=		opt-backwards
TESTS+=		opt-chdir
TESTS+=		opt-debug
TESTS+=		opt-debug-all
TESTS+=		opt-debug-archive
TESTS+=		opt-debug-curdir
TESTS+=		opt-debug-cond
TESTS+=		opt-debug-dir
TESTS+=		opt-debug-errors
TESTS+=		opt-debug-errors-jobs
TESTS+=		opt-debug-file
TESTS+=		opt-debug-for
TESTS+=		opt-debug-graph1
TESTS+=		opt-debug-graph2
TESTS+=		opt-debug-graph3
TESTS+=		opt-debug-hash
#TESTS+=		opt-debug-jobs
TESTS+=		opt-debug-lint
TESTS+=		opt-debug-loud
TESTS+=		opt-debug-meta
TESTS+=		opt-debug-making
TESTS+=		opt-debug-no-rm
TESTS+=		opt-debug-parse
TESTS+=		opt-debug-suff
TESTS+=		opt-debug-targets
TESTS+=		opt-debug-varraw
TESTS+=		opt-debug-var
TESTS+=		opt-debug-x-trace
TESTS+=		opt-define
TESTS+=		opt-env
TESTS+=		opt-file
TESTS+=		opt-ignore
TESTS+=		opt-include-dir
TESTS+=		opt-jobs
TESTS+=		opt-jobs-internal
TESTS+=		opt-jobs-no-action
TESTS+=		opt-keep-going
TESTS+=		opt-keep-going-indirect
TESTS+=		opt-keep-going-multiple
TESTS+=		opt-m-include-dir
TESTS+=		opt-no-action
TESTS+=		opt-no-action-at-all
TESTS+=		opt-no-action-runflags
TESTS+=		opt-no-action-touch
TESTS+=		opt-query
TESTS+=		opt-raw
TESTS+=		opt-silent
TESTS+=		opt-touch
TESTS+=		opt-touch-jobs
TESTS+=		opt-tracefile
TESTS+=		opt-var-expanded
TESTS+=		opt-var-literal
TESTS+=		opt-version
TESTS+=		opt-warnings-as-errors
TESTS+=		opt-where-am-i
TESTS+=		opt-x-reduce-exported
TESTS+=		order
TESTS+=		parse
TESTS+=		parse-var
TESTS+=		phony-end
TESTS+=		posix
TESTS+=		# posix1	# broken by reverting POSIX changes
TESTS+=		recursive
TESTS+=		sh
TESTS+=		sh-dots
TESTS+=		sh-errctl
TESTS+=		sh-flags
TESTS+=		sh-jobs
TESTS+=		sh-jobs-error
TESTS+=		sh-leading-at
TESTS+=		sh-leading-hyphen
TESTS+=		sh-leading-plus
TESTS+=		sh-meta-chars
TESTS+=		sh-multi-line
TESTS+=		sh-single-line
TESTS+=		shell-csh
TESTS+=		shell-custom
.if exists(/bin/ksh)
TESTS+=		shell-ksh
.endif
TESTS+=		shell-sh
TESTS+=		suff-add-later
TESTS+=		suff-clear-regular
TESTS+=		suff-clear-single
TESTS+=		suff-incomplete
TESTS+=		suff-lookup
TESTS+=		suff-main
TESTS+=		suff-main-several
TESTS+=		suff-phony
TESTS+=		suff-rebuild
TESTS+=		suff-self
TESTS+=		suff-transform-debug
TESTS+=		suff-transform-endless
TESTS+=		suff-transform-expand
TESTS+=		suff-transform-select
TESTS+=		suff-use
TESTS+=		sunshcmd
TESTS+=		ternary
TESTS+=		unexport
TESTS+=		unexport-env
TESTS+=		use-inference
TESTS+=		var-readonly
TESTS+=		var-scope
TESTS+=		var-scope-cmdline
TESTS+=		var-scope-env
TESTS+=		var-scope-global
TESTS+=		var-scope-local
TESTS+=		var-scope-local-legacy
TESTS+=		var-eval-short
TESTS+=		var-op
TESTS+=		var-op-append
TESTS+=		var-op-assign
TESTS+=		var-op-default
TESTS+=		var-op-expand
TESTS+=		var-op-shell
TESTS+=		var-op-sunsh
TESTS+=		var-recursive
TESTS+=		varcmd
TESTS+=		vardebug
TESTS+=		varfind
TESTS+=		varmisc
TESTS+=		varmod
TESTS+=		varmod-assign
TESTS+=		varmod-assign-shell
TESTS+=		varmod-defined
TESTS+=		varmod-edge
TESTS+=		varmod-exclam-shell
TESTS+=		varmod-extension
TESTS+=		varmod-gmtime
TESTS+=		varmod-hash
TESTS+=		varmod-head
TESTS+=		varmod-ifelse
TESTS+=		varmod-indirect
TESTS+=		varmod-l-name-to-value
TESTS+=		varmod-localtime
TESTS+=		varmod-loop
TESTS+=		varmod-loop-delete
TESTS+=		varmod-loop-varname
TESTS+=		varmod-match
TESTS+=		varmod-match-escape
TESTS+=		varmod-mtime
TESTS+=		varmod-no-match
TESTS+=		varmod-order
TESTS+=		varmod-order-numeric
TESTS+=		varmod-order-reverse
TESTS+=		varmod-order-shuffle
TESTS+=		varmod-order-string
TESTS+=		varmod-path
TESTS+=		varmod-quote
TESTS+=		varmod-quote-dollar
TESTS+=		varmod-range
TESTS+=		varmod-remember
TESTS+=		varmod-root
TESTS+=		varmod-select-words
TESTS+=		varmod-shell
TESTS+=		varmod-subst
TESTS+=		varmod-subst-regex
TESTS+=		varmod-sun-shell
TESTS+=		varmod-sysv
TESTS+=		varmod-tail
TESTS+=		varmod-to-abs
TESTS+=		varmod-to-lower
TESTS+=		varmod-to-many-words
TESTS+=		varmod-to-one-word
TESTS+=		varmod-to-separator
TESTS+=		varmod-to-upper
TESTS+=		varmod-undefined
TESTS+=		varmod-unique
TESTS+=		varname
TESTS+=		varname-dollar
TESTS+=		varname-dot-alltargets
TESTS+=		varname-dot-curdir
TESTS+=		varname-dot-includes
TESTS+=		varname-dot-includedfromdir
TESTS+=		varname-dot-includedfromfile
TESTS+=		varname-dot-libs
TESTS+=		varname-dot-make-dependfile
TESTS+=		varname-dot-make-expand_variables
TESTS+=		varname-dot-make-exported
TESTS+=		varname-dot-make-jobs
TESTS+=		varname-dot-make-jobs-prefix
TESTS+=		varname-dot-make-level
TESTS+=		varname-dot-make-makefile_preference
TESTS+=		varname-dot-make-makefiles
TESTS+=		varname-dot-make-meta-bailiwick
TESTS+=		varname-dot-make-meta-created
TESTS+=		varname-dot-make-meta-files
.if ${.MAKE.PATH_FILEMON:Uno:Nktrace:N/dev*} == "" && ${TMPDIR:N/tmp*:N/var/tmp*} != ""
# these tests will not work if TMPDIR is or is a subdir of
# /tmp or /var/tmp
.if ${.MAKE.PATH_FILEMON:N/dev/*} != "" || exists(${.MAKE.PATH_FILEMON})
TESTS+=		varname-dot-make-meta-ignore_filter
TESTS+=		varname-dot-make-meta-ignore_paths
TESTS+=		varname-dot-make-meta-ignore_patterns
TESTS+=		varname-dot-make-path_filemon
.else
.warning Skipping tests that require ${.MAKE.PATH_FILEMON}
.endif
.endif
TESTS+=		varname-dot-make-meta-prefix
TESTS+=		varname-dot-make-mode
TESTS+=		varname-dot-make-pid
TESTS+=		varname-dot-make-ppid
TESTS+=		varname-dot-make-save_dollars
TESTS+=		varname-dot-makeflags
TESTS+=		varname-dot-makeoverrides
TESTS+=		varname-dot-newline
TESTS+=		varname-dot-objdir
TESTS+=		varname-dot-parsedir
TESTS+=		varname-dot-parsefile
TESTS+=		varname-dot-path
TESTS+=		varname-dot-shell
TESTS+=		varname-dot-suffixes
TESTS+=		varname-dot-targets
TESTS+=		varname-empty
TESTS+=		varname-make
TESTS+=		varname-make_print_var_on_error
TESTS+=		varname-make_print_var_on_error-jobs
TESTS+=		varname-makefile
TESTS+=		varname-makeflags
TESTS+=		varname-pwd
TESTS+=		varname-vpath
TESTS+=		varparse-dynamic
TESTS+=		varparse-errors
TESTS+=		varparse-mod
TESTS+=		varparse-undef-partial

# some shells have quirks
_shell := ${.SHELL:tA:T}
.if ${_shell} == "dash" 
# dash fails -x output
BROKEN_TESTS+= opt-debug-x-trace
.elif ${_shell} == "ksh"
BROKEN_TESTS+= sh-flags
.endif

.if ${UTC_1:Uno} == ""
# this will not work if UTC_1 is set empty
BROKEN_TESTS+= varmod-localtime
.endif

.if ${.MAKE.OS:NDarwin} == ""
BROKEN_TESTS+= shell-ksh
.endif

.if ${.MAKE.OS:MIRIX*} != ""
BROKEN_TESTS+= \
	cmd-interrupt \
	deptgt-interrupt \
	job-output-null \
	opt-chdir \
	opt-debug-x-trace \
	sh-leading-hyphen \

.endif

.if ${.MAKE.OS} == "SCO_SV"
BROKEN_TESTS+= \
	opt-debug-graph[23] \
	varmod-localtime \
	varmod-to-separator \

.if ${_shell} == "bash"
BROKEN_TESTS+= job-output-null
.else
BROKEN_TESTS+= \
	cmd-interrupt \
	job-flags \

.endif
.endif

# Some tests just do not work on some platforms or environments
# so allow for some filtering.
.if !empty(BROKEN_TESTS)
.warning Skipping broken tests: ${BROKEN_TESTS:O:u}
TESTS:= ${TESTS:${BROKEN_TESTS:S,^,N,:ts:}}
.endif

# Ideas for more tests:
#	char-0020-space.mk
#	char-005C-backslash.mk
#	escape-cond-str.mk
#	escape-cond-func-arg.mk
#	escape-varmod.mk
#	escape-varmod-define.mk
#	escape-varmod-match.mk
#	escape-varname.mk
#	escape-varassign-varname.mk
#	escape-varassign-varname-cmdline.mk
#	escape-varassign-value.mk
#	escape-varassign-value-cmdline.mk
#	escape-dependency-source.mk
#	escape-dependency-target.mk
#	escape-for-varname.mk
#	escape-for-item.mk
#	posix-*.mk (see posix.mk and posix1.mk)

# Additional environment variables for some of the tests.
# The base environment is -i PATH="$PATH".
ENV.depsrc-optional+=   TZ=UTC
ENV.deptgt-phony+=	MAKESYSPATH=.
ENV.directive-undef=	ENV_VAR=env-value
ENV.opt-env=		FROM_ENV=value-from-env
ENV.opt-m-include-dir=	${MAKEOBJDIR:DMAKEOBJDIR=${MAKEOBJDIR}}
ENV.varmisc=		FROM_ENV=env
ENV.varmisc+=		FROM_ENV_BEFORE=env
ENV.varmisc+=		FROM_ENV_AFTER=env
ENV.varmod-localtime+=	TZ=${UTC_1:UEurope/Berlin}
ENV.varname-vpath+=	VPATH=varname-vpath.dir:varname-vpath.dir2

# Override make flags for some of the tests; default is -k.
# If possible, write ".MAKEFLAGS: -dv" in the test .mk file instead of
# settings FLAGS.test=-dv here, since that is closer to the test code.
FLAGS.cond-func-make=		via-cmdline
FLAGS.doterror=			# none, especially not -k
FLAGS.jobs-error-indirect=	# none, especially not -k
FLAGS.jobs-error-nested=	# none, especially not -k
FLAGS.jobs-error-nested-make=	# none, especially not -k
FLAGS.varname-empty=		-dv '$${:U}=cmdline-u' '=cmdline-plain'

# Some tests need extra postprocessing.
SED_CMDS.deptgt-phony=	${STD_SED_CMDS.dd}
SED_CMDS.dir=		${STD_SED_CMDS.dd}
SED_CMDS.directive-include-guard= \
	-e '/\.MAKEFLAGS/d' \
	-e '/^Parsing line/d' \
	-e '/^SetFilenameVars:/d' \
	-e '/^ParseDependency/d' \
	-e '/^ParseEOF:/d'
SED_CMDS.export=	-e '/^[^=_A-Za-z0-9]*=/d'
SED_CMDS.export-all=	${SED_CMDS.export}
SED_CMDS.export-env=	${SED_CMDS.export}
SED_CMDS.cmdline=	-e 's,uid${.MAKE.UID}/,,'
SED_CMDS.job-output-long-lines= \
	${:D Job separators on their own line are ok. } \
	-e '/^--- job-[ab] ---$$/d' \
	${:D Plain output lines are ok as well. } \
	${:D They may come in multiples of 1024 or as 10000. } \
	-e '/^aa*$$/d' \
	-e '/^bb*$$/d' \
	${:D The following lines should rather not occur since the job } \
	${:D marker should always be at the beginning of the line. } \
	-e '/^aa*--- job-b ---$$/d' \
	-e '/^bb*--- job-a ---$$/d'
SED_CMDS.opt-chdir=		-e 's,\(nonexistent\).[1-9][0-9]*,\1,' \
	-e '/name/s,file,File,' \
	-e 's,no such,No such,' \
	-e 's,Filename,File name,'

# meta line numbers can vary based on filemon implementation
SED_CMDS.meta-ignore= -e 's,\(\.meta:\) [1-9][0-9]*:,\1 <line>:,'
SED_CMDS.opt-debug-graph1=	${STD_SED_CMDS.dg1}
SED_CMDS.opt-debug-graph2=	${STD_SED_CMDS.dg2}
SED_CMDS.opt-debug-graph3=	${STD_SED_CMDS.dg3}
SED_CMDS.opt-debug-hash=	-e 's,\(numEntries\)=[1-9][0-9],\1=<entries>,'
SED_CMDS.opt-debug-jobs=	-e 's,([0-9][0-9]*),(<pid>),'
SED_CMDS.opt-debug-jobs+=	-e 's,pid [0-9][0-9]*,pid <pid>,'
SED_CMDS.opt-debug-jobs+=	-e 's,Process [0-9][0-9]*,Process <pid>,'
SED_CMDS.opt-debug-jobs+=	-e 's,JobFinish: [0-9][0-9]*,JobFinish: <pid>,'
SED_CMDS.opt-debug-jobs+=	-e 's,Command: ${.SHELL:T},Command: <shell>,'
# The "-q" may be there or not, see jobs.c, variable shells.
SED_CMDS.opt-debug-jobs+=	-e 's,^\(.Command: <shell>\) -q,\1,'
SED_CMDS.opt-debug-lint+=	${STD_SED_CMDS.regex}
SED_CMDS.opt-jobs-no-action=	${STD_SED_CMDS.hide-from-output}
SED_CMDS.opt-no-action-runflags= ${STD_SED_CMDS.hide-from-output}
SED_CMDS.opt-where-am-i=	-e '/usr.obj/d'
# For Compat_RunCommand, useShell == false.
SED_CMDS.sh-dots=		-e 's,^.*\.\.\.:.*,<not found: ...>,'
# For Compat_RunCommand, useShell == true.
SED_CMDS.sh-dots+=		-e 's,^make: exec(\(.*\)) failed (.*)$$,<not found: \1>,'
SED_CMDS.sh-dots+=		-e 's,^\(\*\*\* Error code \)[1-9][0-9]*,\1<nonzero>,'
SED_CMDS.sh-errctl=		${STD_SED_CMDS.dj}
SED_CMDS.sh-flags=		${STD_SED_CMDS.hide-from-output}
SED_CMDS.shell-csh=		${STD_SED_CMDS.white-space}
SED_CMDS.sh-leading-hyphen=	${STD_SED_CMDS.shell}
SED_CMDS.suff-main+=		${STD_SED_CMDS.dg1}
SED_CMDS.suff-main-several+=	${STD_SED_CMDS.dg1}
SED_CMDS.suff-transform-debug+=	${STD_SED_CMDS.dg1}
SED_CMDS.var-op-shell+=		${STD_SED_CMDS.shell}
SED_CMDS.var-op-shell+=		-e '/command/s,No such.*,not found,'
SED_CMDS.var-op-shell+=		${STD_SED_CMDS.white-space}
SED_CMDS.vardebug+=		-e 's,${.SHELL},</path/to/shell>,'
SED_CMDS.varmod-mtime+=		-e "s,': .*,': <ENOENT>,"
SED_CMDS.varmod-subst-regex+=	${STD_SED_CMDS.regex}
SED_CMDS.varparse-errors+=	${STD_SED_CMDS.timestamp}
SED_CMDS.varname-dot-make-meta-ignore_filter+= ${SED_CMDS.meta-ignore}
SED_CMDS.varname-dot-make-meta-ignore_paths+= ${SED_CMDS.meta-ignore}
SED_CMDS.varname-dot-make-meta-ignore_patterns+= ${SED_CMDS.meta-ignore}
SED_CMDS.varname-dot-parsedir=	-e '/in some cases/ s,^make: "[^"]*,make: "<normalized>,'
SED_CMDS.varname-dot-parsefile=	-e '/in some cases/ s,^make: "[^"]*,make: "<normalized>,'
SED_CMDS.varname-dot-shell=	-e 's, = /[^ ]*, = (details omitted),g'
SED_CMDS.varname-dot-shell+=	-e 's,"/[^" ]*","(details omitted)",g'
SED_CMDS.varname-dot-shell+=	-e 's,\[/[^] ]*\],[(details omitted)],g'
SED_CMDS.varname-empty=		${.OBJDIR .PARSEDIR .PATH .SHELL .SYSPATH:L:@v@-e '/\\$v/d'@}

# Some tests need an additional round of postprocessing.
POSTPROC.depsrc-wait=		sed -e '/^---/d' -e 's,^\(: Making 3[abc]\)[123]$$,\1,'
POSTPROC.deptgt-suffixes=	awk '/^\#\*\*\* Suffixes/,/^never-stop/'
POSTPROC.gnode-submake=		awk '/Input graph/, /^$$/'
POSTPROC.varname-dot-make-mode=	sed 's,^\(: Making [abc]\)[123]$$,\1,'

# Some tests reuse other tests, which makes them unnecessarily fragile.
export-all.rawout: export.mk
unexport.rawout: export.mk
unexport-env.rawout: export.mk

# End of the configuration section.

# Some standard sed commands, to be used in the SED_CMDS above.

# In tests that use the debugging option -dd, ignore debugging output that is
# only logged in -DCLEANUP mode.
STD_SED_CMDS.dd=		-e '/^OpenDirs_Done:/d'
STD_SED_CMDS.dd+=		-e '/^CachedDir /d'

# Omit details such as process IDs from the output of the -dg1 option.
STD_SED_CMDS.dg1=	-e '/\#.* \.$$/d'
STD_SED_CMDS.dg1+=	-e '/\.MAKE.PATH_FILEMON/d'
STD_SED_CMDS.dg1+=	-e '/^\#.*\/mk/d'
STD_SED_CMDS.dg1+=	-e 's,  ${DEFSYSPATH:U/usr/share/mk}$$,  <defsyspath>,'
STD_SED_CMDS.dg1+=	-e 's,^\(\.MAKE *=\) .*,\1 <details omitted>,'
STD_SED_CMDS.dg1+=	-e 's,^\(\.MAKE\.[A-Z_]* *=\) .*,\1 <details omitted>,'
STD_SED_CMDS.dg1+=	-e 's,^\(\.MAKE\.JOBS\.C *=\) .*,\1 <details omitted>,'
STD_SED_CMDS.dg1+=	-e 's,^\(MACHINE[_ARCH]* *=\) .*,\1 <details omitted>,'
STD_SED_CMDS.dg1+=	-e 's,^\(MAKE *=\) .*,\1 <details omitted>,'
STD_SED_CMDS.dg1+=	-e 's,^\(\.SHELL *=\) .*,\1 <details omitted>,'
STD_SED_CMDS.dg1+=	-e '/\.SYSPATH/d'

STD_SED_CMDS.dg2=	${STD_SED_CMDS.dg1}
STD_SED_CMDS.dg2+=	-e 's,\(last modified\) ..:..:.. ... ..\, ....,\1 <timestamp>,'
STD_SED_CMDS.dg3=	${STD_SED_CMDS.dg2}

# Omit details such as process IDs from the output of the -dj option.
STD_SED_CMDS.dj= \
	-e '/Process/d;/JobFinish:/d' \
	-e 's,^\(Job_TokenWithdraw\)([0-9]*),\1(<pid>),' \
	-e 's,^([0-9][0-9]*) \(withdrew token\),(<pid>) \1,' \
	-e 's, \(pid\) [0-9][0-9]*, \1 <pid>,' \
	-e 's,^\(	Command:\) .*,\1 <shell>,'

# Reduce the noise for tests running with the -n option, since there is no
# other way to suppress the echoing of the commands.
STD_SED_CMDS.hide-from-output= \
	-e '/^echo hide-from-output/d' \
	-e 's,hide-from-output ,,' \
	-e 's,hide-from-output,,'

# Normalize the output for error messages from the shell.
#
# $shell -c '...'
#	NetBSD sh	...: not found
#	NetBSD ksh	ksh: ...: not found
#	bash 5.0.18	bash: ...: command not found
#	bash 5.1.0	bash: line 1: ...: command not found
#	dash		dash: 1: ...: not found
#
# $shell -c '< /nonexistent'
#	NetBSD sh	sh: cannot open /nonexistent: no such file
#	NetBSD ksh	ksh: cannot open /nonexistent: No such file or directory
#	bash 5.0.18	bash: /nonexistent: No such file or directory
#	bash 5.1.0	bash: line 1: /nonexistent: No such file or directory
#	dash		dash: 1: cannot open /nonexistent: No such file
#
# echo '< /nonexistent' | $shell
#	NetBSD sh	sh: cannot open /nonexistent: no such file
#	NetBSD ksh	ksh: <stdin>[1]: cannot open /nonexistent: No such file or directory
#	bash 5.0.18	bash: line 1: /nonexistent: No such file or directory
#	bash 5.1.0	bash: line 1: /nonexistent: No such file or directory
#	dash		dash: 1: cannot open /nonexistent: No such file
#
STD_SED_CMDS.shell+=		-e 's,^${.SHELL},${.SHELL:T},'
STD_SED_CMDS.shell+=		-e 's,^${.SHELL:T}: line [0-9][0-9]*: ,,'
STD_SED_CMDS.shell+=		-e 's,^${.SHELL:T}: [0-9][0-9]*: ,,'
STD_SED_CMDS.shell+=		-e 's,^${.SHELL:T}: ,,'
STD_SED_CMDS.shell+=		-e 's,: command not found,: not found,'

STD_SED_CMDS.white-space=	-e 's,   *, ,g' -e 's,  *$$,,'

# The actual error messages for a failed regcomp or regexec differ between the
# implementations.
STD_SED_CMDS.regex= \
	-e 's,\(Regex compilation error:\).*,\1 (details omitted),'

# Normalize timestamps from ':gmtime' or ':localtime' to '<timestamp>'.
# See STD_SED_CMDS.dg2 for timestamps from the debug log.
STD_SED_CMDS.timestamp= \
	-e 's,[A-Z][a-z][a-z] [A-Z][a-z][a-z] [ 0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9] [12][0-9][0-9][0-9],<timestamp>,'

# End of the configuration helpers section.

.-include "Makefile.inc"
.-include "Makefile.config"

UNIT_TESTS:=	${.PARSEDIR}
.PATH: ${UNIT_TESTS}

.if ${USE_ABSOLUTE_TESTNAMES:Uno} == yes
OUTFILES=	${TESTS:@test@${.CURDIR:tA}/${test}.out@}
.else
OUTFILES=	${TESTS:=.out}
.endif

all: ${OUTFILES}

CLEANFILES=		*.rawout *.out *.status *.tmp *.core *.tmp
CLEANFILES+=		obj*.[och] lib*.a	# posix1.mk
CLEANFILES+=		issue* .[ab]*		# suffixes.mk
CLEANDIRS=		dir dummy		# posix1.mk

clean:
	rm -f ${CLEANFILES}
	rm -rf ${CLEANDIRS}

TEST_MAKE?=	${.MAKE}
TOOL_SED?=	sed
TOOL_TR?=	tr
TOOL_DIFF?=	diff
DIFF_FLAGS?=	-u

.if defined(.PARSEDIR)
# ensure consistent results from sort(1)
LC_ALL=		C
LANG=		C
.export LANG LC_ALL
.endif

.if ${.MAKE.MODE:Unormal:Mmeta} != ""
# we don't need the noise
_MKMSG_TEST= :
.endif

# Some Linux systems such as Fedora have deprecated egrep in favor of grep -E.
.if ${.MAKE.OS:NLinux} == ""
EGREP= grep -E
.endif
# Keep the classical definition for all other systems.  Just as the bmake code
# is kept compatible with C90, the tests are kept compatible with systems that
# are several decades old and don't follow modern POSIX standards.
EGREP?= egrep

MAKE_TEST_ENV=  EGREP="${EGREP}"
MAKE_TEST_ENV+=	MALLOC_OPTIONS="JA"	# for jemalloc 100
MAKE_TEST_ENV+=	MALLOC_CONF="junk:true"	# for jemalloc 510
MAKE_TEST_ENV+= TMPDIR=${TMPDIR}

.if ${.MAKE.OS} == "NetBSD"
LIMIT_RESOURCES?=	ulimit -v 300000
.endif
LIMIT_RESOURCES?=	:

# Each test is run in a sub-make, to keep the tests from interfering with
# each other, and because they use different environment variables and
# command line options.
.SUFFIXES: .mk .rawout .out
.mk.rawout:
	@${_MKMSG_TEST:Uecho '#      test '} ${.PREFIX}
	@set -eu; \
	${LIMIT_RESOURCES}; \
	cd ${.OBJDIR}; \
	env -i PATH="$$PATH" ${MAKE_TEST_ENV} ${ENV.${.PREFIX:T}} \
	  ${TEST_MAKE} \
	    -r -C ${.CURDIR} -f ${.IMPSRC} \
	    ${FLAGS.${.PREFIX:T}:U-k} \
	    > ${.TARGET}.tmp 2>&1 \
	&& status=$$? || status=$$?; \
	echo $$status > ${.TARGET:R}.status
	@mv ${.TARGET}.tmp ${.TARGET}

# Postprocess the test output to make the output platform-independent.
#
# Replace anything after 'stopped in' with unit-tests
_SED_CMDS+=	-e '/stopped/s, /.*, unit-tests,'
# Allow the test files to be placed anywhere.
_SED_CMDS+=	-e 's,\(\.PARSEDIR}\) = `'"/[^']*'"',\1 = <some-dir>,'
_SED_CMDS+=	-e 's,\(\.INCLUDEDFROMDIR}\) = `'"/[^']*'"',\1 = <some-dir>,'
_SED_CMDS+=	-e 's,${TMPDIR},<tmpdir>,g' -e 's,${TMPDIR:tA},<tmpdir>,g'
# canonicalize ${.OBJDIR} and ${.CURDIR}
_SED_CMDS+=	-e 's,${.CURDIR},<curdir>,g'
.if ${.OBJDIR} != ${.CURDIR}
# yes this is inaccurate but none of the tests expect <objdir> anywhere
# which we get depending on how MAKEOBJDIR is set.
_SED_CMDS+=	-e 's,${.OBJDIR},<curdir>,g' -e 's,${.OBJDIR:tA},<curdir>,g'
.endif
# always pretend .MAKE was called 'make'
_SED_CMDS+=	-e 's,^${TEST_MAKE:T:S,.,\\.,g}[][0-9]*:,make:,'
_SED_CMDS+=	-e 's,${TEST_MAKE:S,.,\\.,g},make,'
_SED_CMDS+=	-e 's,^usage: ${TEST_MAKE:T:S,.,\\.,g} ,usage: make ,'
_SED_CMDS+=	-e 's,<curdir>/,,g'
_SED_CMDS+=	-e 's,${UNIT_TESTS:S,.,\\.,g}/,,g'
_SED_CMDS+=	-e '/MAKE_VERSION/d'
_SED_CMDS+=	-e '/EGREP=/d'

# on AT&T derived systems: false exits 255 not 1
.if ${.MAKE.OS:N*BSD} != ""
_SED_CMDS+=	-e 's,\(Error code\) 255,\1 1,'
.endif
.if ${.SHELL:T} == "ksh"
_SED_CMDS+=	-e '/^set [+-]v/d'
.endif

.rawout.out:
	@${TOOL_SED} ${_SED_CMDS} ${SED_CMDS.${.PREFIX:T}} \
	  < ${.IMPSRC} > ${.TARGET}.tmp1
	@${POSTPROC.${.PREFIX:T}:Ucat} < ${.TARGET}.tmp1 > ${.TARGET}.tmp2
	@rm ${.TARGET}.tmp1
	@echo "exit status `cat ${.TARGET:R}.status`" >> ${.TARGET}.tmp2
	@mv ${.TARGET}.tmp2 ${.TARGET}

.if empty(DIFF_FLAGS)
DIFF_ECHO= echo
.else
DIFF_ECHO= :
.endif

# Compare all output files
test:	${OUTFILES} .PHONY
	@failed= ; \
	for test in ${TESTS}; do \
	  cmp -s ${UNIT_TESTS}/$${test}.exp $${test}.out && continue || \
	  ${DIFF_ECHO} diff ${UNIT_TESTS}/$${test}.exp $${test}.out; \
	  ${TOOL_DIFF} ${DIFF_FLAGS} ${UNIT_TESTS}/$${test}.exp $${test}.out \
	  || failed="$${failed}$${failed:+ }$${test}" ; \
	done ; \
	if [ -n "$${failed}" ]; then \
	  echo "Failed tests: $${failed}" ; false ; \
	else \
	  echo "All tests passed" ; \
	fi

accept:
	@for test in ${TESTS}; do \
	  cmp -s ${UNIT_TESTS}/$${test}.exp $${test}.out \
	  || { echo "Replacing $${test}.exp" ; \
	       cp $${test}.out ${UNIT_TESTS}/$${test}.exp ; } \
	done

.if exists(${TEST_MAKE})
${TESTS:=.rawout}: ${TEST_MAKE}
# in meta mode, we *know* if a target script is impacted
# by a makefile change.
.if ${.MAKE.MODE:Unormal:Mmeta} == ""
${TESTS:=.rawout}: ${.PARSEDIR}/Makefile
.endif
.endif

.-include <obj.mk>

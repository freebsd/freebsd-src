#

.MAIN: all

.if make(output)
.MAKE.MODE= meta curDirOk=true nofilemon
.else
.MAKE.MODE= compat
.endif

all: output.-B output.-j1

_mf := ${.PARSEDIR}/${.PARSEFILE}

# This output should be accurately reflected in the .meta file.
# We append an extra newline to ${.TARGET} (after it has been
# written to stdout) to match what meta_cmd_finish() will do.
output: .NOPATH
	@{ echo Test ${tag} output; \
	for i in 1 2 3; do \
	printf "test$$i:  "; sleep 0; echo " Done"; \
	done; echo; } | tee ${.TARGET}; echo >> ${.TARGET}

# The diff at the end should produce nothing.
output.-B output.-j1:
	@{ rm -f ${TMPDIR}/output; mkdir -p ${TMPDIR}/obj; \
	MAKEFLAGS= ${.MAKE} -r -C ${TMPDIR} ${.TARGET:E} tag=${.TARGET:E} -f ${_mf} output; \
	sed '1,/command output/d' ${TMPDIR}/obj/output.meta > ${TMPDIR}/obj/output-meta; \
	${DIFF:Udiff} ${TMPDIR}/obj/output ${TMPDIR}/obj/output-meta; }


ACLOCAL_AMFLAGS = -I m4

SUBDIRS = misc include lib bin modules

if WITH_DOC
SUBDIRS += doc
endif

SUBDIRS += t

EXTRA_DIST = \
	CREDITS \
	HISTORY \
	INSTALL \
	LICENSE \
	README \
	RELNOTES \
	autogen.sh \
	misc/gendoc.pl

if WITH_CODE_COVERAGE
covdir = @abs_top_builddir@/cov
coverage: coverage-clean all coverage-prepare coverage-run coverage-report
coverage-clean:
	-rm -rf "${covdir}"
coverage-prepare:
	mkdir "${covdir}"
if CLANG_CODE_COVERAGE
profdata = ${covdir}/@PACKAGE@.profdata
# hardcoding libpam.so here is horrible, need to find a better solution
coverage-run:
	LLVM_PROFILE_FILE="${covdir}/@PACKAGE@.%p.raw" \
	    ${MAKE} -C "@abs_top_builddir@" check
coverage-report:
	llvm-profdata@clang_ver@ merge \
	    --sparse "${covdir}/@PACKAGE@".*.raw -o "${profdata}"
	llvm-cov@clang_ver@ show \
	    --format=html --tab-size=8 \
	    --output-dir="${covdir}" \
	    --instr-profile="${profdata}" \
	    --object "@abs_top_builddir@/lib/libpam/.libs/libpam.so"
	@echo "coverage report: file://${covdir}/index.html"
endif
else
coverage:
	echo "code coverage is not enabled." >&2
	false
endif

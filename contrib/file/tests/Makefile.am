check_PROGRAMS = test
test_LDADD = $(top_builddir)/src/libmagic.la
test_CPPFLAGS = -I$(top_builddir)/src

EXTRA_DIST = \
regex-eol.magic \
CVE-2014-1943.testfile \
JW07022A.mp3.testfile \
escapevel.testfile \
fit-map-data.testfile \
gedcom.testfile \
hddrawcopytool.testfile \
issue311docx.testfile \
issue359xlsx.testfile \
json1.testfile \
json2.testfile \
json3.testfile \
regex-eol.testfile \
zstd-v0.2-FF.testfile \
zstd-v0.3-FF.testfile \
zstd-v0.4-FF.testfile \
zstd-v0.5-FF.testfile \
zstd-v0.6-FF.testfile \
zstd-v0.7-21.testfile \
zstd-v0.7-22.testfile \
zstd-v0.8-01.testfile \
zstd-v0.8-02.testfile \
zstd-v0.8-03.testfile \
zstd-v0.8-16.testfile \
zstd-v0.8-20.testfile \
zstd-v0.8-21.testfile \
zstd-v0.8-22.testfile \
zstd-v0.8-23.testfile \
zstd-v0.8-F4.testfile \
zstd-v0.8-FF.testfile \
CVE-2014-1943.result \
JW07022A.mp3.result \
escapevel.result \
fit-map-data.result \
gedcom.result \
hddrawcopytool.result \
issue311docx.result \
issue359xlsx.result \
json1.result \
json2.result \
json3.result \
regex-eol.result \
zstd-3-skippable-frames.result \
zstd-dictionary-0.result \
zstd-dictionary-1.result \
zstd-dictionary-2.result \
zstd-skippable-frame-0.result \
zstd-skippable-frame-4.result \
zstd-skippable-frame-8.result \
zstd-skippable-frame-C.result \
zstd-v0.2-FF.result \
zstd-v0.3-FF.result \
zstd-v0.4-FF.result \
zstd-v0.5-FF.result \
zstd-v0.6-FF.result \
zstd-v0.7-00.result \
zstd-v0.7-21.result \
zstd-v0.7-22.result \
zstd-v0.8-00.result \
zstd-v0.8-01.result \
zstd-v0.8-02.result \
zstd-v0.8-03.result \
zstd-v0.8-16.result \
zstd-v0.8-20.result \
zstd-v0.8-21.result \
zstd-v0.8-22.result \
zstd-v0.8-23.result \
zstd-v0.8-F4.result \
zstd-v0.8-FF.result

T = $(top_srcdir)/tests
check-local:
	MAGIC=$(top_builddir)/magic/magic ./test
	set -e; \
	for i in $T/*.testfile; do \
		echo Running test: $$i; \
		if [ -f $${i%%.testfile}.magic ]; then \
			m=$${i%%.testfile}.magic; \
		else \
			m=$(top_builddir)/magic/magic; \
		fi; \
		TZ=UTC MAGIC=$$m ./test $$i $${i%%.testfile}.result; \
	done

check_PROGRAMS = test
test_LDADD = $(top_builddir)/src/libmagic.la
test_CPPFLAGS = -I$(top_builddir)/src

EXTRA_DIST = \
android-vdex-1.result \
android-vdex-1.testfile \
android-vdex-2.result \
android-vdex-2.testfile \
arj.result \
arj.testfile \
CVE-2014-1943.result \
CVE-2014-1943.testfile \
JW07022A.mp3.result \
JW07022A.mp3.testfile \
bcachefs.result \
bcachefs.testfile \
cl8m8ocofedso.result \
cl8m8ocofedso.testfile \
dsd64-dff.result \
dsd64-dff.testfile \
dsd64-dsf.result \
dsd64-dsf.testfile \
escapevel.result \
escapevel.testfile \
ext4.result \
ext4.testfile \
fit-map-data.result \
fit-map-data.testfile \
gedcom.result \
gedcom.testfile \
gpkg-1-zst.result \
gpkg-1-zst.testfile \
hddrawcopytool.result \
hddrawcopytool.testfile \
issue311docx.result \
issue311docx.testfile \
issue359xlsx.result \
issue359xlsx.testfile \
json1.result \
json1.testfile \
json2.result \
json2.testfile \
json3.result \
json3.testfile \
json4.result \
json4.testfile \
json5.result \
json5.testfile \
json6.result \
json6.testfile \
json7.result \
json7.testfile \
json8.result \
json8.testfile \
jsonlines1.testfile \
jsonlines1.result \
matilde.arm.result \
matilde.arm.testfile \
pcjr.result \
pcjr.testfile \
pgp-binary-key-v2-phil.result \
pgp-binary-key-v2-phil.testfile \
pgp-binary-key-v3-lutz.result \
pgp-binary-key-v3-lutz.testfile \
pgp-binary-key-v4-dsa.result \
pgp-binary-key-v4-dsa.testfile \
pgp-binary-key-v4-ecc-no-userid-secret.result \
pgp-binary-key-v4-ecc-no-userid-secret.testfile \
pgp-binary-key-v4-ecc-secret-key.result \
pgp-binary-key-v4-ecc-secret-key.testfile \
pgp-binary-key-v4-rsa-key.result \
pgp-binary-key-v4-rsa-key.testfile \
pgp-binary-key-v4-rsa-no-userid-secret.result \
pgp-binary-key-v4-rsa-no-userid-secret.testfile \
pgp-binary-key-v4-rsa-secret-key.result \
pgp-binary-key-v4-rsa-secret-key.testfile \
regex-eol.magic \
regex-eol.result \
regex-eol.testfile \
uf2.result \
uf2.testfile \
zstd-3-skippable-frames.result \
zstd-dictionary-0.result \
zstd-dictionary-1.result \
zstd-dictionary-2.result \
zstd-skippable-frame-0.result \
zstd-skippable-frame-4.result \
zstd-skippable-frame-8.result \
zstd-skippable-frame-C.result \
zstd-v0.2-FF.result \
zstd-v0.2-FF.testfile \
zstd-v0.3-FF.result \
zstd-v0.3-FF.testfile \
zstd-v0.4-FF.result \
zstd-v0.4-FF.testfile \
zstd-v0.5-FF.result \
zstd-v0.5-FF.testfile \
zstd-v0.6-FF.result \
zstd-v0.6-FF.testfile \
zstd-v0.7-00.result \
zstd-v0.7-21.result \
zstd-v0.7-21.testfile \
zstd-v0.7-22.result \
zstd-v0.7-22.testfile \
zstd-v0.8-00.result \
zstd-v0.8-01.result \
zstd-v0.8-01.testfile \
zstd-v0.8-02.result \
zstd-v0.8-02.testfile \
zstd-v0.8-03.result \
zstd-v0.8-03.testfile \
zstd-v0.8-16.result \
zstd-v0.8-16.testfile \
zstd-v0.8-20.result \
zstd-v0.8-20.testfile \
zstd-v0.8-21.result \
zstd-v0.8-21.testfile \
zstd-v0.8-22.result \
zstd-v0.8-22.testfile \
zstd-v0.8-23.result \
zstd-v0.8-23.testfile \
zstd-v0.8-F4.result \
zstd-v0.8-F4.testfile \
zstd-v0.8-FF.result \
zstd-v0.8-FF.testfile

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

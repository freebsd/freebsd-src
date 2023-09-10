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
bcachefs.result \
bcachefs.testfile \
bcachefs2.result \
bcachefs2.testfile \
cl8m8ocofedso.result \
cl8m8ocofedso.testfile \
cmd1.result \
cmd1.testfile \
cmd2.result \
cmd2.testfile \
cmd3.result \
cmd3.testfile \
cmd4.result \
cmd4.testfile \
CVE-2014-1943.result \
CVE-2014-1943.testfile \
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
hello-racket_rkt.result \
hello-racket_rkt.testfile \
hddrawcopytool.result \
hddrawcopytool.testfile \
HWP2016.hwp.result \
HWP2016.hwp.testfile \
HWP2016.hwpx.zip.result \
HWP2016.hwpx.zip.testfile \
HWP97.hwp.result \
HWP97.hwp.testfile \
issue311docx.result \
issue311docx.testfile \
issue359xlsx.result \
issue359xlsx.testfile \
jpeg-text.result \
jpeg-text.testfile \
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
jsonlines1.result \
jsonlines1.testfile \
JW07022A.mp3.result \
JW07022A.mp3.testfile \
matilde.arm.result \
matilde.arm.testfile \
multiple-A.magic \
multiple-B.magic \
multiple.flags \
multiple.result \
multiple.testfile \
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
pnm1.result \
pnm1.testfile \
pnm2.result \
pnm2.testfile \
pnm3.result \
pnm3.testfile \
regex-eol.magic \
regex-eol.result \
regex-eol.testfile \
registry-pol.result \
registry-pol.testfile \
uf2.result \
uf2.testfile \
xclbin.result \
xclbin.testfile \
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
	set -e; \
	for i in $T/*.testfile; do \
		t=$${i%%.testfile}; \
		echo Running test: $$t; \
		m=; \
		for j in $$(eval echo $${t}\*.magic); do \
			if [ -f "$$j" ]; then \
				if [ -z "$$m" ]; then \
					m=$$j; \
				else \
					m=$$m:$$j; \
				fi \
			fi \
		done; \
		if [ -z "$$m" ]; then \
			m=$(top_builddir)/magic/magic; \
		fi; \
		f=-e; \
		if [ -f $${t}.flags ]; then \
			f=$$f$$(cat $${t}.flags); \
		fi; \
		echo TZ=UTC MAGIC=$$m ./test $$f $$i $${t}.result; \
		TZ=UTC MAGIC=$$m ./test $$f $$i $${t}.result; \
	done

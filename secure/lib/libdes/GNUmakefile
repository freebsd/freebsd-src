# This was GNUmakefile
# I have changed the library a bit since I was last able to test the
# build so this may require a little tweaking.
# have fun.

ifeq ($(wildcard ../Rules),)
# no glibc
include Makefile.uni
else
# We are part of glibc
default_cflags:= -O2
subdir := des
headers:= des.h rpc_des.h

routines:= $(destest)
	cbc3_enc cbc_cksm cbc_enc  cfb64enc cfb_enc  \
	ecb3_enc ecb_enc  ede_enc  enc_read enc_writ \
	fcrypt   ncbc_enc ofb64enc ofb_enc  pcbc_enc \
	qud_cksm rand_key read_pwd rpc_enc  set_key  \
	str2key  cfb64ede ofb64ede supp
distribute:= ARTISTIC COPYRIGHT FILES INSTALL Imakefile README VERSION \
	GNUmakefile times vms.com KERBEROS MODES.DES \
	des.man des_crypt.man \
	des_ver.h des_locl.h podd.h sk.h spr.h \
	des.pl testdes.pl doIP doPC1 doPC2 PC1 PC2 shifts.pl \
	des.doc makefile.bc

tests:= destest speed
override +gccwarn := -w
others:= des rpw
install:= des

include ../Rules
endif


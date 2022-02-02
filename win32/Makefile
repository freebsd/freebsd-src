INT = python3
EXT = py
BIN = ../bin
UNI = ../uni

UCS2ANY = $(INT) $(BIN)/ucstoany.$(EXT)
BDF2FNT = $(INT) $(BIN)/bdftofnt.$(EXT) -fModern

CC32 = i686-w64-mingw32-gcc
CFLAGS = -O2 -Wall -Wextra
LDFLAGS = -mwindows -s
FONFLAGS = -shared -nostdlib -nostartfiles
RC32 = windres -F pe-i386
RM = rm -f

REG_MS_1252 = Microsoft CP1252
REG_MS_1250 = Microsoft CP1250
REG_MS_1253 = Microsoft CP1253
REG_MS_1254 = Microsoft CP1254
REG_MS_1251 = Microsoft CP1251
REG_MS_1257 = Microsoft CP1257
REG_MS_1255 = Microsoft CP1255
REG_IBM_437 = IBM CP437
REG_BG_MIK  = Bulgarian MIK

FNT_MS_1252 = ter-112n.fnt ter-112b.fnt ter-114n.fnt ter-114b.fnt ter-116n.fnt ter-116b.fnt ter-118n.fnt ter-118b.fnt ter-120n.fnt ter-120b.fnt ter-122n.fnt ter-122b.fnt ter-124n.fnt ter-124b.fnt ter-128n.fnt ter-128b.fnt ter-132n.fnt ter-132b.fnt
FNT_MS_1250 = ter-212n.fnt ter-212b.fnt ter-214n.fnt ter-214b.fnt ter-216n.fnt ter-216b.fnt ter-218n.fnt ter-218b.fnt ter-220n.fnt ter-220b.fnt ter-222n.fnt ter-222b.fnt ter-224n.fnt ter-224b.fnt ter-228n.fnt ter-228b.fnt ter-232n.fnt ter-232b.fnt
FNT_MS_1253 = ter-712n.fnt ter-712b.fnt ter-714n.fnt ter-714b.fnt ter-716n.fnt ter-716b.fnt ter-718n.fnt ter-718b.fnt ter-720n.fnt ter-720b.fnt ter-722n.fnt ter-722b.fnt ter-724n.fnt ter-724b.fnt ter-728n.fnt ter-728b.fnt ter-732n.fnt ter-732b.fnt
FNT_MS_1254 = ter-912n.fnt ter-912b.fnt ter-914n.fnt ter-914b.fnt ter-916n.fnt ter-916b.fnt ter-918n.fnt ter-918b.fnt ter-920n.fnt ter-920b.fnt ter-922n.fnt ter-922b.fnt ter-924n.fnt ter-924b.fnt ter-928n.fnt ter-928b.fnt ter-932n.fnt ter-932b.fnt
FNT_MS_1251 = ter-c12n.fnt ter-c12b.fnt ter-c14n.fnt ter-c14b.fnt ter-c16n.fnt ter-c16b.fnt ter-c18n.fnt ter-c18b.fnt ter-c20n.fnt ter-c20b.fnt ter-c22n.fnt ter-c22b.fnt ter-c24n.fnt ter-c24b.fnt ter-c28n.fnt ter-c28b.fnt ter-c32n.fnt ter-c32b.fnt
FNT_MS_1257 = ter-d12n.fnt ter-d12b.fnt ter-d14n.fnt ter-d14b.fnt ter-d16n.fnt ter-d16b.fnt ter-d18n.fnt ter-d18b.fnt ter-d20n.fnt ter-d20b.fnt ter-d22n.fnt ter-d22b.fnt ter-d24n.fnt ter-d24b.fnt ter-d28n.fnt ter-d28b.fnt ter-d32n.fnt ter-d32b.fnt
FNT_MS_1255 = ter-h12n.fnt ter-h12b.fnt ter-h14n.fnt ter-h14b.fnt ter-h16n.fnt ter-h16b.fnt ter-h18n.fnt ter-h18b.fnt ter-h20n.fnt ter-h20b.fnt ter-h22n.fnt ter-h22b.fnt ter-h24n.fnt ter-h24b.fnt ter-h28n.fnt ter-h28b.fnt ter-h32n.fnt ter-h32b.fnt
FNT_IBM_437 = ter-i12n.fnt ter-i12b.fnt ter-i14n.fnt ter-i14b.fnt ter-i16n.fnt ter-i16b.fnt ter-i18n.fnt ter-i18b.fnt ter-i20n.fnt ter-i20b.fnt ter-i22n.fnt ter-i22b.fnt ter-i24n.fnt ter-i24b.fnt ter-i28n.fnt ter-i28b.fnt ter-i32n.fnt ter-i32b.fnt
FNT_BG_MIK  = ter-m12n.fnt ter-m12b.fnt ter-m14n.fnt ter-m14b.fnt ter-m16n.fnt ter-m16b.fnt ter-m18n.fnt ter-m18b.fnt ter-m20n.fnt ter-m20b.fnt ter-m22n.fnt ter-m22b.fnt ter-m24n.fnt ter-m24b.fnt ter-m28n.fnt ter-m28b.fnt ter-m32n.fnt ter-m32b.fnt
FNT = $(FNT_MS_1252) $(FNT_MS_1250) $(FNT_MS_1253) $(FNT_MS_1254) $(FNT_MS_1251) $(FNT_MS_1257) $(FNT_MS_1255)

all: $(FNT) terminus.fon fcpw.exe

WIN_MS_1252 = $(UNI)/cntrl.uni $(UNI)/ascii-h.uni $(UNI)/win-1252.uni
WIN_MS_1250 = $(UNI)/cntrl.uni $(UNI)/ascii-h.uni $(UNI)/win-1250.uni
WIN_MS_1253 = $(UNI)/cntrl.uni $(UNI)/ascii-h.uni $(UNI)/win-1253.uni
WIN_MS_1254 = $(UNI)/cntrl.uni $(UNI)/ascii-h.uni $(UNI)/win-1254.uni
WIN_MS_1251 = $(UNI)/cntrl.uni $(UNI)/ascii-h.uni $(UNI)/x11-1251.uni $(UNI)/win-1251.uni
WIN_MS_1257 = $(UNI)/cntrl.uni $(UNI)/ascii-h.uni $(UNI)/x11-1257.uni $(UNI)/win-1257.uni
WIN_MS_1255 = $(UNI)/cntrl.uni $(UNI)/ascii-h.uni $(UNI)/win-1255.uni
WIN_IBM_437 = $(UNI)/cntrl.uni $(UNI)/ascii-h.uni $(UNI)/ibm-437.uni
WIN_BG_MIK  = $(UNI)/cntrl.uni $(UNI)/ascii-h.uni $(UNI)/bg-mik.uni

$(FNT_MS_1252): ter-1%.fnt : ter-u%.bdf $(WIN_MS_1252)
	$(UCS2ANY) $< $(REG_MS_1252) $(WIN_MS_1252) | $(BDF2FNT) -o $@

$(FNT_MS_1250): ter-2%.fnt : ter-u%.bdf $(WIN_MS_1250)
	$(UCS2ANY) $< $(REG_MS_1250) $(WIN_MS_1250) | $(BDF2FNT) -o $@

$(FNT_MS_1253): ter-7%.fnt : ter-u%.bdf $(WIN_MS_1253)
	$(UCS2ANY) $< $(REG_MS_1253) $(WIN_MS_1253) | $(BDF2FNT) -o $@

$(FNT_MS_1254): ter-9%.fnt : ter-u%.bdf $(WIN_MS_1254)
	$(UCS2ANY) $< $(REG_MS_1254) $(WIN_MS_1254) | $(BDF2FNT) -o $@

$(FNT_MS_1251): ter-c%.fnt : ter-u%.bdf $(WIN_MS_1251)
	$(UCS2ANY) $< $(REG_MS_1251) $(WIN_MS_1251) | $(BDF2FNT) -o $@

$(FNT_MS_1257): ter-d%.fnt : ter-u%.bdf $(WIN_MS_1257)
	$(UCS2ANY) $< $(REG_MS_1257) $(WIN_MS_1257) | $(BDF2FNT) -o $@

$(FNT_MS_1255): ter-h%.fnt : ter-u%.bdf $(WIN_MS_1255)
	$(UCS2ANY) $< $(REG_MS_1255) $(WIN_MS_1255) | $(BDF2FNT) -o $@

$(FNT_IBM_437): ter-i%.fnt : ter-u%.bdf $(WIN_IBM_437)
	$(UCS2ANY) -F Terminal $< $(REG_IBM_437) $(WIN_IBM_437) | $(BDF2FNT) -o $@

$(FNT_BG_MIK): ter-m%.fnt : ter-u%.bdf $(WIN_BG_MIK)
	$(UCS2ANY) -F Terminal $< $(REG_BG_MIK) $(WIN_BG_MIK) | $(BDF2FNT) -o $@

fnt: $(FNT)

fnt-255: $(FNT_IBM_437) $(FNT_BG_MIK)

ter-font.o: $(FNT) ter-font.rc
	$(RC32) -o $@ ter-font.rc

terminus.fon: ter-main.c ter-font.o
	$(CC32) $(CFLAGS) $(LDFLAGS) $(FONFLAGS) -o $@ ter-main.c ter-font.o

fcpw.exe: fcp.c
	$(CC32) -DFCPW $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	$(RM) ter-*.bdf ter-*.fnt *.txt ter-*.o terminus.fon fcpw.exe

.PHONY: all fnt fnt-255 clean

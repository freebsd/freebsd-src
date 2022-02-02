INT = python3
EXT = py
BIN = ./bin

UCS2ANY = $(INT) $(BIN)/ucstoany.$(EXT)
BDF2PSF = $(INT) $(BIN)/bdftopsf.$(EXT)
UCS2X11 = $(INT) $(BIN)/ucstoany.$(EXT) -f
BDF2PCF = bdftopcf
#BDF2OTB = fontforge -lang=ff -c 'Open($$3); ScaleToEm(1024); Generate($$2)'
BDF2OTB = $(INT) $(BIN)/otb1cli.$(EXT)

REG_8859_1  = ISO8859 1
REG_8859_2  = ISO8859 2
REG_8859_5  = ISO8859 5
REG_8859_7  = ISO8859 7
REG_8859_9  = ISO8859 9
REG_MS_1251 = Microsoft CP1251
REG_8859_13 = ISO8859 13
REG_8859_15 = ISO8859 15
REG_8859_16 = ISO8859 16
REG_MS_1255 = Microsoft CP1255
REG_IBM_437 = IBM CP437
REG_KOI8_R  = KOI8 R
REG_KOI8_U  = KOI8 U
REG_BG_MIK  = Bulgarian MIK
REG_PT_154  = Paratype PT154
REG_XOS4_2  = XOS4 2
REG_10646_1 = ISO10646 1

PSF_8859_1  = ter-112n.psf ter-114n.psf ter-114b.psf ter-116n.psf ter-116b.psf ter-118n.psf ter-118b.psf ter-120n.psf ter-120b.psf ter-122n.psf ter-122b.psf ter-124n.psf ter-124b.psf ter-128n.psf ter-128b.psf ter-132n.psf ter-132b.psf
PSF_8859_2  = ter-212n.psf ter-214n.psf ter-214b.psf ter-216n.psf ter-216b.psf ter-218n.psf ter-218b.psf ter-220n.psf ter-220b.psf ter-222n.psf ter-222b.psf ter-224n.psf ter-224b.psf ter-228n.psf ter-228b.psf ter-232n.psf ter-232b.psf
PSF_8859_7  = ter-712n.psf ter-714n.psf ter-714b.psf ter-716n.psf ter-716b.psf ter-718n.psf ter-718b.psf ter-720n.psf ter-720b.psf ter-722n.psf ter-722b.psf ter-724n.psf ter-724b.psf ter-728n.psf ter-728b.psf ter-732n.psf ter-732b.psf
PSF_8859_9  = ter-912n.psf ter-914n.psf ter-914b.psf ter-916n.psf ter-916b.psf ter-918n.psf ter-918b.psf ter-920n.psf ter-920b.psf ter-922n.psf ter-922b.psf ter-924n.psf ter-924b.psf ter-928n.psf ter-928b.psf ter-932n.psf ter-932b.psf
PSF_MS_1251 = ter-c12n.psf ter-c14n.psf ter-c14b.psf ter-c16n.psf ter-c16b.psf ter-c18n.psf ter-c18b.psf ter-c20n.psf ter-c20b.psf ter-c22n.psf ter-c22b.psf ter-c24n.psf ter-c24b.psf ter-c28n.psf ter-c28b.psf ter-c32n.psf ter-c32b.psf
PSF_8859_13 = ter-d12n.psf ter-d14n.psf ter-d14b.psf ter-d16n.psf ter-d16b.psf ter-d18n.psf ter-d18b.psf ter-d20n.psf ter-d20b.psf ter-d22n.psf ter-d22b.psf ter-d24n.psf ter-d24b.psf ter-d28n.psf ter-d28b.psf ter-d32n.psf ter-d32b.psf
PSF_8859_16 = ter-g12n.psf ter-g14n.psf ter-g14b.psf ter-g16n.psf ter-g16b.psf ter-g18n.psf ter-g18b.psf ter-g20n.psf ter-g20b.psf ter-g22n.psf ter-g22b.psf ter-g24n.psf ter-g24b.psf ter-g28n.psf ter-g28b.psf ter-g32n.psf ter-g32b.psf
PSF_MS_1255 = ter-h12n.psf ter-h14n.psf ter-h14b.psf ter-h16n.psf ter-h16b.psf ter-h18n.psf ter-h18b.psf ter-h20n.psf ter-h20b.psf ter-h22n.psf ter-h22b.psf ter-h24n.psf ter-h24b.psf ter-h28n.psf ter-h28b.psf ter-h32n.psf ter-h32b.psf
PSF_IBM_437 = ter-i12n.psf ter-i14n.psf ter-i14b.psf ter-i16n.psf ter-i16b.psf ter-i18n.psf ter-i18b.psf ter-i20n.psf ter-i20b.psf ter-i22n.psf ter-i22b.psf ter-i24n.psf ter-i24b.psf ter-i28n.psf ter-i28b.psf ter-i32n.psf ter-i32b.psf
PSF_KOI8_RV = ter-k14n.psf ter-k14b.psf ter-k16n.psf ter-k16b.psf
PSF_KOI8_R  = ter-k12n.psf ter-k18n.psf ter-k18b.psf ter-k20n.psf ter-k20b.psf ter-k22n.psf ter-k22b.psf ter-k24n.psf ter-k24b.psf ter-k28n.psf ter-k28b.psf ter-k32n.psf ter-k32b.psf
PSF_BG_MIK  = ter-m12n.psf ter-m14n.psf ter-m14b.psf ter-m16n.psf ter-m16b.psf ter-m18n.psf ter-m18b.psf ter-m20n.psf ter-m20b.psf ter-m22n.psf ter-m22b.psf ter-m24n.psf ter-m24b.psf ter-m28n.psf ter-m28b.psf ter-m32n.psf ter-m32b.psf
PSF_PT_154  = ter-p12n.psf ter-p14n.psf ter-p14b.psf ter-p16n.psf ter-p16b.psf ter-p18n.psf ter-p18b.psf ter-p20n.psf ter-p20b.psf ter-p22n.psf ter-p22b.psf ter-p24n.psf ter-p24b.psf ter-p28n.psf ter-p28b.psf ter-p32n.psf ter-p32b.psf
PSF_KOI8_UV = ter-u14n.psf ter-u14b.psf ter-u16n.psf ter-u16b.psf
PSF_KOI8_U  = ter-u12n.psf ter-u18n.psf ter-u18b.psf ter-u20n.psf ter-u20b.psf ter-u22n.psf ter-u22b.psf ter-u24n.psf ter-u24b.psf ter-u28n.psf ter-u28b.psf ter-u32n.psf ter-u32b.psf
PSF_XOS4_2  = ter-v12n.psf ter-v14n.psf ter-v14b.psf ter-v16n.psf ter-v16b.psf ter-v18n.psf ter-v18b.psf ter-v20n.psf ter-v20b.psf ter-v22n.psf ter-v22b.psf ter-v24n.psf ter-v24b.psf ter-v28n.psf ter-v28b.psf ter-v32n.psf ter-v32b.psf
PSF = $(PSF_8859_1) $(PSF_8859_2) $(PSF_8859_7) $(PSF_8859_9) $(PSF_MS_1251) $(PSF_8859_13) $(PSF_8859_16) $(PSF_MS_1255) $(PSF_IBM_437) $(PSF_KOI8_RV) $(PSF_KOI8_R) $(PSF_BG_MIK) $(PSF_PT_154) $(PSF_KOI8_UV) $(PSF_KOI8_U) $(PSF_XOS4_2)

PSF_VGAW_8859_1  = ter-114v.psf ter-116v.psf
PSF_VGAW_8859_2  = ter-214v.psf ter-216v.psf
PSF_VGAW_8859_7  = ter-714v.psf ter-716v.psf
PSF_VGAW_8859_9  = ter-914v.psf ter-916v.psf
PSF_VGAW_MS_1251 = ter-c14v.psf ter-c16v.psf
PSF_VGAW_8859_13 = ter-d14v.psf ter-d16v.psf
PSF_VGAW_8859_16 = ter-g14v.psf ter-g16v.psf
PSF_VGAW_MS_1255 = ter-h14v.psf ter-h16v.psf
PSF_VGAW_IBM_437 = ter-i14v.psf ter-i16v.psf
PSF_VGAW_KOI8_RV = ter-k14v.psf ter-k16v.psf
PSF_VGAW_BG_MIK  = ter-m14v.psf ter-m16v.psf
PSF_VGAW_PT_154  = ter-p14v.psf ter-p16v.psf
PSF_VGAW_KOI8_UV = ter-u14v.psf ter-u16v.psf
PSF_VGAW_XOS4_2  = ter-v14v.psf ter-v16v.psf
PSF_VGAW = $(PSF_VGAW_8859_1) $(PSF_VGAW_8859_2) $(PSF_VGAW_8859_7) $(PSF_VGAW_8859_9) $(PSF_VGAW_MS_1251) $(PSF_VGAW_8859_13) $(PSF_VGAW_8859_16) $(PSF_VGAW_MS_1255) $(PSF_VGAW_IBM_437) $(PSF_VGAW_KOI8_RV) $(PSF_VGAW_BG_MIK) $(PSF_VGAW_PT_154) $(PSF_VGAW_KOI8_UV) $(PSF_VGAW_XOS4_2)

PCF_8859_1  = ter-112n.pcf ter-112b.pcf ter-114n.pcf ter-114b.pcf ter-116n.pcf ter-116b.pcf ter-118n.pcf ter-118b.pcf ter-120n.pcf ter-120b.pcf ter-122n.pcf ter-122b.pcf ter-124n.pcf ter-124b.pcf ter-128n.pcf ter-128b.pcf ter-132n.pcf ter-132b.pcf
PCF_8859_2  = ter-212n.pcf ter-212b.pcf ter-214n.pcf ter-214b.pcf ter-216n.pcf ter-216b.pcf ter-218n.pcf ter-218b.pcf ter-220n.pcf ter-220b.pcf ter-222n.pcf ter-222b.pcf ter-224n.pcf ter-224b.pcf ter-228n.pcf ter-228b.pcf ter-232n.pcf ter-232b.pcf
PCF_8859_5  = ter-512n.pcf ter-512b.pcf ter-514n.pcf ter-514b.pcf ter-516n.pcf ter-516b.pcf ter-518n.pcf ter-518b.pcf ter-520n.pcf ter-520b.pcf ter-522n.pcf ter-522b.pcf ter-524n.pcf ter-524b.pcf ter-528n.pcf ter-528b.pcf ter-532n.pcf ter-532b.pcf
PCF_8859_7  = ter-712n.pcf ter-712b.pcf ter-714n.pcf ter-714b.pcf ter-716n.pcf ter-716b.pcf ter-718n.pcf ter-718b.pcf ter-720n.pcf ter-720b.pcf ter-722n.pcf ter-722b.pcf ter-724n.pcf ter-724b.pcf ter-728n.pcf ter-728b.pcf ter-732n.pcf ter-732b.pcf
PCF_8859_9  = ter-912n.pcf ter-912b.pcf ter-914n.pcf ter-914b.pcf ter-916n.pcf ter-916b.pcf ter-918n.pcf ter-918b.pcf ter-920n.pcf ter-920b.pcf ter-922n.pcf ter-922b.pcf ter-924n.pcf ter-924b.pcf ter-928n.pcf ter-928b.pcf ter-932n.pcf ter-932b.pcf
PCF_MS_1251 = ter-c12n.pcf ter-c12b.pcf ter-c14n.pcf ter-c14b.pcf ter-c16n.pcf ter-c16b.pcf ter-c18n.pcf ter-c18b.pcf ter-c20n.pcf ter-c20b.pcf ter-c22n.pcf ter-c22b.pcf ter-c24n.pcf ter-c24b.pcf ter-c28n.pcf ter-c28b.pcf ter-c32n.pcf ter-c32b.pcf
PCF_8859_13 = ter-d12n.pcf ter-d12b.pcf ter-d14n.pcf ter-d14b.pcf ter-d16n.pcf ter-d16b.pcf ter-d18n.pcf ter-d18b.pcf ter-d20n.pcf ter-d20b.pcf ter-d22n.pcf ter-d22b.pcf ter-d24n.pcf ter-d24b.pcf ter-d28n.pcf ter-d28b.pcf ter-d32n.pcf ter-d32b.pcf
PCF_8859_15 = ter-f12n.pcf ter-f12b.pcf ter-f14n.pcf ter-f14b.pcf ter-f16n.pcf ter-f16b.pcf ter-f18n.pcf ter-f18b.pcf ter-f20n.pcf ter-f20b.pcf ter-f22n.pcf ter-f22b.pcf ter-f24n.pcf ter-f24b.pcf ter-f28n.pcf ter-f28b.pcf ter-f32n.pcf ter-f32b.pcf
PCF_8859_16 = ter-g12n.pcf ter-g12b.pcf ter-g14n.pcf ter-g14b.pcf ter-g16n.pcf ter-g16b.pcf ter-g18n.pcf ter-g18b.pcf ter-g20n.pcf ter-g20b.pcf ter-g22n.pcf ter-g22b.pcf ter-g24n.pcf ter-g24b.pcf ter-g28n.pcf ter-g28b.pcf ter-g32n.pcf ter-g32b.pcf
PCF_IBM_437 = ter-i12n.pcf ter-i12b.pcf ter-i14n.pcf ter-i14b.pcf ter-i16n.pcf ter-i16b.pcf ter-i18n.pcf ter-i18b.pcf ter-i20n.pcf ter-i20b.pcf ter-i22n.pcf ter-i22b.pcf ter-i24n.pcf ter-i24b.pcf ter-i28n.pcf ter-i28b.pcf ter-i32n.pcf ter-i32b.pcf
PCF_KOI8_R  = ter-k12n.pcf ter-k12b.pcf ter-k14n.pcf ter-k14b.pcf ter-k16n.pcf ter-k16b.pcf ter-k18n.pcf ter-k18b.pcf ter-k20n.pcf ter-k20b.pcf ter-k22n.pcf ter-k22b.pcf ter-k24n.pcf ter-k24b.pcf ter-k28n.pcf ter-k28b.pcf ter-k32n.pcf ter-k32b.pcf
PCF_PT_154  = ter-p12n.pcf ter-p12b.pcf ter-p14n.pcf ter-p14b.pcf ter-p16n.pcf ter-p16b.pcf ter-p18n.pcf ter-p18b.pcf ter-p20n.pcf ter-p20b.pcf ter-p22n.pcf ter-p22b.pcf ter-p24n.pcf ter-p24b.pcf ter-p28n.pcf ter-p28b.pcf ter-p32n.pcf ter-p32b.pcf
PCF_KOI8_U  = ter-u12n.pcf ter-u12b.pcf ter-u14n.pcf ter-u14b.pcf ter-u16n.pcf ter-u16b.pcf ter-u18n.pcf ter-u18b.pcf ter-u20n.pcf ter-u20b.pcf ter-u22n.pcf ter-u22b.pcf ter-u24n.pcf ter-u24b.pcf ter-u28n.pcf ter-u28b.pcf ter-u32n.pcf ter-u32b.pcf
PCF_10646_1 = ter-x12n.pcf ter-x12b.pcf ter-x14n.pcf ter-x14b.pcf ter-x16n.pcf ter-x16b.pcf ter-x18n.pcf ter-x18b.pcf ter-x20n.pcf ter-x20b.pcf ter-x22n.pcf ter-x22b.pcf ter-x24n.pcf ter-x24b.pcf ter-x28n.pcf ter-x28b.pcf ter-x32n.pcf ter-x32b.pcf
PCF_8BIT = $(PCF_8859_1) $(PCF_8859_2) $(PCF_8859_5) $(PCF_8859_7) $(PCF_8859_9) $(PCF_MS_1251) $(PCF_8859_13) $(PCF_8859_15) $(PCF_8859_16) $(PCF_IBM_437) $(PCF_KOI8_R) $(PCF_PT_154) $(PCF_KOI8_U)
PCF = $(PCF_10646_1)

OTB = ter-u12n.otb ter-u12b.otb ter-u14n.otb ter-u14b.otb ter-u16n.otb ter-u16b.otb ter-u18n.otb ter-u18b.otb ter-u20n.otb ter-u20b.otb ter-u22n.otb ter-u22b.otb ter-u24n.otb ter-u24b.otb ter-u28n.otb ter-u28b.otb ter-u32n.otb ter-u32b.otb

# Default

all: $(PSF) $(PCF)

DESTDIR =
prefix  = /usr/local
psfdir  = $(prefix)/share/consolefonts
x11dir  = $(prefix)/share/fonts/terminus
otbdir  = $(prefix)/share/fonts/terminus

install: $(PSF) $(PCF)
	mkdir -p $(DESTDIR)$(psfdir)
	for i in $(PSF) ; do gzip -c $$i > $(DESTDIR)$(psfdir)/$$i.gz ; done
	mkdir -p $(DESTDIR)$(x11dir)
	for i in $(PCF) ; do gzip -c $$i > $(DESTDIR)$(x11dir)/$$i.gz ; done

uninstall:
	for i in $(PSF) ; do rm -f $(DESTDIR)$(psfdir)/$$i.gz ; done
	for i in $(PCF) ; do rm -f $(DESTDIR)$(x11dir)/$$i.gz ; done

fontdir:
	mkfontscale $(DESTDIR)$(x11dir)
	mkfontdir $(DESTDIR)$(x11dir)
	fc-cache -f $(DESTDIR)$(x11dir)

# Linux Console

VGA_8859_1  = uni/vgagr.uni uni/ascii-h.uni uni/win-1252.uni
VGA_8859_2  = uni/vgagr.uni uni/ascii-h.uni uni/vga-1250.uni uni/8859-2.uni
VGA_8859_7  = uni/vgagr.uni uni/ascii-h.uni uni/vga-1253.uni uni/8859-7.uni
VGA_8859_9  = uni/vgagr.uni uni/ascii-h.uni uni/win-1254.uni
VGA_MS_1251 = uni/vgagr.uni uni/ascii-h.uni uni/vga-1251.uni uni/win-1251.uni
VGA_8859_13 = uni/vgagr.uni uni/ascii-h.uni uni/vga-1257.uni uni/8859-13.uni
VGA_8859_16 = uni/vgagr.uni uni/ascii-h.uni uni/nls-1250.uni uni/8859-16.uni
VGA_MS_1255 = uni/vgagr.uni uni/ascii-h.uni uni/win-1255.uni
VGA_IBM_437 = uni/cntrl.uni uni/ascii-h.uni uni/ibm-437.uni
VGA_KOI8_RV = uni/cntrl.uni uni/ascii-h.uni uni/koibm8-r.uni
VGA_KOI8_R  = uni/cntrl.uni uni/ascii-h.uni uni/koi8-r.uni
VGA_BG_MIK  = uni/cntrl.uni uni/ascii-h.uni uni/bg-mik.uni
VGA_PT_154  = uni/vgagr.uni uni/ascii-h.uni uni/pt-154.uni
VGA_KOI8_UV = uni/cntrl.uni uni/ascii-h.uni uni/koibm8-u.uni
VGA_KOI8_U  = uni/cntrl.uni uni/ascii-h.uni uni/koi8-u.uni
VGA_XOS4_2  = uni/xos4-2.uni

DUP_8859_1  = dup/vgagr.dup dup/ascii-h.dup
DUP_8859_2  = dup/vgagr.dup dup/ascii-h.dup
DUP_8859_7  = dup/vgagr.dup dup/ascii-h.dup
DUP_8859_9  = dup/vgagr.dup dup/ascii-h.dup
DUP_MS_1251 = dup/vgagr.dup dup/ascii-h.dup
DUP_8859_13 = dup/vgagr.dup dup/ascii-h.dup
DUP_8859_16 = dup/vgagr.dup dup/ascii-h.dup
DUP_MS_1255 = dup/vgagr.dup dup/ascii-h.dup
DUP_IBM_437 = dup/cntrl.dup dup/ascii-h.dup dup/ibm-437.dup
DUP_KOI8_RV = dup/cntrl.dup dup/ascii-h.dup dup/koi8.dup
DUP_KOI8_R  = dup/cntrl.dup dup/ascii-h.dup dup/koi8.dup
DUP_BG_MIK  = dup/cntrl.dup dup/ascii-h.dup dup/ibm-437.dup
DUP_PT_154  = dup/vgagr.dup dup/ascii-h.dup
DUP_KOI8_UV = dup/cntrl.dup dup/ascii-h.dup dup/koi8.dup
DUP_KOI8_U  = dup/cntrl.dup dup/ascii-h.dup dup/koi8.dup
DUP_XOS4_2  = dup/vgagr.dup dup/xos4-2.dup

$(PSF_8859_1) $(PSF_VGAW_8859_1): ter-1%.psf : ter-u%.bdf $(VGA_8859_1) $(DUP_8859_1)
	$(UCS2ANY) $< $(REG_8859_1) $(VGA_8859_1) | $(BDF2PSF) -o $@ $(DUP_8859_1)

$(PSF_8859_2) $(PSF_VGAW_8859_2): ter-2%.psf : ter-u%.bdf $(VGA_8859_2) $(DUP_8859_2)
	$(UCS2ANY) $< $(REG_8859_2) $(VGA_8859_2) | $(BDF2PSF) -o $@ $(DUP_8859_2)

$(PSF_8859_7) $(PSF_VGAW_8859_7): ter-7%.psf : ter-u%.bdf $(VGA_8859_7) $(DUP_8859_7)
	$(UCS2ANY) $< $(REG_8859_7) $(VGA_8859_7) | $(BDF2PSF) -o $@ $(DUP_8859_7)

$(PSF_8859_9) $(PSF_VGAW_8859_9): ter-9%.psf : ter-u%.bdf $(VGA_8859_9) $(DUP_8859_9)
	$(UCS2ANY) $< $(REG_8859_9) $(VGA_8859_9) | $(BDF2PSF) -o $@ $(DUP_8859_9)

$(PSF_MS_1251) $(PSF_VGAW_MS_1251): ter-c%.psf : ter-u%.bdf $(VGA_MS_1251) $(DUP_MS_1251)
	$(UCS2ANY) $< $(REG_MS_1251) $(VGA_MS_1251) | $(BDF2PSF) -o $@ $(DUP_MS_1251)

$(PSF_8859_13) $(PSF_VGAW_8859_13): ter-d%.psf : ter-u%.bdf $(VGA_8859_13) $(DUP_8859_13)
	$(UCS2ANY) $< $(REG_8859_13) $(VGA_8859_13) | $(BDF2PSF) -o $@ $(DUP_8859_13)

$(PSF_8859_16) $(PSF_VGAW_8859_16): ter-g%.psf : ter-u%.bdf $(VGA_8859_16) $(DUP_8859_16)
	$(UCS2ANY) $< $(REG_8859_16) $(VGA_8859_16) | $(BDF2PSF) -o $@ $(DUP_8859_16)

$(PSF_MS_1255) $(PSF_VGAW_MS_1255): ter-h%.psf : ter-u%.bdf $(VGA_MS_1255) $(DUP_MS_1255)
	$(UCS2ANY) $< $(REG_MS_1255) $(VGA_MS_1255) | $(BDF2PSF) -o $@ $(DUP_MS_1255)

$(PSF_IBM_437) $(PSF_VGAW_IBM_437): ter-i%.psf : ter-u%.bdf $(VGA_IBM_437) $(DUP_IBM_437)
	$(UCS2ANY) $< $(REG_IBM_437) $(VGA_IBM_437) | $(BDF2PSF) -o $@ $(DUP_IBM_437)

$(PSF_KOI8_RV) $(PSF_VGAW_KOI8_RV): ter-k%.psf : ter-u%.bdf $(VGA_KOI8_RV) $(DUP_KOI8_RV)
	$(UCS2ANY) $< $(REG_KOI8_R) $(VGA_KOI8_RV) | $(BDF2PSF) -o $@ $(DUP_KOI8_RV)

$(PSF_KOI8_R): ter-k%.psf : ter-u%.bdf $(VGA_KOI8_R) $(DUP_KOI8_R)
	$(UCS2ANY) $< $(REG_KOI8_R) $(VGA_KOI8_R) | $(BDF2PSF) -o $@ $(DUP_KOI8_R)

$(PSF_BG_MIK) $(PSF_VGAW_BG_MIK): ter-m%.psf : ter-u%.bdf $(VGA_BG_MIK) $(DUP_BG_MIK)
	$(UCS2ANY) $< $(REG_BG_MIK) $(VGA_BG_MIK) | $(BDF2PSF) -o $@ $(DUP_BG_MIK)

$(PSF_PT_154) $(PSF_VGAW_PT_154): ter-p%.psf : ter-u%.bdf $(VGA_PT_154) $(DUP_PT_154)
	$(UCS2ANY) $< $(REG_PT_154) $(VGA_PT_154) | $(BDF2PSF) -o $@ $(DUP_PT_154)

$(PSF_KOI8_UV) $(PSF_VGAW_KOI8_UV): ter-u%.psf : ter-u%.bdf $(VGA_KOI8_UV) $(DUP_KOI8_UV)
	$(UCS2ANY) $< $(REG_KOI8_R) $(VGA_KOI8_UV) | $(BDF2PSF) -o $@ $(DUP_KOI8_UV)

$(PSF_KOI8_U): ter-u%.psf : ter-u%.bdf $(VGA_KOI8_U) $(DUP_KOI8_U)
	$(UCS2ANY) $< $(REG_KOI8_U) $(VGA_KOI8_U) | $(BDF2PSF) -o $@ $(DUP_KOI8_U)

$(PSF_XOS4_2) $(PSF_VGAW_XOS4_2): ter-v%.psf : ter-u%.bdf $(VGA_XOS4_2) $(DUP_XOS4_2)
	$(UCS2ANY) $< $(REG_XOS4_2) $(VGA_XOS4_2) | $(BDF2PSF) -o $@ $(DUP_XOS4_2)

psf: $(PSF)

install-psf: $(PSF)
	mkdir -p $(DESTDIR)$(psfdir)
	for i in $(PSF) ; do gzip -c $$i > $(DESTDIR)$(psfdir)/$$i.gz ; done

uninstall-psf:
	for i in $(PSF) ; do rm -f $(DESTDIR)$(psfdir)/$$i.gz ; done

psf-vgaw: $(PSF_VGAW)

install-psf-vgaw: $(PSF_VGAW)
	mkdir -p $(DESTDIR)$(psfdir)
	for i in $(PSF_VGAW) ; do gzip -c $$i > $(DESTDIR)$(psfdir)/$$i.gz ; done

uninstall-psf-vgaw:
	for i in $(PSF_VGAW) ; do rm -f $(DESTDIR)$(psfdir)/$$i.gz ; done

psfref = $(psfdir)/README.terminus

install-psf-ref: README
	mkdir -p $(DESTDIR)$(psfdir)
	sed -e"/^2\.4/,/^2\.5/p" -n README | grep -v "^2\." > $(DESTDIR)$(psfref)

uninstall-psf-ref:
	rm -f $(DESTDIR)$(psfref)

# X11 Window System

X11_8859_1  = uni/x11gr.uni uni/ascii-h.uni uni/win-1252.uni
X11_8859_2  = uni/x11gr.uni uni/ascii-h.uni uni/empty.uni uni/8859-2.uni
X11_8859_5  = uni/x11gr.uni uni/ascii-h.uni uni/empty.uni uni/8859-5.uni
X11_8859_7  = uni/x11gr.uni uni/ascii-h.uni uni/empty.uni uni/8859-7.uni
X11_8859_9  = uni/x11gr.uni uni/ascii-h.uni uni/win-1254.uni
X11_MS_1251 = uni/x11gr.uni uni/ascii-h.uni uni/x11-1251.uni uni/win-1251.uni
X11_8859_13 = uni/x11gr.uni uni/ascii-h.uni uni/x11-1257.uni uni/8859-13.uni
X11_8859_15 = uni/x11gr.uni uni/ascii-h.uni uni/empty.uni uni/8859-15.uni
X11_8859_16 = uni/x11gr.uni uni/ascii-h.uni uni/empty.uni uni/8859-16.uni
X11_IBM_437 = uni/cntrl.uni uni/ascii-h.uni uni/ibm-437.uni
X11_KOI8_R  = uni/x11gr.uni uni/ascii-h.uni uni/koi8-r.uni
X11_PT_154  = uni/x11gr.uni uni/ascii-h.uni uni/pt-154.uni
X11_KOI8_U  = uni/x11gr.uni uni/ascii-h.uni uni/koi8-u.uni
X11_10646_1 = uni/x11gr.uni uni/10646-1.uni

$(PCF_8859_1): ter-1%.pcf : ter-u%.bdf $(X11_8859_1)
	$(UCS2X11) $< $(REG_8859_1) $(X11_8859_1) | $(BDF2PCF) -o $@

$(PCF_8859_2): ter-2%.pcf : ter-u%.bdf $(X11_8859_2)
	$(UCS2X11) $< $(REG_8859_2) $(X11_8859_2) | $(BDF2PCF) -o $@

$(PCF_8859_5): ter-5%.pcf : ter-u%.bdf $(X11_8859_5)
	$(UCS2X11) $< $(REG_8859_5) $(X11_8859_5) | $(BDF2PCF) -o $@

$(PCF_8859_7): ter-7%.pcf : ter-u%.bdf $(X11_8859_7)
	$(UCS2X11) $< $(REG_8859_7) $(X11_8859_7) | $(BDF2PCF) -o $@

$(PCF_8859_9): ter-9%.pcf : ter-u%.bdf $(X11_8859_9)
	$(UCS2X11) $< $(REG_8859_9) $(X11_8859_9) | $(BDF2PCF) -o $@

$(PCF_MS_1251): ter-c%.pcf : ter-u%.bdf $(X11_MS_1251)
	$(UCS2X11) $< $(REG_MS_1251) $(X11_MS_1251) | $(BDF2PCF) -o $@

$(PCF_8859_13): ter-d%.pcf : ter-u%.bdf $(X11_8859_13)
	$(UCS2X11) $< $(REG_8859_13) $(X11_8859_13) | $(BDF2PCF) -o $@

$(PCF_8859_15): ter-f%.pcf : ter-u%.bdf $(X11_8859_15)
	$(UCS2X11) $< $(REG_8859_15) $(X11_8859_15) | $(BDF2PCF) -o $@

$(PCF_8859_16): ter-g%.pcf : ter-u%.bdf $(X11_8859_16)
	$(UCS2X11) $< $(REG_8859_16) $(X11_8859_16) | $(BDF2PCF) -o $@

$(PCF_IBM_437): ter-i%.pcf : ter-u%.bdf $(X11_IBM_437)
	$(UCS2X11) $< $(REG_IBM_437) $(X11_IBM_437) | $(BDF2PCF) -o $@

$(PCF_KOI8_R): ter-k%.pcf : ter-u%.bdf $(X11_KOI8_R)
	$(UCS2X11) $< $(REG_KOI8_R) $(X11_KOI8_R) | $(BDF2PCF) -o $@

$(PCF_PT_154): ter-p%.pcf : ter-u%.bdf $(X11_PT_154)
	$(UCS2X11) $< $(REG_PT_154) $(X11_PT_154) | $(BDF2PCF) -o $@

$(PCF_KOI8_U): ter-u%.pcf : ter-u%.bdf $(X11_KOI8_U)
	$(UCS2X11) $< $(REG_KOI8_U) $(X11_KOI8_U) | $(BDF2PCF) -o $@

$(PCF_10646_1): ter-x%.pcf : ter-u%.bdf $(X11_10646_1)
	$(UCS2X11) $< $(REG_10646_1) $(X11_10646_1) | $(BDF2PCF) -o $@

pcf: $(PCF)

install-pcf: $(PCF)
	mkdir -p $(DESTDIR)$(x11dir)
	for i in $(PCF) ; do gzip -c $$i > $(DESTDIR)$(x11dir)/$$i.gz ; done

uninstall-pcf:
	for i in $(PCF) ; do rm -f $(DESTDIR)$(x11dir)/$$i.gz ; done

pcf-8bit: $(PCF_8BIT)

install-pcf-8bit: $(PCF_8BIT)
	mkdir -p $(DESTDIR)$(x11dir)
	for i in $(PCF_8BIT) ; do gzip -c $$i > $(DESTDIR)$(x11dir)/$$i.gz ; done

uninstall-pcf-8bit:
	for i in $(PCF_8BIT) ; do rm -f $(DESTDIR)$(x11dir)/$$i.gz ; done

# Open Type Bitmap

$(OTB): ter-u%.otb : ter-u%.bdf
	$(BDF2OTB) -o $@ $<

otb: $(OTB)

install-otb: $(OTB)
	mkdir -p $(DESTDIR)$(otbdir)
	cp -f $(OTB) $(DESTDIR)$(otbdir)

uninstall-otb:
	for i in $(OTB) ; do rm -f $(DESTDIR)$(otbdir)/$$i ; done

# Cleanup

clean:
	rm -f $(PSF) $(PSF_VGAW) $(PCF) $(PCF_8BIT) $(OTB)

.PHONY: all install uninstall fontdir psf install-psf uninstall-psf psf-vgaw install-psf-vgaw uninstall-psf-vgaw install-psf-ref uninstall-psf-ref pcf install-pcf uninstall-pcf pcf-8bit install-pcf-8bit uninstall-pcf-8bit otb install-otb uninstall-otb clean

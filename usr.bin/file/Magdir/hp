#
# magic.hp: Hewlett Packard Magic
#
# XXX - somebody should figure out whether any byte order needs to be
# applied to the "TML" stuff; I'm assuming the Apollo stuff is
# big-endian as it was mostly 68K-based.
#
# HP-PA is big-endian, so it (and "800", which is *also* HP-PA-based; I
# assume "HPPA-RISC1.1" really means "HP-PA Version 1.1", which first
# showed up in the 700 series, although later 800 series machines are,
# I think, based on the PA7100 which implements HP-PA 1.1) are flagged
# as big-endian.
#
# I think the 500 series was the old stack-based machines, running a
# UNIX environment atop the "SUN kernel"; dunno whether it was
# big-endian or little-endian.
#
# I'm guessing that the 200 series was 68K-based; the 300 and 400 series
# are.
#
# The "misc" stuff needs a byte order; the archives look suspiciously
# like the old 177545 archives (0xff65 = 0177545).
#
#### Old Apollo stuff
0	beshort		0627		Apollo m68k COFF executable
>18	beshort		^040000		not stripped 
>22	beshort		>0		- version %ld
0	beshort		0624		apollo a88k COFF executable
>18	beshort		^040000		not stripped 
>22	beshort		>0		- version %ld
0       long            01203604016     TML 0123 byte-order format
0       long            01702407010     TML 1032 byte-order format
0       long            01003405017     TML 2301 byte-order format
0       long            01602007412     TML 3210 byte-order format
#### HPPA
0	belong 		0x02100106	HPPA-RISC1.1 relocatable object
0	belong 		0x02100107	HPPA-RISC1.1 executable
>(144)	belong		0x054ef630	dynamically linked
>96	belong		>0		-not stripped

0	belong 		0x02100108	HPPA-RISC1.1 shared executable
>(144)	belong		0x054ef630	dynamically linked
>96	belong		>0		-not stripped

0	belong 		0x0210010b	HPPA-RISC1.1 demand-load executable
>(144)	belong		0x054ef630	dynamically linked
>96	belong		>0		-not stripped

0	belong 		0x0210010e	HPPA-RISC1.1 shared library
>96	belong		>0		-not stripped

0	belong 		0x0210010d	HPPA-RISC1.1 dynamic load library
>96	belong		>0		-not stripped

#### 800
0	belong 		0x020b0106	HP s800 relocatable object

0	belong 		0x020b0107	HP s800 executable
>(144)	belong		0x054ef630	dynamically linked
>96	belong		>0		-not stripped

0	belong 		0x020b0108	HP s800 shared executable
>(144)	belong		0x054ef630	dynamically linked
>96	belong		>0		-not stripped

0	belong 		0x020b010b	HP s800 demand-load executable
>(144)	belong		0x054ef630	dynamically linked
>96	belong		>0		-not stripped

0	belong 		0x020b010e	HP s800 shared library
>96	belong		>0		-not stripped

0	belong 		0x020b010d	HP s800 dynamic load library
>96	belong		>0		-not stripped

0	belong		0x213c6172	archive file
>68	belong 		0x020b0619	-HP s800 relocatable library

#### 500
0	long		0x02080106	HP s500 relocatable executable
>16	long		>0		-version %ld

0	long		0x02080107	HP s500 executable
>16	long		>0		-version %ld

0	long		0x02080108	HP s500 pure executable
>16	long		>0		-version %ld

#### 200
0	belong 		0x020c0108	HP s200 pure executable
>4	beshort		>0		-version %ld
>8	belong		&0x80000000	save fp regs
>8	belong		&0x40000000	dynamically linked
>8	belong		&0x20000000	debuggable
>36	belong		>0		not stripped

0	belong		0x020c0107	HP s200 executable
>4	beshort		>0		-version %ld
>8	belong		&0x80000000	save fp regs
>8	belong		&0x40000000	dynamically linked
>8	belong		&0x20000000	debuggable
>36	belong		>0		not stripped

0	belong		0x020c010b	HP s200 demand-load executable
>4	beshort		>0		-version %ld
>8	belong		&0x80000000	save fp regs
>8	belong		&0x40000000	dynamically linked
>8	belong		&0x20000000	debuggable
>36	belong		>0		not stripped

0	belong		0x020c0106	HP s200 relocatable executable
>4	beshort		>0		-version %ld
>6	beshort		>0		-highwater %d
>8	belong		&0x80000000	save fp regs
>8	belong		&0x20000000	debuggable
>8	belong		&0x10000000	PIC

0	belong 		0x020a0108	HP s200 (2.x release) pure executable
>4	beshort		>0		-version %ld
>36	belong		>0		not stripped

0	belong		0x020a0107	HP s200 (2.x release) executable
>4	beshort		>0		-version %ld
>36	belong		>0		not stripped

0	belong		0x020c010e	HP s200 shared library
>4	beshort		>0		-version %ld
>6	beshort		>0		-highwater %d
>36	belong		>0		not stripped

0	belong		0x020c010d	HP s200 dynamic load library
>4	beshort		>0		-version %ld
>6	beshort		>0		-highwater %d
>36	belong		>0		not stripped

#### MISC
0	long		0x0000ff65	HP old archive
0	long		0x020aff65	HP s200 old archive
0	long		0x020cff65	HP s200 old archive
0	long		0x0208ff65	HP s500 old archive

0	long		0x015821a6	HP core file

0	long		0x4da7eee8	HP-WINDOWS font
>8	byte		>0		-version %ld
0	string		Bitmapfile	HP Bitmapfile

0	string		IMGfile	CIS 	compimg HP Bitmapfile
0	short		0x8000		lif file
0	long		0x020c010c	compiled Lisp

0	string		msgcat01	HP NLS message catalog,
>8	long		>0		%d messages

# addendum to /etc/magic with HP-48sx file-types by phk@data.fls.dk 1jan92
0	string		HPHP48-		HP48 binary
>7	byte		>0		- Rev %c
>8	short		0x1129		(ADR)
>8	short		0x3329		(REAL)
>8	short		0x5529		(LREAL)
>8	short		0x7729		(COMPLX)
>8	short		0x9d29		(LCOMPLX)
>8	short		0xbf29		(CHAR)
>8	short		0xe829		(ARRAY)
>8	short		0x0a2a		(LNKARRAY)
>8	short		0x2c2a		(STRING)
>8	short		0x4e2a		(HXS)
>8	short		0x742a		(LIST)
>8	short		0x962a		(DIR)
>8	short		0xb82a		(ALG)
>8	short		0xda2a		(UNIT)
>8	short		0xfc2a		(TAGGED)
>8	short		0x1e2b		(GROB)
>8	short		0x402b		(LIB)
>8	short		0x622b		(BACKUP)
>8	short		0x882b		(LIBDATA)
>8	short		0x9d2d		(PROG)
>8	short		0xcc2d		(CODE)
>8	short		0x482e		(GNAME)
>8	short		0x6d2e		(LNAME)
>8	short		0x922e		(XLIB)
0	string		%%HP:		HP48 text
>6	string		T(0)		- T(0)
>6	string		T(1)		- T(1)
>6	string		T(2)		- T(2)
>6	string		T(3)		- T(3)
>10	string		A(D)		A(D)
>10	string		A(R)		A(R)
>10	string		A(G)		A(G)
>14	string		F(.)		F(.);
>14	string		F(,)		F(,);

SHLIB_MAJOR= 1
SHLIB_MINOR= 0

LIB=I77
CFLAGS+= -DSkip_f2c_Undefs -DNON_ANSI_RW_MODES

I77SRCS = Version.c backspace.c close.c dfe.c dolio.c due.c endfile.c err.c \
	fmt.c fmtlib.c iio.c ilnw.c inquire.c lread.c lwrite.c open.c \
	rdfmt.c rewind.c rsfe.c rsli.c rsne.c sfe.c sue.c typesize.c uio.c \
	util.c wref.c wrtfmt.c wsfe.c wsle.c wsne.c xwsne.c

SRCS= ${I77SRCS}

.include <bsd.lib.mk>

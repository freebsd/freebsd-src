#	@(#)Makefile	8.5 (Berkeley) 7/16/94

SRCS=	vi.1
DOCS=	vi.0 vi.0.ps

all: ${DOCS}

vi.0: vi.1
	groff -man -Tascii < vi.1 > $@
vi.0.ps: vi.1
	groff -man < vi.1 > $@

clean:
	rm -f ${DOCS}

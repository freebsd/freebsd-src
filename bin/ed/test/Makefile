# $FreeBSD: src/bin/ed/test/Makefile,v 1.8.2.1 1999/08/29 14:12:30 peter Exp $

SHELL=	/bin/sh
ED=	${.OBJDIR}/ed

all: check
	@:

check: build test
	@if grep -h '\*\*\*' errs.o scripts.o; then :; else \
		echo "tests completed successfully."; \
	fi

build: mkscripts.sh
	@if [ -f errs.o ]; then :; else \
		uudecode < ascii.d.uu ; \
		uudecode < ascii.r.uu ; \
		echo "building test scripts for $(ED) ..."; \
		$(SHELL) mkscripts.sh $(ED); \
	fi

test: build ckscripts.sh
	@echo testing $(ED) ...
	@$(SHELL) ckscripts.sh $(ED)

clean:
	rm -f *.ed *.red *.[oz] *~ ascii.d ascii.r

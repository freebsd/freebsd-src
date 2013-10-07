# $FreeBSD$

TESTS=	test-trimdomain test-trimdomain-nodomain test-flopen test-grp \
	test-pidfile test-humanize_number
CFLAGS+= -g -Wall -Wextra -Werror -lutil

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}

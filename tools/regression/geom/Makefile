# $FreeBSD: src/tools/regression/geom/Makefile,v 1.2 2002/05/19 18:57:00 phk Exp $

SUBDIR+=	ConfCmp GLib Test

.include <bsd.subdir.mk>

test:	all _SUBDIR


toflat:
	scp *.c *.h *.sh Makefile root@flat:/sys/geom

fromflat:
	scp root@flat:/sys/geom/\* .

publish:
	$(MAKE) cleandir
	tar --exclude CVS -czf - . | \
	ssh phk@phk "cd www/geom && cat > geom.tgz && rm -rf src && mkdir -p src && cd src && tar xzf ../geom.tgz"

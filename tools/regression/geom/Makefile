# $FreeBSD$

SUBDIR+=	ConfCmp GLib Test

.include <bsd.subdir.mk>

test:	all _SUBDIRUSE

toflat:
	scp *.c *.h *.sh Makefile root@flat:/sys/geom

fromflat:
	scp root@flat:/sys/geom/\* .

publish:
	$(MAKE) cleandir
	tar --exclude CVS -czf - . | \
	ssh phk@phk "cd www/geom && cat > geom.tgz && rm -rf src && mkdir -p src && cd src && tar xzf ../geom.tgz"

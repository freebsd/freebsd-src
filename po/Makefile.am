
noinst_SCRIPTS = gen-po.sh

gen-po.sh: gen-po.in
	sed \
	  -e 's,[@]top_srcdir[@],$(top_srcdir),' \
	  -e 's,[@]PACKAGE_NAME[@],$(PACKAGE_NAME),' \
	  -e 's,[@]PACKAGE_VERSION[@],$(PACKAGE_VERSION),' \
	  < $(srcdir)/gen-po.in > gen-po.tmp
	chmod +x gen-po.tmp
	mv gen-po.tmp gen-po.sh

ceprefix=heim_com_err

po: gen-po.sh
	./gen-po.sh heimdal_krb5 $(top_srcdir)/lib/krb5/*.[chly]
	./gen-po.sh heimdal_kuser $(top_srcdir)/kuser/*.[ch]
	find $(top_srcdir) -name *.et | while read x; do \
		y=$$(basename $$x); \
		echo $$y ; \
		z=$$(echo $$y | sed 's/\.et$$//') ; \
		t=$$(find $(top_builddir) -name $$z.c) ; \
		t=$$(echo $$t | sed 's/\.c$$//') ; \
		base=$$(grep 'ERROR_TABLE_BASE_' $${t}.h | cut -f3 -d' ') ; \
		./gen-po.sh $(ceprefix)$$base $${t}.c ; \
		test -f $(top_srcdir)/po/$(ceprefix)$$base/$(ceprefix)$$base.pot || { echo "$$y missing" ; exit 1; } \
	done

mo:
	cd $(srcdir) ; \
	rm localefiles ; \
	touch localefiles ; \
	find . -name '*.po' | while read s ; do \
	   t=$$(echo $$s | sed -e 's/\.po$$/.mo/') ; \
	   msgfmt -o $$t $$s ; \
	   echo $$t | sed 's@\./@@' >> localefiles ; \
	done

install-data-hook:
	@for x in `cat $(srcdir)/localefiles` ; do \
	    domain=`echo $$x | sed 's@/.*@@'`; \
	    lang=`echo $$x | sed 's@.*/\(.*\)\\.mo$$@\1@'`; \
	    echo "installing lang $$domain $$lang" ; \
	    $(top_srcdir)/install-sh -d \
		"$(DESTDIR)$(localedir)/$$lang/LC_MESSAGES" ; \
	    $(top_srcdir)/install-sh $(srcdir)/$$x \
	        "$(DESTDIR)$(localedir)/$$lang/LC_MESSAGES/$$domain.mo" ; \
	done

#find . -name '*.po' -or -name '*.pot' -or -name '*.mo' | sed 's/^../    /' | sed 's/$/ \\/'
FILES = \
    heim_com_err-1750206208/heim_com_err-1750206208.pot \
    heim_com_err-1765328384/heim_com_err-1765328384.pot \
    heim_com_err-1765328384/sv_SE.mo \
    heim_com_err-1765328384/sv_SE.po \
    heim_com_err-1980176640/heim_com_err-1980176640.pot \
    heim_com_err-969269760/heim_com_err-969269760.pot \
    heim_com_err1859794432/heim_com_err1859794432.pot \
    heim_com_err35224064/heim_com_err35224064.pot \
    heim_com_err36150272/heim_com_err36150272.pot \
    heim_com_err39525376/heim_com_err39525376.pot \
    heim_com_err43787520/heim_com_err43787520.pot \
    heim_com_err569856/heim_com_err569856.pot \
    heimdal_kuser/heimdal_kuser.pot \
    heimdal_krb5/heimdal_krb5.pot \
    heimdal_krb5/sv_SE.mo \
    heimdal_krb5/sv_SE.po

EXTRA_DIST = gen-po.in $(FILES)
CLEANFILES = gen-po.tmp $(noinst_SCRIPTS) heimdal-pot.tar.gz

launchpad:
	tar czCf \
		$(top_srcdir) \
		$(top_builddir)/po/heimdal-pot.tar.gz \
		$$(cd $(top_srcdir) && find . -name '*.pot')

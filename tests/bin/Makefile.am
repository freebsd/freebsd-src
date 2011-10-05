include $(top_srcdir)/Makefile.am.common

noinst_SCRIPTS = setup-env

do_subst = \
	top_srcdir="$$(cd ${top_srcdir} && pwd)" ; \
	top_builddir="$$(cd ${top_builddir} && pwd)" ; \
	sed $(do_dlopen) \
	-e "s,[@]EGREP[@],$(EGREP),g" \
	-e "s,[@]top_srcdir[@],$${top_srcdir},g" \
	-e "s,[@]top_builddir[@],$${top_builddir},g" \
	-e "s,[@]NO_AFS[@],$(NO_AFS),g"

setup-env: setup-env.in Makefile
	$(do_subst) < $(srcdir)/setup-env.in > setup-env.tmp
	chmod +x setup-env.tmp
	mv setup-env.tmp setup-env

EXTRA_DIST = setup-env.in

CLEANFILES = setup-env setup-env.tmp


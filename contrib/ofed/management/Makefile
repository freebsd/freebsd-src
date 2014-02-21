
SUBDIRS:= libibcommon libibumad libibmad opensm infiniband-diags

all:

config:
	$(foreach dir, $(SUBDIRS), \
		if [ ! -z "$(force)" -o ! -x $(dir)/configure ] ; then \
			( cd $(dir) && ./autogen.sh && ./configure $(CONFIG_OPTS) ) \
				|| exit 1 ; \
		elif [ ! -e $(dir)/Makefile ] ; then \
			( cd $(dir) && ./configure $(CONFIG_OPTS) ) \
				|| exit 1 ; \
		fi ; )

automake: force=1
automake: config

all install: config
all install clean:
	$(foreach dir, $(SUBDIRS), $(MAKE) -C $(dir) $@ && ) echo $@ done

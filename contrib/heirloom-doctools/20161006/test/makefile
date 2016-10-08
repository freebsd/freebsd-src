DIRS=	\
	eqn \
	grap \
	pic \
	refer/32 \
	refer/ab \
	tbl

# should be .DEFAULT but doesn't work with GNU make
test:
	for i in $(DIRS); do \
		(cd $$i && $(MAKE) $@) || exit 1; \
	done

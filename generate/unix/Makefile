#
# Common make for acpica tools and utilities
#
include Makefile.config


all:	${DIRS}
${DIRS}: FORCE
	@cd $@; make; ls -al $@

clean:	FORCE
	@for d in ${DIRS}; do \
		(cd $$d; \
		if [ $$? -ne 0 ]; then \
			echo "Bad element of DIRS: <$$d>"; \
		else \
			pwd; make clean; \
		fi); \
	done

install:	FORCE
	@for d in ${DIRS}; do \
		(cd $$d; \
		if [ $$? -ne 0 ]; then \
			echo "Bad element of DIRS: <$$d>"; \
		else \
			pwd; make install; \
		fi); \
	done

FORCE:

#       @(#)Makefile.dist	8.2 (Berkeley) 2/17/98

SHELL= /bin/sh
SUBDIRS= src mail.local mailstats makemap praliases rmail smrsh
BUILD=   ./Build
OPTIONS= $(CONFIG) $(FLAGS)

all clean install:: FRC
	@for x in $(SUBDIRS); \
	do \
		(cd $$x; echo Making $@ in:; pwd; \
		$(SHELL) $(BUILD) $(OPTIONS) $@); \
	done

fresh:: FRC
	@for x in $(SUBDIRS); \
	do \
		(cd $$x; echo Making $@ in:; pwd; \
		$(SHELL) $(BUILD) $(OPTIONS) -c); \
	done

$(SUBDIRS):: FRC
	@cd $@; pwd; \
	$(SHELL) $(BUILD) $(OPTIONS)

FRC:

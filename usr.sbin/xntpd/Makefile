# WARNING:
#	CONTENTS UNDER PRESSURE.
#	HIGHLY FLAMMABLE.
#	RISK OF SHOCK.
#	DO NOT ATTEMPT TO OPEN COVER.
#	NO USER SERVICEABLE PARTS INSIDE.
#	REFER SERVICING TO QUALIFIED PERSONNEL.
#
#	The vendor hits you...
#	You try to hit the vendor...
#	You die.
#
# Unfortunately the above is no fun...
#
# During testing/porting we have found a long list
# of "make" and "sh" and "awk" features in different implementations.
# Some goodies (make good horror stories for your kids 8-():
#	gmake 3.62
#		non standard target construction
#
#	pmake (e. g. NetBSD on MAC, possible other BNR2+pmake systems)
#		skips '' (empty string positional) args to sh
#		(this leads to following stupid constructions
#		 sh -c "./scripts/makeconfig.sh '$(OS)' '$(COMP)'")
#
#		Following Makefile construction fails for no
#		apparent reason (at least to me)
#		doit:
#			$(MAKE) MAKE=\"$(MAKE)\" all
#
#		all:
#			@echo all done.
#
#		for the "make MAKE=make" call not for "make" or
#		"make -e MAKE=make". Use the last form if you suffer
#		from that kind of make problems. (Easily detected
#		by failure to build with the message:
#		"don't know how to make make".
#
#	sh (Ultrix 4.2 MIPS)
#		shell broken (reversed pipe construction "false | true"
#				 returns false - major bummer)
#
#	awk (EP/IX 2.?)
#		unable to do regexp matches
#		(aka awk '/..*/ { print; }' fails on match)
#
# Usually the vendor should fix these bugs in vital utilities.
# We try to circumvent these bugs in a hopefully portable way.
# If you can reproduce these bugs on your system please bug your
# vendor to fix them. We are not trying anything fancy in here and
# we are shocked that even the most common tools fail so miserably.
# By the time you get this code the above utilities may already
# have been fixed. Hopefully one day we do not have to cope with
# this kind of broken utilities.
#
# Sorry about the situation,
#	Frank Kardel
#
SHELL=/bin/sh
CONF =	Config
CONFL =	$(CONF).local
CONFLD=	$(CONFL).dist
TARGETS	= xntpd/xntpd xntpdc/xntpdc ntpq/ntpq ntpdate/ntpdate \
	  ntptrace/ntptrace xntpres/xntpres authstuff/authspeed util/tickadj
OPTTARG = adjtime/adjtimed util/ntptime util/precision
REFCONF=
COMPRESSOR=compress
# Base distribution name (will be extended by <VERSION>.tar.<compressorsuffix>)
DISTNAME=xntp-
MAKE=	make

all:	version $(TARGETS) kernel_modules

$(TARGETS): VERSION $(CONF) 

version:
	@echo '### Building XNTP:' "`egrep '^.*=.*$$' VERSION | tr '\012' ';'`"

makeconfig:
	sh -c "./scripts/makeconfig.sh '$(OS)' '$(COMP)'"

$(CONFL).NO.clock:
	@echo '###' creating $(CONFL) for absolutely '*NO*' clocks '*AT ALL*'
	rm -f $(CONFL)-t $(CONFL)
	cat < $(CONFLD) > $(CONFL)-t && mv $(CONFL)-t $(CONFL)

$(CONFL).green:
	@echo '###' creating $(CONFL) for greenhorns '(local refclock only)'
	rm -f $(CONFL)-t $(CONFL)
	sed 's/#GREEN//' < $(CONFLD) > $(CONFL)-t && mv $(CONFL)-t $(CONFL)

$(CONFL):
	@echo ''
	@echo '### creating a $(CONFL) file as none existed.'
	@echo '### Use "make refconf" if you have a radio clock'
	@echo ''
	@$(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" $(CONFL).green

$(CONF): $(CONFL)
	@echo
	@echo '###' creating new configuration
	@sh -c "./scripts/makeconfig.sh '$(OS)' '$(COMP)'"

refconf: $(CONF)
	-@sh refclocks/rconfig '$(REFCONF)'
	@sh -c "./scripts/makeconfig.sh '$(OS)' '$(COMP)'"

kernel_modules: kernel/Makefile
	@cd kernel && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" MAKE="$(MAKE)"

xntpd/xntpd:	lib/libntp.a parse/libparse.a xntpd/Makefile FRC
	@echo
	@echo '###' creating NTP daemon
	@cd xntpd && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" MAKE="$(MAKE)"

xntpdc/xntpdc:	lib/libntp.a xntpdc/Makefile FRC
	@echo
	@echo '###' creating XNTPDC utility
	@cd xntpdc && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" MAKE="$(MAKE)"

ntpq/ntpq:	lib/libntp.a ntpq/Makefile FRC
	@echo
	@echo '###' creating NTPQ utility
	@cd ntpq && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" MAKE="$(MAKE)"

ntptrace/ntptrace:	lib/libntp.a ntptrace/Makefile FRC
	@echo
	@echo '###' creating NTPTRACE utility
	@cd ntptrace && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" MAKE="$(MAKE)"

ntpdate/ntpdate: lib/libntp.a ntpdate/Makefile FRC
	@echo
	@echo '###' creating NTPDATE utility
	@cd ntpdate && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" MAKE="$(MAKE)"

authstuff/authspeed:	lib/libntp.a authstuff/Makefile FRC
	@echo
	@echo '###' creating AUTH utilities
	@cd authstuff && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" MAKE="$(MAKE)"

xntpres/xntpres:	lib/libntp.a xntpres/Makefile FRC
	@echo
	@echo '###' creating XNTPRES utility
	@cd xntpres && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" MAKE="$(MAKE)" 

util/tickadj: util/Makefile FRC
	@echo
	@echo '###' creating TICKADJ utility
	@cd util && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" MAKE="$(MAKE)"

lib/libntp.a: lib/*.c lib/Makefile adjtime/Makefile
	@echo
	@echo '###' creating NTP library
	@cd lib && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" MAKE="$(MAKE)"

parse/libparse.a:	parse/*.c parse/Makefile parse/util/Makefile lib/libntp.a
	@echo
	@echo '### creating PARSE subsystem (if configured)'
	@cd parse && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" MAKE="$(MAKE)"

FRC:

savebin:
	@test -d bin || mkdir bin
	@echo
	@echo '### saving $(TARGETS) $(OPTTARG) in bin'
	-@for f in $(TARGETS) $(OPTTARG); \
	 do test -f $$f && mv $$f bin/. && echo "### saved $$f in bin/"; \
	 done; \
	 true

neatneat:
	@echo '###' cleaning derived config files
	-@rm -f $(CONF).sed $(CONF)

neat:
	@echo '###' cleaning top level left overs
	-@rm -f eddep makedep Makefile.bak make.log make.out

distclean: neatneat clean
	@echo '###' cleaning configuration dependent Makefiles
	-@find . -name Makefile -print | \
	  while read X; do \
	    if [ -f "$$X.tmpl" ]; then \
	      rm -f "$$X"; \
	    else \
	      :; \
	    fi \
	  done
	@echo '###' cleaning old scratch files
	-@find . \( -name '*.rej' -o -name '*.orig' -o -name '*~' -o \
		    -name '.version' -o -name '#*' -o -name '.#*' -o \
		    -name core -o -name version.c \) -print | xargs rm -f
	@echo '###' cleaning saved binaries
	-@rm -fr bin

clean:	neat
	@echo '###' cleaning adjtime
	@cd adjtime && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" MAKE="$(MAKE)" -f Makefile.tmpl $@
	@echo '###' cleaning authstuff
	@cd authstuff && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" -f Makefile.tmpl MAKE="$(MAKE)" $@
	@echo '###' cleaning clockstuff
	@cd clockstuff && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" -f Makefile.tmpl MAKE="$(MAKE)" $@
	@echo '###' cleaning lib
	@cd lib && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" -f Makefile.tmpl MAKE="$(MAKE)" $@
	@echo '###' cleaning ntpdate
	@cd ntpdate && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" -f Makefile.tmpl MAKE="$(MAKE)" $@
	@echo '###' cleaning ntpq
	@cd ntpq && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" -f Makefile.tmpl MAKE="$(MAKE)" $@
	@echo '###' cleaning ntptrace
	@cd ntptrace && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" -f Makefile.tmpl MAKE="$(MAKE)" $@
	@echo '###' cleaning util
	@cd util && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" -f Makefile.tmpl MAKE="$(MAKE)" $@
	@echo '###' cleaning xntpd
	@cd xntpd && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" -f Makefile.tmpl MAKE="$(MAKE)" $@
	@echo '###' cleaning xntpdc
	@cd xntpdc && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" -f Makefile.tmpl MAKE="$(MAKE)" $@
	@echo '###' cleaning xntpres
	@cd xntpres && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" -f Makefile.tmpl MAKE="$(MAKE)" $@
	@echo '###' cleaning parse
	@cd parse && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" -f Makefile.tmpl MAKE="$(MAKE)" $@

install:	all
	@echo installing from xntpd
	@cd xntpd && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" MAKE="$(MAKE)" install
	@echo installing from xntpdc
	@cd xntpdc && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" MAKE="$(MAKE)" install
	@echo installing from ntpq
	@cd ntpq && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" MAKE="$(MAKE)" install
	@echo installing from ntptrace
	@cd ntptrace && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" MAKE="$(MAKE)" install
	@echo installing from ntpdate
	@cd ntpdate && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" MAKE="$(MAKE)" install
	@echo installing from xntpres
	@cd xntpres && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" MAKE="$(MAKE)" install
	@echo installing from util
	@cd util && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" MAKE="$(MAKE)" install
	@echo installing from parse
	@cd parse && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" MAKE="$(MAKE)" install

dist:
	@echo '### building distribution ...'
	@$(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" MAKE="$(MAKE)" distclean
	@DISTVERSION="`sed -e 's/^[ 	]*[Vv][Ee][Rr][Ss][Ii][Oo][Nn][ 	]*=\(.*\)$$/\1/' VERSION | \
		    sed -e 's/[^0-9a-zA-Z\.]/_/g; s/__*/_/g; s/_*$$//'`" && \
	 echo "### creating distribution file $(DISTNAME)$${DISTVERSION}.tar" && \
	 rm -f $(DISTNAME)$${DISTVERSION}.tar $(DISTNAME)$${DISTVERSION}.tar.* && \
	 tar cfv $(DISTNAME)$${DISTVERSION}.tar `ls | egrep -v "^$(CONFL)$$|^$(DISTNAME)$${DISTVERSION}.tar$$"` && \
	 $(COMPRESSOR) -v $(DISTNAME)$${DISTVERSION}.tar

$(CONF).sed:	$(CONF) Makefile
	@sed -n -e 's:^\([^ 	]*\)=[ 	]*\(.*\):s~^\1=.*~&~:p' < $(CONF) > $@

depend:
	find . -name Makefile.tmpl -print > eddep
	echo >> makedep
	sed -e 's:^\./::' -e '/^Makefile/d' \
	    -e h \
	    -e 's/^\(.*\)\.tmpl$$/\1:	\1.tmpl $${CONF}.sed/' -e p -e g \
	    -e 's/.*/	@echo/' -e p -e g \
	    -e 's:^\(.*\)/Makefile\.tmpl$$:	@echo '"'###'"' updating Makefile in \1:' -e p -e g \
	    -e 's/.*/	@sed -f $${CONF}.sed < $$@.tmpl > $$@/' -e p -e g \
	    -e 's:^\(.*\)/Makefile\.tmpl$$:	@echo '"'###'"' cleaning in \1:' -e p -e g \
	    -e 's:^\(.*\)/Makefile\.tmpl$$:	@cd \1 \&\& $$(MAKE) $$(MFLAGS) MFLAGS="$$(MFLAGS)" -f Makefile.tmpl MAKE="$$(MAKE)" clean:p' \
	    < eddep >> makedep
	echo '/^# DO NOT DELETE THIS LINE/+1,$$d' >eddep
	echo '$$r makedep' >>eddep
	echo 'w' >>eddep
	cp Makefile Makefile.bak
	/bin/ed - Makefile < eddep
	rm eddep makedep

# DO NOT DELETE THIS LINE -- It is used by 'make depend' to update this file

adjtime/Makefile:	adjtime/Makefile.tmpl ${CONF}.sed
	@echo
	@echo '###' updating Makefile in adjtime
	@sed -f ${CONF}.sed < $@.tmpl > $@
	@echo '###' cleaning in adjtime
	@cd adjtime && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" -f Makefile.tmpl MAKE="$(MAKE)" clean
authstuff/Makefile:	authstuff/Makefile.tmpl ${CONF}.sed
	@echo
	@echo '###' updating Makefile in authstuff
	@sed -f ${CONF}.sed < $@.tmpl > $@
	@echo '###' cleaning in authstuff
	@cd authstuff && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" -f Makefile.tmpl MAKE="$(MAKE)" clean
clockstuff/Makefile:	clockstuff/Makefile.tmpl ${CONF}.sed
	@echo
	@echo '###' updating Makefile in clockstuff
	@sed -f ${CONF}.sed < $@.tmpl > $@
	@echo '###' cleaning in clockstuff
	@cd clockstuff && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" -f Makefile.tmpl MAKE="$(MAKE)" clean
kernel/Makefile:	kernel/Makefile.tmpl ${CONF}.sed
	@echo
	@echo '###' updating Makefile in kernel
	@sed -f ${CONF}.sed < $@.tmpl > $@
	@echo '###' cleaning in kernel
	@cd kernel && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" -f Makefile.tmpl MAKE="$(MAKE)" clean
lib/Makefile:	lib/Makefile.tmpl ${CONF}.sed
	@echo
	@echo '###' updating Makefile in lib
	@sed -f ${CONF}.sed < $@.tmpl > $@
	@echo '###' cleaning in lib
	@cd lib && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" -f Makefile.tmpl MAKE="$(MAKE)" clean
ntpdate/Makefile:	ntpdate/Makefile.tmpl ${CONF}.sed
	@echo
	@echo '###' updating Makefile in ntpdate
	@sed -f ${CONF}.sed < $@.tmpl > $@
	@echo '###' cleaning in ntpdate
	@cd ntpdate && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" -f Makefile.tmpl MAKE="$(MAKE)" clean
ntpq/Makefile:	ntpq/Makefile.tmpl ${CONF}.sed
	@echo
	@echo '###' updating Makefile in ntpq
	@sed -f ${CONF}.sed < $@.tmpl > $@
	@echo '###' cleaning in ntpq
	@cd ntpq && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" -f Makefile.tmpl MAKE="$(MAKE)" clean
ntptrace/Makefile:	ntptrace/Makefile.tmpl ${CONF}.sed
	@echo
	@echo '###' updating Makefile in ntptrace
	@sed -f ${CONF}.sed < $@.tmpl > $@
	@echo '###' cleaning in ntptrace
	@cd ntptrace && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" -f Makefile.tmpl MAKE="$(MAKE)" clean
util/Makefile:	util/Makefile.tmpl ${CONF}.sed
	@echo
	@echo '###' updating Makefile in util
	@sed -f ${CONF}.sed < $@.tmpl > $@
	@echo '###' cleaning in util
	@cd util && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" -f Makefile.tmpl MAKE="$(MAKE)" clean
xntpd/Makefile:	xntpd/Makefile.tmpl ${CONF}.sed
	@echo
	@echo '###' updating Makefile in xntpd
	@sed -f ${CONF}.sed < $@.tmpl > $@
	@echo '###' cleaning in xntpd
	@cd xntpd && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" -f Makefile.tmpl MAKE="$(MAKE)" clean
xntpdc/Makefile:	xntpdc/Makefile.tmpl ${CONF}.sed
	@echo
	@echo '###' updating Makefile in xntpdc
	@sed -f ${CONF}.sed < $@.tmpl > $@
	@echo '###' cleaning in xntpdc
	@cd xntpdc && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" -f Makefile.tmpl MAKE="$(MAKE)" clean
xntpres/Makefile:	xntpres/Makefile.tmpl ${CONF}.sed
	@echo
	@echo '###' updating Makefile in xntpres
	@sed -f ${CONF}.sed < $@.tmpl > $@
	@echo '###' cleaning in xntpres
	@cd xntpres && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" -f Makefile.tmpl MAKE="$(MAKE)" clean
parse/util/Makefile:	parse/util/Makefile.tmpl ${CONF}.sed
	@echo
	@echo '###' updating Makefile in parse/util
	@sed -f ${CONF}.sed < $@.tmpl > $@
	@echo '###' cleaning in parse/util
	@cd parse/util && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" -f Makefile.tmpl MAKE="$(MAKE)" clean
parse/Makefile:	parse/Makefile.tmpl ${CONF}.sed
	@echo
	@echo '###' updating Makefile in parse
	@sed -f ${CONF}.sed < $@.tmpl > $@
	@echo '###' cleaning in parse
	@cd parse && $(MAKE) $(MFLAGS) MFLAGS="$(MFLAGS)" -f Makefile.tmpl MAKE="$(MAKE)" clean

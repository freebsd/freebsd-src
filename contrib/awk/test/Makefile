# Generated automatically from Makefile.in by configure.
# Makefile for GNU Awk test suite.
#
# Copyright (C) 1988-1997 the Free Software Foundation, Inc.
# 
# This file is part of GAWK, the GNU implementation of the
# AWK Programming Language.
# 
# GAWK is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# GAWK is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

SHELL = /bin/sh
AWK = ../gawk
CMP = cmp

srcdir = .

bigtest:	basic unix-tests gawk.extensions

basic:	msg swaplns messages argarray longwrds \
	getline fstabplus compare arrayref rs fsrs rand \
	fsbs negexp asgext anchgsub splitargv awkpath nfset reparse \
	convfmt arrayparm paramdup nonl defref nofmtch litoct resplit \
	rswhite prmarscl sclforin sclifin intprec childin noeffect \
	numsubstr pcntplus prmreuse math fldchg fldchgnf reindops \
	sprintfc backgsub tweakfld clsflnam mmap8k fnarray \
	dynlj substr eofsplit prt1eval gsubasgn prtoeval gsubtest splitwht \
	back89 tradanch nlfldsep splitvar intest nfldstr nors fnarydel \
	noparms funstack clobber delarprm prdupval

unix-tests: poundbang fflush getlnhd pipeio1 pipeio2 strftlng pid

gawk.extensions: fieldwdth ignrcase posix manyfiles igncfs argtest \
		badargs strftime gensub gnureops reint nondec

extra:	regtest inftest

poundbang::
	@cp $(AWK) /tmp/gawk && $(srcdir)/poundbang $(srcdir)/poundbang >_`basename $@`
	@rm -f /tmp/gawk
	$(CMP) $(srcdir)/poundbang.ok _`basename $@` && rm -f _`basename $@`

msg::
	@echo 'Any output from "cmp" is bad news, although some differences'
	@echo 'in floating point values are probably benign -- in particular,'
	@echo 'some systems may omit a leading zero and the floating point'
	@echo 'precision may lead to slightly different output in a few cases.'

swaplns::
	@$(AWK) -f $(srcdir)/swaplns.awk $(srcdir)/swaplns.in >_$@
	$(CMP) $(srcdir)/swaplns.ok _$@ && rm -f _$@

messages::
	@$(AWK) -f $(srcdir)/messages.awk >out2 2>out3
	{ $(CMP) $(srcdir)/out1.ok out1 && $(CMP) $(srcdir)/out2.ok out2 && \
	$(CMP) $(srcdir)/out3.ok out3 && rm -f out1 out2 out3; } || \
	{ { test -d /dev/fd || test -d /proc/self/fd; } && \
	echo IT IS OK THAT THIS TEST FAILED; }

argarray::
	@case $(srcdir) in \
	.)	: ;; \
	*)	cp $(srcdir)/argarray.in . ;; \
	esac
	@TEST=test echo just a test | $(AWK) -f $(srcdir)/argarray.awk ./argarray.in - >_$@
	$(CMP) $(srcdir)/argarray.ok _$@ && rm -f _$@

fstabplus::
	@echo '1		2' | $(AWK) -f $(srcdir)/fstabplus.awk >_$@
	$(CMP) $(srcdir)/fstabplus.ok _$@ && rm -f _$@

fsrs::
	@$(AWK) -f $(srcdir)/fsrs.awk $(srcdir)/fsrs.in >_$@
	$(CMP) $(srcdir)/fsrs.ok _$@ && rm -f _$@

igncfs::
	@$(AWK) -f $(srcdir)/igncfs.awk $(srcdir)/igncfs.in >_$@
	$(CMP) $(srcdir)/igncfs.ok _$@ && rm -f _$@

longwrds::
	@$(AWK) -f $(srcdir)/longwrds.awk $(srcdir)/manpage | sort >_$@
	$(CMP) $(srcdir)/longwrds.ok _$@ && rm -f _$@

fieldwdth::
	@echo '123456789' | $(AWK) -v FIELDWIDTHS="2 3 4" '{ print $$2}' >_$@
	$(CMP) $(srcdir)/fieldwdth.ok _$@ && rm -f _$@

ignrcase::
	@echo xYz | $(AWK) -v IGNORECASE=1 '{ sub(/y/, ""); print}' >_$@
	$(CMP) $(srcdir)/ignrcase.ok _$@ && rm -f _$@

regtest::
	@echo 'Some of the output from regtest is very system specific, do not'
	@echo 'be distressed if your output differs from that distributed.'
	@echo 'Manual inspection is called for.'
	AWK=`pwd`/$(AWK) $(srcdir)/regtest

posix::
	@echo '1:2,3 4' | $(AWK) -f $(srcdir)/posix.awk >_$@
	$(CMP) $(srcdir)/posix.ok _$@ && rm -f _$@

manyfiles::
	@rm -rf junk
	@mkdir junk
	@$(AWK) 'BEGIN { for (i = 1; i <= 300; i++) print i, i}' >_$@
	@$(AWK) -f $(srcdir)/manyfiles.awk _$@ _$@
	@echo "This number better be 1 ->" | tr -d '\012'
	@wc -l junk/* | $(AWK) '$$1 != 2' | wc -l
	@rm -rf junk _$@

compare::
	@$(AWK) -f $(srcdir)/compare.awk 0 1 $(srcdir)/compare.in >_$@
	$(CMP) $(srcdir)/compare.ok _$@ && rm -f _$@

arrayref::
	@$(AWK) -f $(srcdir)/arrayref.awk >_$@
	$(CMP) $(srcdir)/arrayref.ok _$@ && rm -f _$@

rs::
	@$(AWK) -v RS="" '{ print $$1, $$2}' $(srcdir)/rs.in >_$@
	$(CMP) $(srcdir)/rs.ok _$@ && rm -f _$@

fsbs::
	@$(AWK) -v FS='\' '{ print $$1, $$2 }' $(srcdir)/fsbs.in >_$@
	$(CMP) $(srcdir)/fsbs.ok _$@ && rm -f _$@

inftest::
	@echo This test is very machine specific...
	@$(AWK) -f $(srcdir)/inftest.awk >_$@
	$(CMP) $(srcdir)/inftest.ok _$@ && rm -f _$@

getline::
	@$(AWK) -f $(srcdir)/getline.awk $(srcdir)/getline.awk $(srcdir)/getline.awk >_$@
	$(CMP) $(srcdir)/getline.ok _$@ && rm -f _$@

rand::
	@$(AWK) -f $(srcdir)/rand.awk >_$@
	$(CMP) $(srcdir)/rand.ok _$@ && rm -f _$@

negexp::
	@$(AWK) 'BEGIN { a = -2; print 10^a }' >_$@
	$(CMP) $(srcdir)/negexp.ok _$@ && rm -f _$@

asgext::
	@$(AWK) -f $(srcdir)/asgext.awk $(srcdir)/asgext.in >_$@
	$(CMP) $(srcdir)/asgext.ok _$@ && rm -f _$@

anchgsub::
	@$(AWK) -f $(srcdir)/anchgsub.awk $(srcdir)/anchgsub.in >_$@
	$(CMP) $(srcdir)/anchgsub.ok _$@ && rm -f _$@

splitargv::
	@$(AWK) -f $(srcdir)/splitargv.awk $(srcdir)/splitargv.in >_$@
	$(CMP) $(srcdir)/splitargv.ok _$@ && rm -f _$@

awkpath::
	@AWKPATH="$(srcdir):$(srcdir)/lib" $(AWK) -f awkpath.awk >_$@
	$(CMP) $(srcdir)/awkpath.ok _$@ && rm -f _$@

nfset::
	@$(AWK) -f $(srcdir)/nfset.awk $(srcdir)/nfset.in >_$@
	$(CMP) $(srcdir)/nfset.ok _$@ && rm -f _$@

reparse::
	@$(AWK) -f $(srcdir)/reparse.awk $(srcdir)/reparse.in >_$@
	$(CMP) $(srcdir)/reparse.ok _$@ && rm -f _$@

argtest::
	@$(AWK) -f $(srcdir)/argtest.awk -x -y abc >_$@
	$(CMP) $(srcdir)/argtest.ok _$@ && rm -f _$@

badargs::
	@-$(AWK) -f 2>&1 | grep -v patchlevel >_$@
	$(CMP) $(srcdir)/badargs.ok _$@ && rm -f _$@

convfmt::
	@$(AWK) -f $(srcdir)/convfmt.awk >_$@
	$(CMP) $(srcdir)/convfmt.ok _$@ && rm -f _$@

arrayparm::
	@-AWKPATH=$(srcdir) $(AWK) -f arrayparm.awk >_$@ 2>&1 || exit 0
	$(CMP) $(srcdir)/arrayparm.ok _$@ && rm -f _$@

paramdup::
	@-AWKPATH=$(srcdir) $(AWK) -f paramdup.awk >_$@ 2>&1 || exit 0
	$(CMP) $(srcdir)/paramdup.ok _$@ && rm -f _$@

nonl::
	@-AWKPATH=$(srcdir) $(AWK) --lint -f nonl.awk /dev/null >_$@ 2>&1
	$(CMP) $(srcdir)/nonl.ok _$@ && rm -f _$@

defref::
	@-AWKPATH=$(srcdir) $(AWK) --lint -f defref.awk >_$@ 2>&1 || exit 0
	$(CMP) $(srcdir)/defref.ok _$@ && rm -f _$@

nofmtch::
	@-AWKPATH=$(srcdir) $(AWK) --lint -f nofmtch.awk >_$@ 2>&1
	$(CMP) $(srcdir)/nofmtch.ok _$@ && rm -f _$@

strftime::
	: this test could fail on slow machines or on a second boundary,
	: so if it does, double check the actual results
	@LC_ALL=C; export LC_ALL; LANC=C; export LANG; \
	date | $(AWK) '{ $$3 = sprintf("%02d", $$3 + 0) ; \
	print > "strftime.ok" ; \
	print strftime() > "'_$@'" }'
	$(CMP) strftime.ok _$@ && rm -f _$@ strftime.ok || exit 0

litoct::
	@echo ab | $(AWK) --traditional -f $(srcdir)/litoct.awk >_$@
	$(CMP) $(srcdir)/litoct.ok _$@ && rm -f _$@

gensub::
	@$(AWK) -f $(srcdir)/gensub.awk $(srcdir)/gensub.in >_$@
	$(CMP) $(srcdir)/gensub.ok _$@ && rm -f _$@

resplit::
	@echo a:b:c d:e:f | $(AWK) '{ FS = ":"; $$0 = $$0; print $$2 }' > _$@
	$(CMP) $(srcdir)/resplit.ok _$@ && rm -f _$@

rswhite::
	@$(AWK) -f $(srcdir)/rswhite.awk $(srcdir)/rswhite.in > _$@
	$(CMP) $(srcdir)/rswhite.ok _$@ && rm -f _$@

prmarscl::
	@-AWKPATH=$(srcdir) $(AWK) -f prmarscl.awk > _$@ 2>&1 || exit 0
	$(CMP) $(srcdir)/prmarscl.ok _$@ && rm -f _$@

sclforin::
	@-AWKPATH=$(srcdir) $(AWK) -f sclforin.awk > _$@ 2>&1 || exit 0
	$(CMP) $(srcdir)/sclforin.ok _$@ && rm -f _$@

sclifin::
	@-AWKPATH=$(srcdir) $(AWK) -f sclifin.awk > _$@ 2>&1 || exit 0
	$(CMP) $(srcdir)/sclifin.ok _$@ && rm -f _$@

intprec::
	@-$(AWK) -f $(srcdir)/intprec.awk > _$@ 2>&1
	$(CMP) $(srcdir)/intprec.ok _$@ && rm -f _$@

childin::
	@echo hi | $(AWK) 'BEGIN { "cat" | getline; print; close("cat") }' > _$@
	$(CMP) $(srcdir)/childin.ok _$@ && rm -f _$@

noeffect::
	@-AWKPATH=$(srcdir) $(AWK) --lint -f noeffect.awk > _$@ 2>&1
	$(CMP) $(srcdir)/noeffect.ok _$@ && rm -f _$@

numsubstr::
	@-AWKPATH=$(srcdir) $(AWK) -f numsubstr.awk $(srcdir)/numsubstr.in >_$@
	$(CMP) $(srcdir)/numsubstr.ok _$@ && rm -f _$@

gnureops::
	@$(AWK) -f $(srcdir)/gnureops.awk >_$@
	$(CMP) $(srcdir)/gnureops.ok _$@ && rm -f _$@

pcntplus::
	@$(AWK) -f $(srcdir)/pcntplus.awk >_$@
	$(CMP) $(srcdir)/pcntplus.ok _$@ && rm -f _$@

prmreuse::
	@$(AWK) -f $(srcdir)/prmreuse.awk >_$@
	$(CMP) $(srcdir)/prmreuse.ok _$@ && rm -f _$@

math::
	@$(AWK) -f $(srcdir)/math.awk >_$@
	$(CMP) $(srcdir)/math.ok _$@ && rm -f _$@

fflush::
	@$(srcdir)/fflush.sh >_$@
	$(CMP) $(srcdir)/fflush.ok _$@ && rm -f _$@

fldchg::
	@$(AWK) -f $(srcdir)/fldchg.awk $(srcdir)/fldchg.in >_$@
	$(CMP) $(srcdir)/fldchg.ok _$@ && rm -f _$@

fldchgnf::
	@$(AWK) -f $(srcdir)/fldchgnf.awk $(srcdir)/fldchgnf.in >_$@
	$(CMP) $(srcdir)/fldchgnf.ok _$@ && rm -f _$@

reindops::
	@$(AWK) -f $(srcdir)/reindops.awk $(srcdir)/reindops.in >_$@
	$(CMP) $(srcdir)/reindops.ok _$@ && rm -f _$@

sprintfc::
	@$(AWK) -f $(srcdir)/sprintfc.awk $(srcdir)/sprintfc.in >_$@
	$(CMP) $(srcdir)/sprintfc.ok _$@ && rm -f _$@

getlnhd::
	@$(AWK) -f $(srcdir)/getlnhd.awk >_$@
	$(CMP) $(srcdir)/getlnhd.ok _$@ && rm -f _$@

backgsub::
	@$(AWK) -f $(srcdir)/backgsub.awk $(srcdir)/backgsub.in >_$@
	$(CMP) $(srcdir)/backgsub.ok _$@ && rm -f _$@

tweakfld::
	@$(AWK) -f $(srcdir)/tweakfld.awk $(srcdir)/tweakfld.in >_$@
	@rm -f errors.cleanup
	$(CMP) $(srcdir)/tweakfld.ok _$@ && rm -f _$@

clsflnam::
	@$(AWK) -f $(srcdir)/clsflnam.awk $(srcdir)/clsflnam.in >_$@
	$(CMP) $(srcdir)/clsflnam.ok _$@ && rm -f _$@

mmap8k::
	@$(AWK) '{ print }' $(srcdir)/mmap8k.in >_$@
	$(CMP) $(srcdir)/mmap8k.in _$@ && rm -f _$@

fnarray::
	@-AWKPATH=$(srcdir) $(AWK) -f fnarray.awk >_$@ 2>&1 || exit 0
	$(CMP) $(srcdir)/fnarray.ok _$@ && rm -f _$@

dynlj::
	@$(AWK) -f $(srcdir)/dynlj.awk >_$@
	$(CMP) $(srcdir)/dynlj.ok _$@ && rm -f _$@

substr::
	@$(AWK) -f $(srcdir)/substr.awk >_$@
	$(CMP) $(srcdir)/substr.ok _$@ && rm -f _$@

eofsplit::
	@$(AWK) -f $(srcdir)/eofsplit.awk >_$@
	$(CMP) $(srcdir)/eofsplit.ok _$@ && rm -f _$@

prt1eval::
	@$(AWK) -f $(srcdir)/prt1eval.awk >_$@
	$(CMP) $(srcdir)/prt1eval.ok _$@ && rm -f _$@

gsubasgn::
	@-AWKPATH=$(srcdir) $(AWK) -f gsubasgn.awk >_$@ 2>&1 || exit 0
	$(CMP) $(srcdir)/gsubasgn.ok _$@ && rm -f _$@

prtoeval::
	@$(AWK) -f $(srcdir)/prtoeval.awk >_$@
	$(CMP) $(srcdir)/prtoeval.ok _$@ && rm -f _$@

gsubtest::
	@$(AWK) -f $(srcdir)/gsubtest.awk >_$@
	$(CMP) $(srcdir)/gsubtest.ok _$@ && rm -f _$@

splitwht::
	@$(AWK) -f $(srcdir)/splitwht.awk >_$@
	$(CMP) $(srcdir)/splitwht.ok _$@ && rm -f _$@

back89::
	@$(AWK) '/a\8b/' $(srcdir)/back89.in >_$@
	$(CMP) $(srcdir)/back89.ok _$@ && rm -f _$@

tradanch::
	@$(AWK) --traditional -f $(srcdir)/tradanch.awk $(srcdir)/tradanch.in >_$@
	$(CMP) $(srcdir)/tradanch.ok _$@ && rm -f _$@

nlfldsep::
	@$(AWK) -f $(srcdir)/nlfldsep.awk $(srcdir)/nlfldsep.in > _$@
	$(CMP) $(srcdir)/nlfldsep.ok _$@ && rm -f _$@

splitvar::
	@$(AWK) -f $(srcdir)/splitvar.awk $(srcdir)/splitvar.in >_$@
	$(CMP) $(srcdir)/splitvar.ok _$@ && rm -f _$@

intest::
	@$(AWK) -f $(srcdir)/intest.awk >_$@
	$(CMP) $(srcdir)/intest.ok _$@ && rm -f _$@

# AIX /bin/sh exec's the last command in a list, therefore issue a ":"
# command so that pid.sh is fork'ed as a child before being exec'ed.
pid::
	@AWKPATH=$(srcdir) AWK=$(AWK) $(SHELL) $(srcdir)/pid.sh $$$$ > _`basename $@` ; :
	$(CMP) $(srcdir)/pid.ok _`basename $@` && rm -f _`basename $@` _`basename $@`.in

strftlng::
	@TZ=UTC; export TZ; $(AWK) -f $(srcdir)/strftlng.awk >_$@
	@if $(CMP) -s $(srcdir)/strftlng.ok _$@ ; then : ; else \
 	TZ=UTC0; export TZ; $(AWK) -f $(srcdir)/strftlng.awk >_$@ ; \
	fi
	$(CMP) $(srcdir)/strftlng.ok _$@ && rm -f _$@

nfldstr::
	@echo | $(AWK) '$$1 == 0 { print "bug" }' > _$@
	$(CMP) $(srcdir)/nfldstr.ok _$@ && rm -f _$@

nors::
	@echo A B C D E | tr -d '\12' | $(AWK) '{ print $$NF }' - $(srcdir)/nors.in > _$@
	$(CMP) $(srcdir)/nors.ok _$@ && rm -f _$@

fnarydel::
	@$(AWK) -f $(srcdir)/fnarydel.awk >_$@
	$(CMP) $(srcdir)/fnarydel.ok _$@ && rm -f _$@

reint::
	@$(AWK) --re-interval -f $(srcdir)/reint.awk $(srcdir)/reint.in >_$@
	$(CMP) $(srcdir)/reint.ok _$@ && rm -f _$@

noparms::
	@-AWKPATH=$(srcdir) $(AWK) -f noparms.awk >_$@ 2>&1 || exit 0
	$(CMP) $(srcdir)/noparms.ok _$@ && rm -f _$@

pipeio1::
	@$(AWK) -f $(srcdir)/pipeio1.awk >_$@
	@rm -f test1 test2
	$(CMP) $(srcdir)/pipeio1.ok _$@ && rm -f _$@

pipeio2::
	@$(AWK) -v SRCDIR=$(srcdir) -f $(srcdir)/pipeio2.awk >_$@
	$(CMP) $(srcdir)/pipeio2.ok _$@ && rm -f _$@

funstack::
	@$(AWK) -f $(srcdir)/funstack.awk $(srcdir)/funstack.in >_$@
	$(CMP) $(srcdir)/funstack.ok _$@ && rm -f _$@

clobber::
	@$(AWK) -f $(srcdir)/clobber.awk >_$@
	$(CMP) $(srcdir)/clobber.ok seq && $(CMP) $(srcdir)/clobber.ok _$@ && rm -f _$@
	@rm -f seq

delarprm::
	@$(AWK) -f $(srcdir)/delarprm.awk >_$@
	$(CMP) $(srcdir)/delarprm.ok _$@ && rm -f _$@

prdupval::
	@$(AWK) -f $(srcdir)/prdupval.awk $(srcdir)/prdupval.in >_$@
	$(CMP) $(srcdir)/prdupval.ok _$@ && rm -f _$@

nondec::
	@if grep BITOP ../config.h | grep define > /dev/null; \
	then \
		$(AWK) -f $(srcdir)/nondec.awk >_$@; \
	else \
		cp $(srcdir)/nondec.ok _$@; \
	fi
	$(CMP) $(srcdir)/nondec.ok _$@ && rm -f _$@

clean:
	rm -fr _* core junk out1 out2 out3 strftime.ok test1 test2 seq *~

distclean: clean
	rm -f Makefile

maintainer-clean: distclean

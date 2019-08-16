# Before running this file, we assume we have generated all tables by running
# the command `iasl -T ALL`
#
# Note: the NAME flag is required when running the test

aml_obj=$(NAME).aml
dsl_obj=$(NAME).dsl
aml_obj2=$(NAME)_recomp.aml


all: $(aml_obj2)

# recompile and binary compare
$(aml_obj2): %_recomp.aml: %.dsl
	iasl -p `basename $@` $< > /dev/null 2> /dev/null
	acpibin -c $@ $(patsubst %_recomp.aml,%.aml,$@) > /dev/null 2> /dev/null
	printf "Data table %s PASS\n" $(basename $< .dsl)

# disassemble
$(dsl_obj): %.dsl:  %.aml
	iasl -d $< > /dev/null 2> /dev/null

# initial compile
$(aml_obj): %.aml:  %.asl
	iasl $< > /dev/null 2> /dev/null


templates :
	sh templates.sh

.PHONY: clean
clean:
	rm -f *.asl *.aml *.dsl *.hex diff.log


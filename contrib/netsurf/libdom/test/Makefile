testutils_files := testutils/comparators.c;testutils/list.c;testutils/domtsasserts.c
testutils_files := $(testutils_files);testutils/utils.c;testutils/foreach.c;testutils/load.c

TESTCFLAGS := $(TESTCFLAGS) -I$(DIR) -I$(DIR)testutils -Ibindings/xml -Ibindings/hubbub -Wno-unused -fno-strict-aliasing

ALL_XML_TESTS :=

# 1: Path to XML file
# 2: Fragment C file name
# 3: DTD file
# 4: Test name
define do_xml_test


ifeq ($$(WANT_TEST),yes)

$(DIR)$2: $(DIR)testcases/tests/$1 $(DIR)transform.pl  $(DIR)DOMTSHandler.pm
	$(VQ)$(ECHO) "   XFORM: $1"
	$(Q)$(PERL) $(DIR)transform.pl $(DIR)$3 $(DIR)testcases/tests/$(dir $1)/files $(DIR)testcases/tests/$1 > $(DIR)$2

DIR_TEST_ITEMS := $$(DIR_TEST_ITEMS) $4:$2;$(testutils_files)

endif

DISTCLEAN_ITEMS := $$(DISTCLEAN_ITEMS) $(DIR)$2

ALL_XML_TESTS := $$(ALL_XML_TESTS) $4



endef

# 1: suite base
# 2: dtd for suite
define do_xml_suite

$(foreach XML,$(filter-out $1/metadata.xml,$(filter-out $1/alltests.xml,$(subst $(DIR)testcases/tests/,,$(wildcard $(DIR)testcases/tests/$1/*.xml)))),$(call do_xml_test,$(XML),$(subst /,_,$(XML:.xml=.c)),$2,$(subst /,_,$(XML:.xml=))))

endef

# 1: test name
define write_index

	$(Q)$(ECHO) "$1	$1" >> $@

endef

$(DIR)INDEX: test/Makefile
	$(VQ)$(ECHO) "   INDEX: Making test index"
	$(Q)$(ECHO) "#test	desc	dir" > $@
	$(foreach XMLTEST,$(sort $(ALL_XML_TESTS)),$(call write_index,$(XMLTEST)))

TEST_PREREQS := $(TEST_PREREQS) $(DIR)INDEX

# Include the level 1 core tests
$(eval $(call do_xml_suite,level1/core,dom1-interfaces.xml))
# Include level 1 html tests
$(eval $(call do_xml_suite,level1/html,dom1-interfaces.xml))

# Include the level 2 core tests
$(eval $(call do_xml_suite,level2/core,dom2-core-interface.xml))

CLEAN_ITEMS := $(DIR)INDEX

include $(NSBUILD)/Makefile.subdir


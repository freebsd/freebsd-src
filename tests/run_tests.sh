#! /bin/sh

. ./tests.sh

if [ -z "$CC" ]; then
    CC=gcc
fi

export QUIET_TEST=1
STOP_ON_FAIL=0

export VALGRIND=
VGCODE=126

tot_tests=0
tot_pass=0
tot_fail=0
tot_config=0
tot_vg=0
tot_strange=0

base_run_test() {
    tot_tests=$((tot_tests + 1))
    if VALGRIND="$VALGRIND" "$@"; then
	tot_pass=$((tot_pass + 1))
    else
	ret="$?"
	if [ "$STOP_ON_FAIL" -eq 1 ]; then
	    exit 1
	fi
	if [ "$ret" -eq 1 ]; then
	    tot_config=$((tot_config + 1))
	elif [ "$ret" -eq 2 ]; then
	    tot_fail=$((tot_fail + 1))
	elif [ "$ret" -eq $VGCODE ]; then
	    tot_vg=$((tot_vg + 1))
	else
	    tot_strange=$((tot_strange + 1))
	fi
    fi
}

shorten_echo () {
    limit=32
    printf "$1"
    shift
    for x; do
	if [ ${#x} -le $limit ]; then
	    printf " $x"
	else
	    short=$(echo "$x" | head -c$limit)
	    printf " \"$short\"...<${#x} bytes>"
	fi
    done
}

run_test () {
    printf "$*:	"
    if [ -n "$VALGRIND" -a -f $1.supp ]; then
	VGSUPP="--suppressions=$1.supp"
    fi
    base_run_test $VALGRIND $VGSUPP "./$@"
}

run_sh_test () {
    printf "$*:	"
    base_run_test sh "$@"
}

wrap_test () {
    (
	if verbose_run "$@"; then
	    PASS
	else
	    ret="$?"
	    if [ "$ret" -gt 127 ]; then
		signame=$(kill -l $((ret - 128)))
		FAIL "Killed by SIG$signame"
	    else
		FAIL "Returned error code $ret"
	    fi
	fi
    )
}

run_wrap_test () {
    shorten_echo "$@:	"
    base_run_test wrap_test "$@"
}

wrap_error () {
    (
	if verbose_run "$@"; then
	    FAIL "Expected non-zero return code"
	else
	    ret="$?"
	    if [ "$ret" -gt 127 ]; then
		signame=$(kill -l $((ret - 128)))
		FAIL "Killed by SIG$signame"
	    else
		PASS
	    fi
	fi
    )
}

run_wrap_error_test () {
    shorten_echo "$@"
    printf " {!= 0}:	"
    base_run_test wrap_error "$@"
}

# $1: dtb file
# $2: align base
check_align () {
    shorten_echo "check_align $@:	"
    local size=$(stat -c %s "$1")
    local align="$2"
    (
	if [ $(($size % $align)) -eq 0 ] ;then
	    PASS
	else
	    FAIL "Output size $size is not $align-byte aligned"
	fi
    )
}

run_dtc_test () {
    printf "dtc $*:	"
    base_run_test wrap_test $VALGRIND $DTC "$@"
}

asm_to_so () {
    $CC -shared -o $1.test.so data.S $1.test.s
}

asm_to_so_test () {
    run_wrap_test asm_to_so "$@"
}

run_fdtget_test () {
    expect="$1"
    shift
    printf "fdtget-runtest.sh %s $*:	" "$(echo $expect)"
    base_run_test sh fdtget-runtest.sh "$expect" "$@"
}

run_fdtput_test () {
    expect="$1"
    shift
    shorten_echo fdtput-runtest.sh "$expect" "$@"
    printf ":	"
    base_run_test sh fdtput-runtest.sh "$expect" "$@"
}

run_fdtdump_test() {
    file="$1"
    shorten_echo fdtdump-runtest.sh "$file"
    printf ":	"
    base_run_test sh fdtdump-runtest.sh "$file"
}

BAD_FIXUP_TREES="bad_index \
		empty \
		empty_index \
		index_trailing \
		path_empty_prop \
		path_only \
		path_only_sep \
		path_prop"

# Test to exercise libfdt overlay application without dtc's overlay support
libfdt_overlay_tests () {
    # First test a doctored overlay which requires only local fixups
    run_dtc_test -I dts -O dtb -o overlay_base_no_symbols.test.dtb overlay_base.dts
    run_test check_path overlay_base_no_symbols.test.dtb not-exists "/__symbols__"
    run_test check_path overlay_base_no_symbols.test.dtb not-exists "/__fixups__"
    run_test check_path overlay_base_no_symbols.test.dtb not-exists "/__local_fixups__"

    run_dtc_test -I dts -O dtb -o overlay_overlay_no_fixups.test.dtb overlay_overlay_no_fixups.dts
    run_test check_path overlay_overlay_no_fixups.test.dtb not-exists "/__symbols__"
    run_test check_path overlay_overlay_no_fixups.test.dtb not-exists "/__fixups__"
    run_test check_path overlay_overlay_no_fixups.test.dtb exists "/__local_fixups__"

    run_test overlay overlay_base_no_symbols.test.dtb overlay_overlay_no_fixups.test.dtb

    # Then test with manually constructed fixups
    run_dtc_test -I dts -O dtb -o overlay_base_manual_symbols.test.dtb overlay_base_manual_symbols.dts
    run_test check_path overlay_base_manual_symbols.test.dtb exists "/__symbols__"
    run_test check_path overlay_base_manual_symbols.test.dtb not-exists "/__fixups__"
    run_test check_path overlay_base_manual_symbols.test.dtb not-exists "/__local_fixups__"

    run_dtc_test -I dts -O dtb -o overlay_overlay_manual_fixups.test.dtb overlay_overlay_manual_fixups.dts
    run_test check_path overlay_overlay_manual_fixups.test.dtb not-exists "/__symbols__"
    run_test check_path overlay_overlay_manual_fixups.test.dtb exists "/__fixups__"
    run_test check_path overlay_overlay_manual_fixups.test.dtb exists "/__local_fixups__"

    run_test overlay overlay_base_manual_symbols.test.dtb overlay_overlay_manual_fixups.test.dtb

    # Bad fixup tests
    for test in $BAD_FIXUP_TREES; do
	tree="overlay_bad_fixup_$test"
	run_dtc_test -I dts -O dtb -o $tree.test.dtb $tree.dts
	run_test overlay_bad_fixup overlay_base_no_symbols.test.dtb $tree.test.dtb
    done
}

# Tests to exercise dtc's overlay generation support
dtc_overlay_tests () {
    # Overlay tests for dtc
    run_dtc_test -@ -I dts -O dtb -o overlay_base.test.dtb overlay_base.dts
    run_test check_path overlay_base.test.dtb exists "/__symbols__"
    run_test check_path overlay_base.test.dtb not-exists "/__fixups__"
    run_test check_path overlay_base.test.dtb not-exists "/__local_fixups__"

    run_dtc_test -I dts -O dtb -o overlay_overlay.test.dtb overlay_overlay.dts
    run_test check_path overlay_overlay.test.dtb not-exists "/__symbols__"
    run_test check_path overlay_overlay.test.dtb exists "/__fixups__"
    run_test check_path overlay_overlay.test.dtb exists "/__local_fixups__"

    run_test overlay overlay_base.test.dtb overlay_overlay.test.dtb

    # test plugin source to dtb and back
    run_dtc_test -I dtb -O dts -o overlay_overlay_decompile.test.dts overlay_overlay.test.dtb
    run_dtc_test -I dts -O dtb -o overlay_overlay_decompile.test.dtb overlay_overlay_decompile.test.dts
    run_test dtbs_equal_ordered overlay_overlay.test.dtb overlay_overlay_decompile.test.dtb

    # Test generation of aliases insted of symbols
    run_dtc_test -A -I dts -O dtb -o overlay_base_with_aliases.dtb overlay_base.dts
    run_test check_path overlay_base_with_aliases.dtb exists "/aliases"
    run_test check_path overlay_base_with_aliases.dtb not-exists "/__symbols__"
    run_test check_path overlay_base_with_aliases.dtb not-exists "/__fixups__"
    run_test check_path overlay_base_with_aliases.dtb not-exists "/__local_fixups__"
}

tree1_tests () {
    TREE=$1

    # Read-only tests
    run_test get_mem_rsv $TREE
    run_test root_node $TREE
    run_test find_property $TREE
    run_test subnode_offset $TREE
    run_test path_offset $TREE
    run_test get_name $TREE
    run_test getprop $TREE
    run_test get_phandle $TREE
    run_test get_path $TREE
    run_test supernode_atdepth_offset $TREE
    run_test parent_offset $TREE
    run_test node_offset_by_prop_value $TREE
    run_test node_offset_by_phandle $TREE
    run_test node_check_compatible $TREE
    run_test node_offset_by_compatible $TREE
    run_test notfound $TREE

    # Write-in-place tests
    run_test setprop_inplace $TREE
    run_test nop_property $TREE
    run_test nop_node $TREE
}

tree1_tests_rw () {
    TREE=$1

    # Read-write tests
    run_test set_name $TREE
    run_test setprop $TREE
    run_test del_property $TREE
    run_test del_node $TREE
}

check_tests () {
    tree="$1"
    shift
    run_sh_test dtc-checkfails.sh "$@" -- -I dts -O dtb $tree
    run_dtc_test -I dts -O dtb -o $tree.test.dtb -f $tree
    run_sh_test dtc-checkfails.sh "$@" -- -I dtb -O dtb $tree.test.dtb
}

ALL_LAYOUTS="mts mst tms tsm smt stm"

libfdt_tests () {
    tree1_tests test_tree1.dtb

    run_dtc_test -I dts -O dtb -o addresses.test.dtb addresses.dts
    run_test addr_size_cells addresses.test.dtb

    run_dtc_test -I dts -O dtb -o stringlist.test.dtb stringlist.dts
    run_test stringlist stringlist.test.dtb

    # Sequential write tests
    run_test sw_tree1
    tree1_tests sw_tree1.test.dtb
    tree1_tests unfinished_tree1.test.dtb
    run_test dtbs_equal_ordered test_tree1.dtb sw_tree1.test.dtb

    # Resizing tests
    for mode in resize realloc; do
	run_test sw_tree1 $mode
	tree1_tests sw_tree1.test.dtb
	tree1_tests unfinished_tree1.test.dtb
	run_test dtbs_equal_ordered test_tree1.dtb sw_tree1.test.dtb
    done

    # fdt_move tests
    for tree in test_tree1.dtb sw_tree1.test.dtb unfinished_tree1.test.dtb; do
	rm -f moved.$tree shunted.$tree deshunted.$tree
	run_test move_and_save $tree
	run_test dtbs_equal_ordered $tree moved.$tree
	run_test dtbs_equal_ordered $tree shunted.$tree
	run_test dtbs_equal_ordered $tree deshunted.$tree
    done

    # v16 and alternate layout tests
    for tree in test_tree1.dtb; do
	for version in 17 16; do
	    for layout in $ALL_LAYOUTS; do
		run_test mangle-layout $tree $version $layout
		tree1_tests v$version.$layout.$tree
		run_test dtbs_equal_ordered $tree v$version.$layout.$tree
	    done
	done
    done

    # Read-write tests
    for basetree in test_tree1.dtb; do
	for version in 17 16; do
	    for layout in $ALL_LAYOUTS; do
		tree=v$version.$layout.$basetree
		rm -f opened.$tree repacked.$tree
		run_test open_pack $tree
		tree1_tests opened.$tree
		tree1_tests repacked.$tree

		tree1_tests_rw $tree
		tree1_tests_rw opened.$tree
		tree1_tests_rw repacked.$tree
	    done
	done
    done
    run_test rw_tree1
    tree1_tests rw_tree1.test.dtb
    tree1_tests_rw rw_tree1.test.dtb
    run_test appendprop1
    run_test appendprop2 appendprop1.test.dtb
    run_dtc_test -I dts -O dtb -o appendprop.test.dtb appendprop.dts
    run_test dtbs_equal_ordered appendprop2.test.dtb appendprop.test.dtb
    libfdt_overlay_tests

    for basetree in test_tree1.dtb sw_tree1.test.dtb rw_tree1.test.dtb; do
	run_test nopulate $basetree
	run_test dtbs_equal_ordered $basetree noppy.$basetree
	tree1_tests noppy.$basetree
	tree1_tests_rw noppy.$basetree
    done

    run_dtc_test -I dts -O dtb -o subnode_iterate.dtb subnode_iterate.dts
    run_test subnode_iterate subnode_iterate.dtb

    run_dtc_test -I dts -O dtb -o property_iterate.dtb property_iterate.dts
    run_test property_iterate property_iterate.dtb

    # Tests for behaviour on various sorts of corrupted trees
    run_test truncated_property

    # Check aliases support in fdt_path_offset
    run_dtc_test -I dts -O dtb -o aliases.dtb aliases.dts
    run_test get_alias aliases.dtb
    run_test path_offset_aliases aliases.dtb

    # Specific bug tests
    run_test add_subnode_with_nops
    run_dtc_test -I dts -O dts -o sourceoutput.test.dts sourceoutput.dts
    run_dtc_test -I dts -O dtb -o sourceoutput.test.dtb sourceoutput.dts
    run_dtc_test -I dts -O dtb -o sourceoutput.test.dts.test.dtb sourceoutput.test.dts
    run_test dtbs_equal_ordered sourceoutput.test.dtb sourceoutput.test.dts.test.dtb

    run_dtc_test -I dts -O dtb -o embedded_nul.test.dtb embedded_nul.dts
    run_dtc_test -I dts -O dtb -o embedded_nul_equiv.test.dtb embedded_nul_equiv.dts
    run_test dtbs_equal_ordered embedded_nul.test.dtb embedded_nul_equiv.test.dtb

    run_dtc_test -I dts -O dtb bad-size-cells.dts

    run_wrap_error_test $DTC division-by-zero.dts
    run_wrap_error_test $DTC bad-octal-literal.dts
    run_dtc_test -I dts -O dtb nul-in-escape.dts
    run_wrap_error_test $DTC nul-in-line-info1.dts
    run_wrap_error_test $DTC nul-in-line-info2.dts

    run_wrap_error_test $DTC -I dtb -O dts -o /dev/null ovf_size_strings.dtb
}

dtc_tests () {
    run_dtc_test -I dts -O dtb -o dtc_tree1.test.dtb test_tree1.dts
    tree1_tests dtc_tree1.test.dtb
    tree1_tests_rw dtc_tree1.test.dtb
    run_test dtbs_equal_ordered dtc_tree1.test.dtb test_tree1.dtb

    run_dtc_test -I dts -O dtb -o dtc_escapes.test.dtb propname_escapes.dts
    run_test propname_escapes dtc_escapes.test.dtb

    run_dtc_test -I dts -O dtb -o line_directives.test.dtb line_directives.dts

    run_dtc_test -I dts -O dtb -o dtc_escapes.test.dtb escapes.dts
    run_test string_escapes dtc_escapes.test.dtb

    run_dtc_test -I dts -O dtb -o dtc_char_literal.test.dtb char_literal.dts
    run_test char_literal dtc_char_literal.test.dtb

    run_dtc_test -I dts -O dtb -o dtc_sized_cells.test.dtb sized_cells.dts
    run_test sized_cells dtc_sized_cells.test.dtb

    run_dtc_test -I dts -O dtb -o dtc_extra-terminating-null.test.dtb extra-terminating-null.dts
    run_test extra-terminating-null dtc_extra-terminating-null.test.dtb

    run_dtc_test -I dts -O dtb -o dtc_references.test.dtb references.dts
    run_test references dtc_references.test.dtb

    run_dtc_test -I dts -O dtb -o dtc_path-references.test.dtb path-references.dts
    run_test path-references dtc_path-references.test.dtb

    run_test phandle_format dtc_references.test.dtb both
    for f in legacy epapr both; do
	run_dtc_test -I dts -O dtb -H $f -o dtc_references.test.$f.dtb references.dts
	run_test phandle_format dtc_references.test.$f.dtb $f
    done

    run_dtc_test -I dts -O dtb -o multilabel.test.dtb multilabel.dts
    run_test references multilabel.test.dtb

    run_dtc_test -I dts -O dtb -o label_repeated.test.dtb label_repeated.dts

    run_dtc_test -I dts -O dtb -o dtc_comments.test.dtb comments.dts
    run_dtc_test -I dts -O dtb -o dtc_comments-cmp.test.dtb comments-cmp.dts
    run_test dtbs_equal_ordered dtc_comments.test.dtb dtc_comments-cmp.test.dtb

    # Check /include/ directive
    run_dtc_test -I dts -O dtb -o includes.test.dtb include0.dts
    run_test dtbs_equal_ordered includes.test.dtb test_tree1.dtb

    # Check /incbin/ directive
    run_dtc_test -I dts -O dtb -o incbin.test.dtb incbin.dts
    run_test incbin incbin.test.dtb

    # Check boot_cpuid_phys handling
    run_dtc_test -I dts -O dtb -o boot_cpuid.test.dtb boot-cpuid.dts
    run_test boot-cpuid boot_cpuid.test.dtb 16

    run_dtc_test -I dts -O dtb -b 17 -o boot_cpuid_17.test.dtb boot-cpuid.dts
    run_test boot-cpuid boot_cpuid_17.test.dtb 17

    run_dtc_test -I dtb -O dtb -o preserve_boot_cpuid.test.dtb boot_cpuid.test.dtb
    run_test boot-cpuid preserve_boot_cpuid.test.dtb 16
    run_test dtbs_equal_ordered preserve_boot_cpuid.test.dtb boot_cpuid.test.dtb

    run_dtc_test -I dtb -O dtb -o preserve_boot_cpuid_17.test.dtb boot_cpuid_17.test.dtb
    run_test boot-cpuid preserve_boot_cpuid_17.test.dtb 17
    run_test dtbs_equal_ordered preserve_boot_cpuid_17.test.dtb boot_cpuid_17.test.dtb

    run_dtc_test -I dtb -O dtb -b17 -o override17_boot_cpuid.test.dtb boot_cpuid.test.dtb
    run_test boot-cpuid override17_boot_cpuid.test.dtb 17

    run_dtc_test -I dtb -O dtb -b0 -o override0_boot_cpuid_17.test.dtb boot_cpuid_17.test.dtb
    run_test boot-cpuid override0_boot_cpuid_17.test.dtb 0


    # Check -Oasm mode
    for tree in test_tree1.dts escapes.dts references.dts path-references.dts \
	comments.dts aliases.dts include0.dts incbin.dts \
	value-labels.dts ; do
	run_dtc_test -I dts -O asm -o oasm_$tree.test.s $tree
	asm_to_so_test oasm_$tree
	run_dtc_test -I dts -O dtb -o $tree.test.dtb $tree
	run_test asm_tree_dump ./oasm_$tree.test.so oasm_$tree.test.dtb
	run_wrap_test cmp oasm_$tree.test.dtb $tree.test.dtb
    done

    run_test value-labels ./oasm_value-labels.dts.test.so

    # Check -Odts mode preserve all dtb information
    for tree in test_tree1.dtb dtc_tree1.test.dtb dtc_escapes.test.dtb \
	dtc_extra-terminating-null.test.dtb dtc_references.test.dtb; do
	run_dtc_test -I dtb -O dts -o odts_$tree.test.dts $tree
	run_dtc_test -I dts -O dtb -o odts_$tree.test.dtb odts_$tree.test.dts
	run_test dtbs_equal_ordered $tree odts_$tree.test.dtb
    done

    # Check version conversions
    for tree in test_tree1.dtb ; do
	 for aver in 1 2 3 16 17; do
	     atree="ov${aver}_$tree.test.dtb"
	     run_dtc_test -I dtb -O dtb -V$aver -o $atree $tree
	     for bver in 16 17; do
		 btree="ov${bver}_$atree"
		 run_dtc_test -I dtb -O dtb -V$bver -o $btree $atree
		 run_test dtbs_equal_ordered $btree $tree
	     done
	 done
    done

    # Check merge/overlay functionality
    run_dtc_test -I dts -O dtb -o dtc_tree1_merge.test.dtb test_tree1_merge.dts
    tree1_tests dtc_tree1_merge.test.dtb test_tree1.dtb
    run_dtc_test -I dts -O dtb -o dtc_tree1_merge_labelled.test.dtb test_tree1_merge_labelled.dts
    tree1_tests dtc_tree1_merge_labelled.test.dtb test_tree1.dtb
    run_dtc_test -I dts -O dtb -o dtc_tree1_label_noderef.test.dtb test_tree1_label_noderef.dts
    run_test dtbs_equal_unordered dtc_tree1_label_noderef.test.dtb test_tree1.dtb
    run_dtc_test -I dts -O dtb -o multilabel_merge.test.dtb multilabel_merge.dts
    run_test references multilabel.test.dtb
    run_test dtbs_equal_ordered multilabel.test.dtb multilabel_merge.test.dtb
    run_dtc_test -I dts -O dtb -o dtc_tree1_merge_path.test.dtb test_tree1_merge_path.dts
    tree1_tests dtc_tree1_merge_path.test.dtb test_tree1.dtb
    run_wrap_error_test $DTC -I dts -O dtb -o /dev/null test_label_ref.dts

    # Check prop/node delete functionality
    run_dtc_test -I dts -O dtb -o dtc_tree1_delete.test.dtb test_tree1_delete.dts
    tree1_tests dtc_tree1_delete.test.dtb

    run_dtc_test -I dts -O dts -o delete_reinstate_multilabel.dts.test.dts delete_reinstate_multilabel.dts
    run_wrap_test cmp delete_reinstate_multilabel.dts.test.dts delete_reinstate_multilabel_ref.dts

    # Check some checks
    check_tests dup-nodename.dts duplicate_node_names
    check_tests dup-propname.dts duplicate_property_names
    check_tests dup-phandle.dts explicit_phandles
    check_tests zero-phandle.dts explicit_phandles
    check_tests minusone-phandle.dts explicit_phandles
    run_sh_test dtc-checkfails.sh phandle_references -- -I dts -O dtb nonexist-node-ref.dts
    run_sh_test dtc-checkfails.sh phandle_references -- -I dts -O dtb nonexist-label-ref.dts
    run_sh_test dtc-fatal.sh -I dts -O dtb nonexist-node-ref2.dts
    check_tests bad-name-property.dts name_properties

    check_tests bad-ncells.dts address_cells_is_cell size_cells_is_cell interrupt_cells_is_cell
    check_tests bad-string-props.dts device_type_is_string model_is_string status_is_string
    check_tests bad-reg-ranges.dts reg_format ranges_format
    check_tests bad-empty-ranges.dts ranges_format
    check_tests reg-ranges-root.dts reg_format ranges_format
    check_tests default-addr-size.dts avoid_default_addr_size
    check_tests obsolete-chosen-interrupt-controller.dts obsolete_chosen_interrupt_controller
    check_tests reg-without-unit-addr.dts unit_address_vs_reg
    check_tests unit-addr-without-reg.dts unit_address_vs_reg
    run_sh_test dtc-checkfails.sh node_name_chars -- -I dtb -O dtb bad_node_char.dtb
    run_sh_test dtc-checkfails.sh node_name_format -- -I dtb -O dtb bad_node_format.dtb
    run_sh_test dtc-checkfails.sh prop_name_chars -- -I dtb -O dtb bad_prop_char.dtb

    run_sh_test dtc-checkfails.sh duplicate_label -- -I dts -O dtb reuse-label1.dts
    run_sh_test dtc-checkfails.sh duplicate_label -- -I dts -O dtb reuse-label2.dts
    run_sh_test dtc-checkfails.sh duplicate_label -- -I dts -O dtb reuse-label3.dts
    run_sh_test dtc-checkfails.sh duplicate_label -- -I dts -O dtb reuse-label4.dts
    run_sh_test dtc-checkfails.sh duplicate_label -- -I dts -O dtb reuse-label5.dts
    run_sh_test dtc-checkfails.sh duplicate_label -- -I dts -O dtb reuse-label6.dts

    run_test check_path test_tree1.dtb exists "/subnode@1"
    run_test check_path test_tree1.dtb not-exists "/subnode@10"

    # Check warning options
    run_sh_test dtc-checkfails.sh address_cells_is_cell interrupt_cells_is_cell -n size_cells_is_cell -- -Wno_size_cells_is_cell -I dts -O dtb bad-ncells.dts
    run_sh_test dtc-fails.sh -n test-warn-output.test.dtb -I dts -O dtb bad-ncells.dts
    run_sh_test dtc-fails.sh test-error-output.test.dtb -I dts -O dtb bad-ncells.dts -Esize_cells_is_cell
    run_sh_test dtc-checkfails.sh always_fail -- -Walways_fail -I dts -O dtb test_tree1.dts
    run_sh_test dtc-checkfails.sh -n always_fail -- -Walways_fail -Wno_always_fail -I dts -O dtb test_tree1.dts
    run_sh_test dtc-fails.sh test-negation-1.test.dtb -Ealways_fail -I dts -O dtb test_tree1.dts
    run_sh_test dtc-fails.sh -n test-negation-2.test.dtb -Ealways_fail -Eno_always_fail -I dts -O dtb test_tree1.dts
    run_sh_test dtc-fails.sh test-negation-3.test.dtb -Ealways_fail -Wno_always_fail -I dts -O dtb test_tree1.dts
    run_sh_test dtc-fails.sh -n test-negation-4.test.dtb -Esize_cells_is_cell -Eno_size_cells_is_cell -I dts -O dtb bad-ncells.dts
    run_sh_test dtc-checkfails.sh size_cells_is_cell -- -Esize_cells_is_cell -Eno_size_cells_is_cell -I dts -O dtb bad-ncells.dts

    # Check for proper behaviour reading from stdin
    run_dtc_test -I dts -O dtb -o stdin_dtc_tree1.test.dtb - < test_tree1.dts
    run_wrap_test cmp stdin_dtc_tree1.test.dtb dtc_tree1.test.dtb
    run_dtc_test -I dtb -O dts -o stdin_odts_test_tree1.dtb.test.dts - < test_tree1.dtb
    run_wrap_test cmp stdin_odts_test_tree1.dtb.test.dts odts_test_tree1.dtb.test.dts

    # Check integer expresisons
    run_test integer-expressions -g integer-expressions.test.dts
    run_dtc_test -I dts -O dtb -o integer-expressions.test.dtb integer-expressions.test.dts
    run_test integer-expressions integer-expressions.test.dtb

    # Check for graceful failure in some error conditions
    run_sh_test dtc-fatal.sh -I dts -O dtb nosuchfile.dts
    run_sh_test dtc-fatal.sh -I dtb -O dtb nosuchfile.dtb
    run_sh_test dtc-fatal.sh -I fs -O dtb nosuchfile

    # Dependencies
    run_dtc_test -I dts -O dtb -o dependencies.test.dtb -d dependencies.test.d dependencies.dts
    run_wrap_test cmp dependencies.test.d dependencies.cmp

    # Search paths
    run_wrap_error_test $DTC -I dts -O dtb -o search_paths.dtb search_paths.dts
    run_dtc_test -i search_dir -I dts -O dtb -o search_paths.dtb \
	search_paths.dts
    run_wrap_error_test $DTC -i search_dir_b -I dts -O dtb \
	-o search_paths_b.dtb search_paths_b.dts
    run_dtc_test -i search_dir_b -i search_dir -I dts -O dtb \
	-o search_paths_b.dtb search_paths_b.dts
    run_dtc_test -I dts -O dtb -o search_paths_subdir.dtb \
	search_dir_b/search_paths_subdir.dts

    # Check -a option
    for align in 2 4 8 16 32 64; do
	# -p -a
	run_dtc_test -O dtb -p 1000 -a $align -o align0.dtb subnode_iterate.dts
	check_align align0.dtb $align
	# -S -a
	run_dtc_test -O dtb -S 1999 -a $align -o align1.dtb subnode_iterate.dts
	check_align align1.dtb $align
    done

    # Tests for overlay/plugin generation
    dtc_overlay_tests
}

cmp_tests () {
    basetree="$1"
    shift
    wrongtrees="$@"

    run_test dtb_reverse $basetree

    # First dtbs_equal_ordered
    run_test dtbs_equal_ordered $basetree $basetree
    run_test dtbs_equal_ordered -n $basetree $basetree.reversed.test.dtb
    for tree in $wrongtrees; do
	run_test dtbs_equal_ordered -n $basetree $tree
    done

    # now unordered
    run_test dtbs_equal_unordered $basetree $basetree
    run_test dtbs_equal_unordered $basetree $basetree.reversed.test.dtb
    run_test dtbs_equal_unordered $basetree.reversed.test.dtb $basetree
    for tree in $wrongtrees; do
	run_test dtbs_equal_unordered -n $basetree $tree
    done

    # now dtc --sort
    run_dtc_test -I dtb -O dtb -s -o $basetree.sorted.test.dtb $basetree
    run_test dtbs_equal_unordered $basetree $basetree.sorted.test.dtb
    run_dtc_test -I dtb -O dtb -s -o $basetree.reversed.sorted.test.dtb $basetree.reversed.test.dtb
    run_test dtbs_equal_unordered $basetree.reversed.test.dtb $basetree.reversed.sorted.test.dtb
    run_test dtbs_equal_ordered $basetree.sorted.test.dtb $basetree.reversed.sorted.test.dtb
}

dtbs_equal_tests () {
    WRONG_TREE1=""
    for x in 1 2 3 4 5 6 7 8 9; do
	run_dtc_test -I dts -O dtb -o test_tree1_wrong$x.test.dtb test_tree1_wrong$x.dts
	WRONG_TREE1="$WRONG_TREE1 test_tree1_wrong$x.test.dtb"
    done
    cmp_tests test_tree1.dtb $WRONG_TREE1
}

fdtget_tests () {
    dts=label01.dts
    dtb=$dts.fdtget.test.dtb
    run_dtc_test -O dtb -o $dtb $dts

    # run_fdtget_test <expected-result> [<flags>] <file> <node> <property>
    run_fdtget_test "MyBoardName" $dtb / model
    run_fdtget_test "MyBoardName MyBoardFamilyName" $dtb / compatible
    run_fdtget_test "77 121 66 111 \
97 114 100 78 97 109 101 0 77 121 66 111 97 114 100 70 97 109 105 \
108 121 78 97 109 101 0" -t bu $dtb / compatible
    run_fdtget_test "MyBoardName MyBoardFamilyName" -t s $dtb / compatible
    run_fdtget_test 32768 $dtb /cpus/PowerPC,970@1 d-cache-size
    run_fdtget_test 8000 -tx $dtb /cpus/PowerPC,970@1 d-cache-size
    run_fdtget_test "61 62 63 0" -tbx $dtb /randomnode tricky1
    run_fdtget_test "a b c d de ea ad be ef" -tbx $dtb /randomnode blob

    # Here the property size is not a multiple of 4 bytes, so it should fail
    run_wrap_error_test $DTGET -tlx $dtb /randomnode mixed
    run_fdtget_test "6162 6300 1234 0 a 0 b 0 c" -thx $dtb /randomnode mixed
    run_fdtget_test "61 62 63 0 12 34 0 0 0 a 0 0 0 b 0 0 0 c" \
	-thhx $dtb /randomnode mixed
    run_wrap_error_test $DTGET -ts $dtb /randomnode doctor-who

    # Test multiple arguments
    run_fdtget_test "MyBoardName\nmemory" -ts $dtb / model /memory device_type

    # Test defaults
    run_wrap_error_test $DTGET -tx $dtb /randomnode doctor-who
    run_fdtget_test "<the dead silence>" -tx \
	-d "<the dead silence>" $dtb /randomnode doctor-who
    run_fdtget_test "<blink>" -tx -d "<blink>" $dtb /memory doctor-who
}

fdtput_tests () {
    dts=label01.dts
    dtb=$dts.fdtput.test.dtb
    text=lorem.txt

    # Allow just enough space for $text
    run_dtc_test -O dtb -p $(stat -c %s $text) -o $dtb $dts

    # run_fdtput_test <expected-result> <file> <node> <property> <flags> <value>
    run_fdtput_test "a_model" $dtb / model -ts "a_model"
    run_fdtput_test "board1 board2" $dtb / compatible -ts board1 board2
    run_fdtput_test "board1 board2" $dtb / compatible -ts "board1 board2"
    run_fdtput_test "32768" $dtb /cpus/PowerPC,970@1 d-cache-size "" "32768"
    run_fdtput_test "8001" $dtb /cpus/PowerPC,970@1 d-cache-size -tx 0x8001
    run_fdtput_test "2 3 12" $dtb /randomnode tricky1 -tbi "02 003 12"
    run_fdtput_test "a b c ea ad be ef" $dtb /randomnode blob \
	-tbx "a b c ea ad be ef"
    run_fdtput_test "a0b0c0d deeaae ef000000" $dtb /randomnode blob \
	-tx "a0b0c0d deeaae ef000000"
    run_fdtput_test "$(cat $text)" $dtb /randomnode blob -ts "$(cat $text)"

    # Test expansion of the blob when insufficient room for property
    run_fdtput_test "$(cat $text $text)" $dtb /randomnode blob -ts "$(cat $text $text)"

    # Start again with a fresh dtb
    run_dtc_test -O dtb -p $(stat -c %s $text) -o $dtb $dts

    # Node creation
    run_wrap_error_test $DTPUT $dtb -c /baldrick sod
    run_wrap_test $DTPUT $dtb -c /chosen/son /chosen/daughter
    run_fdtput_test "eva" $dtb /chosen/daughter name "" -ts "eva"
    run_fdtput_test "adam" $dtb /chosen/son name "" -ts "adam"

    # Not allowed to create an existing node
    run_wrap_error_test $DTPUT $dtb -c /chosen
    run_wrap_error_test $DTPUT $dtb -c /chosen/son

    # Automatic node creation
    run_wrap_test $DTPUT $dtb -cp /blackadder/the-second/turnip \
	/blackadder/the-second/potato
    run_fdtput_test 1000 $dtb /blackadder/the-second/turnip cost "" 1000
    run_fdtput_test "fine wine" $dtb /blackadder/the-second/potato drink \
	"-ts" "fine wine"
    run_wrap_test $DTPUT $dtb -p /you/are/drunk/sir/winston slurp -ts twice

    # Test expansion of the blob when insufficent room for a new node
    run_wrap_test $DTPUT $dtb -cp "$(cat $text $text)/longish"

    # Allowed to create an existing node with -p
    run_wrap_test $DTPUT $dtb -cp /chosen
    run_wrap_test $DTPUT $dtb -cp /chosen/son

    # Start again with a fresh dtb
    run_dtc_test -O dtb -p $(stat -c %s $text) -o $dtb $dts

    # Node delete
    run_wrap_test $DTPUT $dtb -c /chosen/node1 /chosen/node2 /chosen/node3
    run_fdtget_test "node3\nnode2\nnode1" $dtb -l  /chosen
    run_wrap_test $DTPUT $dtb -r /chosen/node1 /chosen/node2
    run_fdtget_test "node3" $dtb -l  /chosen

    # Delete the non-existent node
    run_wrap_error_test $DTPUT $dtb -r /non-existent/node

    # Property delete
    run_fdtput_test "eva" $dtb /chosen/ name "" -ts "eva"
    run_fdtput_test "016" $dtb /chosen/ age  "" -ts "016"
    run_fdtget_test "age\nname\nbootargs\nlinux,platform" $dtb -p  /chosen
    run_wrap_test $DTPUT $dtb -d /chosen/ name age
    run_fdtget_test "bootargs\nlinux,platform" $dtb -p  /chosen

    # Delete the non-existent property
    run_wrap_error_test $DTPUT $dtb -d /chosen   non-existent-prop

    # TODO: Add tests for verbose mode?
}

utilfdt_tests () {
    run_test utilfdt_test
}

fdtdump_tests () {
    run_fdtdump_test fdtdump.dts
}

while getopts "vt:me" ARG ; do
    case $ARG in
	"v")
	    unset QUIET_TEST
	    ;;
	"t")
	    TESTSETS=$OPTARG
	    ;;
	"m")
	    VALGRIND="valgrind --tool=memcheck -q --error-exitcode=$VGCODE"
	    ;;
	"e")
	    STOP_ON_FAIL=1
	    ;;
    esac
done

if [ -z "$TESTSETS" ]; then
    TESTSETS="libfdt utilfdt dtc dtbs_equal fdtget fdtput fdtdump"
fi

# Make sure we don't have stale blobs lying around
rm -f *.test.dtb *.test.dts

for set in $TESTSETS; do
    case $set in
	"libfdt")
	    libfdt_tests
	    ;;
	"utilfdt")
	    utilfdt_tests
	    ;;
	"dtc")
	    dtc_tests
	    ;;
	"dtbs_equal")
	    dtbs_equal_tests
	    ;;
	"fdtget")
	    fdtget_tests
	    ;;
	"fdtput")
	    fdtput_tests
	    ;;
	"fdtdump")
	    fdtdump_tests
	    ;;
    esac
done

echo "********** TEST SUMMARY"
echo "*     Total testcases:	$tot_tests"
echo "*                PASS:	$tot_pass"
echo "*                FAIL:	$tot_fail"
echo "*   Bad configuration:	$tot_config"
if [ -n "$VALGRIND" ]; then
    echo "*    valgrind errors:	$tot_vg"
fi
echo "* Strange test result:	$tot_strange"
echo "**********"


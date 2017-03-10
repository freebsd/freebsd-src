#include <fdt.h>
#include "testdata.h"

#define FDTLONG(val) \
	.byte	((val) >> 24) & 0xff ; \
	.byte	((val) >> 16) & 0xff ; \
	.byte	((val) >> 8) & 0xff ; \
	.byte	(val) & 0xff	;

#define FDTQUAD(val) \
	.byte	((val) >> 56) & 0xff ; \
	.byte	((val) >> 48) & 0xff ; \
	.byte	((val) >> 40) & 0xff ; \
	.byte	((val) >> 32) & 0xff ; \
	.byte	((val) >> 24) & 0xff ; \
	.byte	((val) >> 16) & 0xff ; \
	.byte	((val) >> 8) & 0xff ; \
	.byte	(val) & 0xff	;

#define TREE_HDR(tree) \
	.balign	8		; \
	.globl	_##tree		; \
_##tree:	\
tree:	\
	FDTLONG(FDT_MAGIC)	; \
	FDTLONG(tree##_end - tree) ; \
	FDTLONG(tree##_struct - tree) ; \
	FDTLONG(tree##_strings - tree) ; \
	FDTLONG(tree##_rsvmap - tree) ; \
	FDTLONG(0x11)		; \
	FDTLONG(0x10)		; \
	FDTLONG(0)		; \
	FDTLONG(tree##_strings_end - tree##_strings) ; \
	FDTLONG(tree##_struct_end - tree##_struct) ;

#define RSVMAP_ENTRY(addr, len) \
	FDTQUAD(addr)		; \
	FDTQUAD(len)		; \

#define EMPTY_RSVMAP(tree) \
	.balign	8		; \
tree##_rsvmap:			; \
	RSVMAP_ENTRY(0, 0) \
tree##_rsvmap_end:		;

#define PROPHDR(tree, name, len) \
	FDTLONG(FDT_PROP)	; \
	FDTLONG(len)		; \
	FDTLONG(tree##_##name - tree##_strings) ;

#define PROP_INT(tree, name, val) \
	PROPHDR(tree, name, 4) \
	FDTLONG(val)		;

#define PROP_INT64(tree, name, val) \
	PROPHDR(tree, name, 8) \
	FDTQUAD(val)		;

#define PROP_STR(tree, name, str) \
	PROPHDR(tree, name, 55f - 54f) \
54:	\
	.string	str		; \
55:	\
	.balign	4		;

#define BEGIN_NODE(name) \
	FDTLONG(FDT_BEGIN_NODE)	; \
	.string	name		; \
	.balign 4		;

#define END_NODE \
	FDTLONG(FDT_END_NODE)	;

#define STRING(tree, name, str) \
tree##_##name:			; \
	.string	str		;

	.data

	TREE_HDR(test_tree1)

	.balign	8
test_tree1_rsvmap:
	RSVMAP_ENTRY(TEST_ADDR_1, TEST_SIZE_1)
	RSVMAP_ENTRY(TEST_ADDR_2, TEST_SIZE_2)
	RSVMAP_ENTRY(0, 0)
test_tree1_rsvmap_end:

test_tree1_struct:
	BEGIN_NODE("")
	PROP_STR(test_tree1, compatible, "test_tree1")
	PROP_INT(test_tree1, prop_int, TEST_VALUE_1)
	PROP_INT64(test_tree1, prop_int64, TEST_VALUE64_1)
	PROP_STR(test_tree1, prop_str, TEST_STRING_1)
	PROP_INT(test_tree1, address_cells, 1)
	PROP_INT(test_tree1, size_cells, 0)

	BEGIN_NODE("subnode@1")
	PROP_STR(test_tree1, compatible, "subnode1")
	PROP_INT(test_tree1, reg, 1)
	PROP_INT(test_tree1, prop_int, TEST_VALUE_1)

	BEGIN_NODE("subsubnode")
	PROP_STR(test_tree1, compatible, "subsubnode1\0subsubnode")
	PROP_INT(test_tree1, prop_int, TEST_VALUE_1)
	END_NODE

	BEGIN_NODE("ss1")
	END_NODE

	END_NODE

	BEGIN_NODE("subnode@2")
	PROP_INT(test_tree1, reg, 2)
	PROP_INT(test_tree1, linux_phandle, PHANDLE_1)
	PROP_INT(test_tree1, prop_int, TEST_VALUE_2)
	PROP_INT(test_tree1, address_cells, 1)
	PROP_INT(test_tree1, size_cells, 0)

	BEGIN_NODE("subsubnode@0")
	PROP_INT(test_tree1, reg, 0)
	PROP_INT(test_tree1, phandle, PHANDLE_2)
	PROP_STR(test_tree1, compatible, "subsubnode2\0subsubnode")
	PROP_INT(test_tree1, prop_int, TEST_VALUE_2)
	END_NODE

	BEGIN_NODE("ss2")
	END_NODE

	END_NODE

	END_NODE
	FDTLONG(FDT_END)
test_tree1_struct_end:

test_tree1_strings:
	STRING(test_tree1, compatible, "compatible")
	STRING(test_tree1, prop_int, "prop-int")
	STRING(test_tree1, prop_int64, "prop-int64")
	STRING(test_tree1, prop_str, "prop-str")
	STRING(test_tree1, linux_phandle, "linux,phandle")
	STRING(test_tree1, phandle, "phandle")
	STRING(test_tree1, reg, "reg")
	STRING(test_tree1, address_cells, "#address-cells")
	STRING(test_tree1, size_cells, "#size-cells")
test_tree1_strings_end:
test_tree1_end:


	TREE_HDR(truncated_property)
	EMPTY_RSVMAP(truncated_property)

truncated_property_struct:
	BEGIN_NODE("")
	PROPHDR(truncated_property, prop_truncated, 4)
	/* Oops, no actual property data here */
truncated_property_struct_end:

truncated_property_strings:
	STRING(truncated_property, prop_truncated, "truncated")
truncated_property_strings_end:

truncated_property_end:


	TREE_HDR(bad_node_char)
	EMPTY_RSVMAP(bad_node_char)

bad_node_char_struct:
	BEGIN_NODE("")
	BEGIN_NODE("sub$node")
	END_NODE
	END_NODE
	FDTLONG(FDT_END)
bad_node_char_struct_end:

bad_node_char_strings:
bad_node_char_strings_end:
bad_node_char_end:


	TREE_HDR(bad_node_format)
	EMPTY_RSVMAP(bad_node_format)

bad_node_format_struct:
	BEGIN_NODE("")
	BEGIN_NODE("subnode@1@2")
	END_NODE
	END_NODE
	FDTLONG(FDT_END)
bad_node_format_struct_end:

bad_node_format_strings:
bad_node_format_strings_end:
bad_node_format_end:


	TREE_HDR(bad_prop_char)
	EMPTY_RSVMAP(bad_prop_char)

bad_prop_char_struct:
	BEGIN_NODE("")
	PROP_INT(bad_prop_char, prop, TEST_VALUE_1)
	END_NODE
	FDTLONG(FDT_END)
bad_prop_char_struct_end:

bad_prop_char_strings:
	STRING(bad_prop_char, prop, "prop$erty")
bad_prop_char_strings_end:
bad_prop_char_end:


	/* overflow_size_strings */
	.balign	8
	.globl	_ovf_size_strings
_ovf_size_strings:
ovf_size_strings:
	FDTLONG(FDT_MAGIC)
	FDTLONG(ovf_size_strings_end - ovf_size_strings)
	FDTLONG(ovf_size_strings_struct - ovf_size_strings) 
	FDTLONG(ovf_size_strings_strings - ovf_size_strings)
	FDTLONG(ovf_size_strings_rsvmap - ovf_size_strings)
	FDTLONG(0x11)
	FDTLONG(0x10)
	FDTLONG(0)
	FDTLONG(0xffffffff)
	FDTLONG(ovf_size_strings_struct_end - ovf_size_strings_struct)
	EMPTY_RSVMAP(ovf_size_strings)

ovf_size_strings_struct:
	BEGIN_NODE("")
	PROP_INT(ovf_size_strings, bad_string, 0)
	END_NODE
	FDTLONG(FDT_END)
ovf_size_strings_struct_end:

ovf_size_strings_strings:
	STRING(ovf_size_strings, x, "x")
	ovf_size_strings_bad_string = ovf_size_strings_strings + 0x10000000
ovf_size_strings_strings_end:
ovf_size_strings_end:

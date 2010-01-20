/*-
 * Copyright (c) 2007 John Birrell (jb@freebsd.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <stdlib.h>
#include <string.h>
#include "_libdwarf.h"

const char *
get_sht_desc(uint32_t sh_type)
{
	switch (sh_type) {
	case SHT_NULL:
		return "inactive";
	case SHT_PROGBITS:
		return "program defined information";
	case SHT_SYMTAB:
		return "symbol table section";
	case SHT_STRTAB:
		return "string table section";
	case SHT_RELA:
		return "relocation section with addends";
	case SHT_HASH:
		return "symbol hash table section";
	case SHT_DYNAMIC:
		return "dynamic section";
	case SHT_NOTE:
		return "note section";
	case SHT_NOBITS:
		return "no space section";
	case SHT_REL:
		return "relocation section - no addends";
	case SHT_SHLIB:
		return "reserved - purpose unknown";
	case SHT_DYNSYM:
		return "dynamic symbol table section";
	case SHT_INIT_ARRAY:
		return "Initialization function pointers.";
	case SHT_FINI_ARRAY:
		return "Termination function pointers.";
	case SHT_PREINIT_ARRAY:
		return "Pre-initialization function ptrs.";
	case SHT_GROUP:
		return "Section group.";
	case SHT_SYMTAB_SHNDX:
		return "Section indexes (see SHN_XINDEX).";
	case SHT_GNU_verdef:
		return "Symbol versions provided";
	case SHT_GNU_verneed:
		return "Symbol versions required";
	case SHT_GNU_versym:
		return "Symbol version table";
	case SHT_AMD64_UNWIND:
		return "AMD64 unwind";
	default:
		return "Unknown";
	}
}

const char *
get_attr_desc(uint32_t attr)
{
	switch (attr) {
	case DW_AT_abstract_origin:
		return "DW_AT_abstract_origin";
	case DW_AT_accessibility:
		return "DW_AT_accessibility";
	case DW_AT_address_class:
		return "DW_AT_address_class";
	case DW_AT_artificial:
		return "DW_AT_artificial";
	case DW_AT_base_types:
		return "DW_AT_base_types";
	case DW_AT_bit_offset:
		return "DW_AT_bit_offset";
	case DW_AT_bit_size:
		return "DW_AT_bit_size";
	case DW_AT_byte_size:
		return "DW_AT_byte_size";
	case DW_AT_calling_convention:
		return "DW_AT_calling_convention";
	case DW_AT_common_reference:
		return "DW_AT_common_reference";
	case DW_AT_comp_dir:
		return "DW_AT_comp_dir";
	case DW_AT_const_value:
		return "DW_AT_const_value";
	case DW_AT_containing_type:
		return "DW_AT_containing_type";
	case DW_AT_count:
		return "DW_AT_count";
	case DW_AT_data_member_location:
		return "DW_AT_data_member_location";
	case DW_AT_decl_column:
		return "DW_AT_decl_column";
	case DW_AT_decl_file:
		return "DW_AT_decl_file";
	case DW_AT_decl_line:
		return "DW_AT_decl_line";
	case DW_AT_declaration:
		return "DW_AT_declaration";
	case DW_AT_default_value:
		return "DW_AT_default_value";
	case DW_AT_discr:
		return "DW_AT_discr";
	case DW_AT_discr_list:
		return "DW_AT_discr_list";
	case DW_AT_discr_value:
		return "DW_AT_discr_value";
	case DW_AT_element_list:
		return "DW_AT_element_list";
	case DW_AT_encoding:
		return "DW_AT_encoding";
	case DW_AT_external:
		return "DW_AT_external";
	case DW_AT_frame_base:
		return "DW_AT_frame_base";
	case DW_AT_friend:
		return "DW_AT_friend";
	case DW_AT_high_pc:
		return "DW_AT_high_pc";
	case DW_AT_identifier_case:
		return "DW_AT_identifier_case";
	case DW_AT_import:
		return "DW_AT_import";
	case DW_AT_inline:
		return "DW_AT_inline";
	case DW_AT_is_optional:
		return "DW_AT_is_optional";
	case DW_AT_language:
		return "DW_AT_language";
	case DW_AT_location:
		return "DW_AT_location";
	case DW_AT_low_pc:
		return "DW_AT_low_pc";
	case DW_AT_lower_bound:
		return "DW_AT_lower_bound";
	case DW_AT_macro_info:
		return "DW_AT_macro_info";
	case DW_AT_member:
		return "DW_AT_member";
	case DW_AT_name:
		return "DW_AT_name";
	case DW_AT_namelist_item:
		return "DW_AT_namelist_item";
	case DW_AT_ordering:
		return "DW_AT_ordering";
	case DW_AT_priority:
		return "DW_AT_priority";
	case DW_AT_producer:
		return "DW_AT_producer";
	case DW_AT_prototyped:
		return "DW_AT_prototyped";
	case DW_AT_return_addr:
		return "DW_AT_return_addr";
	case DW_AT_segment:
		return "DW_AT_segment";
	case DW_AT_sibling:
		return "DW_AT_sibling";
	case DW_AT_specification:
		return "DW_AT_specification";
	case DW_AT_start_scope:
		return "DW_AT_start_scope";
	case DW_AT_static_link:
		return "DW_AT_static_link";
	case DW_AT_stmt_list:
		return "DW_AT_stmt_list";
	case DW_AT_stride_size:
		return "DW_AT_stride_size";
	case DW_AT_string_length:
		return "DW_AT_string_length";
	case DW_AT_subscr_data:
		return "DW_AT_subscr_data";
	case DW_AT_type:
		return "DW_AT_type";
	case DW_AT_upper_bound:
		return "DW_AT_upper_bound";
	case DW_AT_use_location:
		return "DW_AT_use_location";
	case DW_AT_variable_parameter:
		return "DW_AT_variable_parameter";
	case DW_AT_virtuality:
		return "DW_AT_virtuality";
	case DW_AT_visibility:
		return "DW_AT_visibility";
	case DW_AT_vtable_elem_location:
		return "DW_AT_vtable_elem_location";
	default:
		break;
	}

	return "Unknown attribute";
}

const char *
get_form_desc(uint32_t form)
{
	switch (form) {
	case DW_FORM_addr:
		return "DW_FORM_addr";
	case DW_FORM_block:
		return "DW_FORM_block";
	case DW_FORM_block1:
		return "DW_FORM_block1";
	case DW_FORM_block2:
		return "DW_FORM_block2";
	case DW_FORM_block4:
		return "DW_FORM_block4";
	case DW_FORM_data1:
		return "DW_FORM_data1";
	case DW_FORM_data2:
		return "DW_FORM_data2";
	case DW_FORM_data4:
		return "DW_FORM_data4";
	case DW_FORM_data8:
		return "DW_FORM_data8";
	case DW_FORM_flag:
		return "DW_FORM_flag";
	case DW_FORM_indirect:
		return "DW_FORM_indirect";
	case DW_FORM_ref1:
		return "DW_FORM_ref1";
	case DW_FORM_ref2:
		return "DW_FORM_ref2";
	case DW_FORM_ref4:
		return "DW_FORM_ref4";
	case DW_FORM_ref8:
		return "DW_FORM_ref8";
	case DW_FORM_ref_addr:
		return "DW_FORM_ref_addr";
	case DW_FORM_ref_udata:
		return "DW_FORM_ref_udata";
	case DW_FORM_sdata:
		return "DW_FORM_sdata";
	case DW_FORM_string:
		return "DW_FORM_string";
	case DW_FORM_strp:
		return "DW_FORM_strp";
	case DW_FORM_udata:
		return "DW_FORM_udata";
	default:
		break;
	}

	return "Unknown attribute";
}

const char *
get_tag_desc(uint32_t tag)
{
	switch (tag) {
	case DW_TAG_access_declaration:
		return "DW_TAG_access_declaration";
	case DW_TAG_array_type:
		return "DW_TAG_array_type";
	case DW_TAG_base_type:
		return "DW_TAG_base_type";
	case DW_TAG_catch_block:
		return "DW_TAG_catch_block";
	case DW_TAG_class_type:
		return "DW_TAG_class_type";
	case DW_TAG_common_block:
		return "DW_TAG_common_block";
	case DW_TAG_common_inclusion:
		return "DW_TAG_common_inclusion";
	case DW_TAG_compile_unit:
		return "DW_TAG_compile_unit";
	case DW_TAG_condition:
		return "DW_TAG_condition";
	case DW_TAG_const_type:
		return "DW_TAG_const_type";
	case DW_TAG_constant:
		return "DW_TAG_constant";
	case DW_TAG_dwarf_procedure:
		return "DW_TAG_dwarf_procedure";
	case DW_TAG_entry_point:
		return "DW_TAG_entry_point";
	case DW_TAG_enumeration_type:
		return "DW_TAG_enumeration_type";
	case DW_TAG_enumerator:
		return "DW_TAG_enumerator";
	case DW_TAG_formal_parameter:
		return "DW_TAG_formal_parameter";
	case DW_TAG_friend:
		return "DW_TAG_friend";
	case DW_TAG_imported_declaration:
		return "DW_TAG_imported_declaration";
	case DW_TAG_imported_module:
		return "DW_TAG_imported_module";
	case DW_TAG_imported_unit:
		return "DW_TAG_imported_unit";
	case DW_TAG_inheritance:
		return "DW_TAG_inheritance";
	case DW_TAG_inlined_subroutine:
		return "DW_TAG_inlined_subroutine";
	case DW_TAG_interface_type:
		return "DW_TAG_interface_type";
	case DW_TAG_label:
		return "DW_TAG_label";
	case DW_TAG_lexical_block:
		return "DW_TAG_lexical_block";
	case DW_TAG_member:
		return "DW_TAG_member";
	case DW_TAG_module:
		return "DW_TAG_module";
	case DW_TAG_namelist:
		return "DW_TAG_namelist";
	case DW_TAG_namelist_item:
		return "DW_TAG_namelist_item";
	case DW_TAG_namespace:
		return "DW_TAG_namespace";
	case DW_TAG_packed_type:
		return "DW_TAG_packed_type";
	case DW_TAG_partial_unit:
		return "DW_TAG_partial_unit";
	case DW_TAG_pointer_type:
		return "DW_TAG_pointer_type";
	case DW_TAG_ptr_to_member_type:
		return "DW_TAG_ptr_to_member_type";
	case DW_TAG_reference_type:
		return "DW_TAG_reference_type";
	case DW_TAG_restrict_type:
		return "DW_TAG_restrict_type";
	case DW_TAG_set_type:
		return "DW_TAG_set_type";
	case DW_TAG_shared_type:
		return "DW_TAG_shared_type";
	case DW_TAG_string_type:
		return "DW_TAG_string_type";
	case DW_TAG_structure_type:
		return "DW_TAG_structure_type";
	case DW_TAG_subprogram:
		return "DW_TAG_subprogram";
	case DW_TAG_subrange_type:
		return "DW_TAG_subrange_type";
	case DW_TAG_subroutine_type:
		return "DW_TAG_subroutine_type";
	case DW_TAG_template_type_parameter:
		return "DW_TAG_template_type_parameter";
	case DW_TAG_template_value_parameter:
		return "DW_TAG_template_value_parameter";
	case DW_TAG_thrown_type:
		return "DW_TAG_thrown_type";
	case DW_TAG_try_block:
		return "DW_TAG_try_block";
	case DW_TAG_typedef:
		return "DW_TAG_typedef";
	case DW_TAG_union_type:
		return "DW_TAG_union_type";
	case DW_TAG_unspecified_parameters:
		return "DW_TAG_unspecified_parameters";
	case DW_TAG_unspecified_type:
		return "DW_TAG_unspecified_type";
	case DW_TAG_variable:
		return "DW_TAG_variable";
	case DW_TAG_variant:
		return "DW_TAG_variant";
	case DW_TAG_variant_part:
		return "DW_TAG_variant_part";
	case DW_TAG_volatile_type:
		return "DW_TAG_volatile_type";
	case DW_TAG_with_stmt:
		return "DW_TAG_with_stmt";
	default:
		break;
	}

	return "Unknown tag";
}

void
dwarf_dump_abbrev(Dwarf_Debug dbg)
{
	Dwarf_Abbrev a;
	Dwarf_Attribute at;
	Dwarf_CU cu;

	printf("Contents of the .debug_abbrev section:\n\nEntry Tag\n");

	STAILQ_FOREACH(cu, &dbg->dbg_cu, cu_next) {
		STAILQ_FOREACH(a, &cu->cu_abbrev, a_next) {
			printf("%5lu %-30s [%s children]\n",
			    (u_long) a->a_entry, get_tag_desc(a->a_tag),
			    (a->a_children == DW_CHILDREN_yes) ? "has" : "no");

			STAILQ_FOREACH(at, &a->a_attrib, at_next)
				printf("      %-30s %s\n", get_attr_desc(at->at_attrib),
				    get_form_desc(at->at_form));
		}
	}
}
#ifdef DOODAD
    case DW_AT_inline:
      switch (uvalue)
	{
	case DW_INL_not_inlined:
	  printf (_("(not inlined)"));
	  break;
	case DW_INL_inlined:
	  printf (_("(inlined)"));
	  break;
	case DW_INL_declared_not_inlined:
	  printf (_("(declared as inline but ignored)"));
	  break;
	case DW_INL_declared_inlined:
	  printf (_("(declared as inline and inlined)"));
	  break;
	default:
	  printf (_("  (Unknown inline attribute value: %lx)"), uvalue);
	  break;
	}
      break;

    case DW_AT_language:
      switch (uvalue)
	{
	case DW_LANG_C:			printf ("(non-ANSI C)"); break;
	case DW_LANG_C89:		printf ("(ANSI C)"); break;
	case DW_LANG_C_plus_plus:	printf ("(C++)"); break;
	case DW_LANG_Fortran77:		printf ("(FORTRAN 77)"); break;
	case DW_LANG_Fortran90:		printf ("(Fortran 90)"); break;
	case DW_LANG_Modula2:		printf ("(Modula 2)"); break;
	case DW_LANG_Pascal83:		printf ("(ANSI Pascal)"); break;
	case DW_LANG_Ada83:		printf ("(Ada)"); break;
	case DW_LANG_Cobol74:		printf ("(Cobol 74)"); break;
	case DW_LANG_Cobol85:		printf ("(Cobol 85)"); break;
	  /* DWARF 2.1 values.	*/
	case DW_LANG_C99:		printf ("(ANSI C99)"); break;
	case DW_LANG_Ada95:		printf ("(ADA 95)"); break;
	case DW_LANG_Fortran95:		printf ("(Fortran 95)"); break;
	  /* MIPS extension.  */
	case DW_LANG_Mips_Assembler:	printf ("(MIPS assembler)"); break;
	  /* UPC extension.  */
	case DW_LANG_Upc:		printf ("(Unified Parallel C)"); break;
	default:
	  printf ("(Unknown: %lx)", uvalue);
	  break;
	}
      break;

    case DW_AT_encoding:
      switch (uvalue)
	{
	case DW_ATE_void:		printf ("(void)"); break;
	case DW_ATE_address:		printf ("(machine address)"); break;
	case DW_ATE_boolean:		printf ("(boolean)"); break;
	case DW_ATE_complex_float:	printf ("(complex float)"); break;
	case DW_ATE_float:		printf ("(float)"); break;
	case DW_ATE_signed:		printf ("(signed)"); break;
	case DW_ATE_signed_char:	printf ("(signed char)"); break;
	case DW_ATE_unsigned:		printf ("(unsigned)"); break;
	case DW_ATE_unsigned_char:	printf ("(unsigned char)"); break;
	  /* DWARF 2.1 value.  */
	case DW_ATE_imaginary_float:	printf ("(imaginary float)"); break;
	default:
	  if (uvalue >= DW_ATE_lo_user
	      && uvalue <= DW_ATE_hi_user)
	    printf ("(user defined type)");
	  else
	    printf ("(unknown type)");
	  break;
	}
      break;

    case DW_AT_accessibility:
      switch (uvalue)
	{
	case DW_ACCESS_public:		printf ("(public)"); break;
	case DW_ACCESS_protected:	printf ("(protected)"); break;
	case DW_ACCESS_private:		printf ("(private)"); break;
	default:
	  printf ("(unknown accessibility)");
	  break;
	}
      break;

    case DW_AT_visibility:
      switch (uvalue)
	{
	case DW_VIS_local:		printf ("(local)"); break;
	case DW_VIS_exported:		printf ("(exported)"); break;
	case DW_VIS_qualified:		printf ("(qualified)"); break;
	default:			printf ("(unknown visibility)"); break;
	}
      break;

    case DW_AT_virtuality:
      switch (uvalue)
	{
	case DW_VIRTUALITY_none:	printf ("(none)"); break;
	case DW_VIRTUALITY_virtual:	printf ("(virtual)"); break;
	case DW_VIRTUALITY_pure_virtual:printf ("(pure_virtual)"); break;
	default:			printf ("(unknown virtuality)"); break;
	}
      break;

    case DW_AT_identifier_case:
      switch (uvalue)
	{
	case DW_ID_case_sensitive:	printf ("(case_sensitive)"); break;
	case DW_ID_up_case:		printf ("(up_case)"); break;
	case DW_ID_down_case:		printf ("(down_case)"); break;
	case DW_ID_case_insensitive:	printf ("(case_insensitive)"); break;
	default:			printf ("(unknown case)"); break;
	}
      break;

    case DW_AT_calling_convention:
      switch (uvalue)
	{
	case DW_CC_normal:	printf ("(normal)"); break;
	case DW_CC_program:	printf ("(program)"); break;
	case DW_CC_nocall:	printf ("(nocall)"); break;
	default:
	  if (uvalue >= DW_CC_lo_user
	      && uvalue <= DW_CC_hi_user)
	    printf ("(user defined)");
	  else
	    printf ("(unknown convention)");
	}
      break;

    case DW_AT_ordering:
      switch (uvalue)
	{
	case -1: printf ("(undefined)"); break;
	case 0:  printf ("(row major)"); break;
	case 1:  printf ("(column major)"); break;
	}
      break;

    case DW_AT_frame_base:
    case DW_AT_location:
    case DW_AT_data_member_location:
    case DW_AT_vtable_elem_location:
    case DW_AT_allocated:
    case DW_AT_associated:
    case DW_AT_data_location:
    case DW_AT_stride:
    case DW_AT_upper_bound:
    case DW_AT_lower_bound:
      if (block_start)
	{
	  printf ("(");
	  decode_location_expression (block_start, pointer_size, uvalue);
	  printf (")");
	}
      else if (form == DW_FORM_data4 || form == DW_FORM_data8)
	{
	  printf ("(");
	  printf ("location list");
	  printf (")");
	}
      break;
#endif

static void
dwarf_dump_av_attr(Dwarf_Die die __unused, Dwarf_AttrValue av)
{
	switch (av->av_attrib) {
	case DW_AT_accessibility:
		break;

	case DW_AT_calling_convention:
		break;

	case DW_AT_encoding:
		break;

	case DW_AT_identifier_case:
		break;

	case DW_AT_inline:
		break;

	case DW_AT_language:
		break;

	case DW_AT_ordering:
		break;

	case DW_AT_virtuality:
		break;

	case DW_AT_visibility:
		break;

	case DW_AT_frame_base:
	case DW_AT_location:
	case DW_AT_data_member_location:
	case DW_AT_vtable_elem_location:
	case DW_AT_upper_bound:
	case DW_AT_lower_bound:
		break;

	default:
		break;
	}
}

void
dwarf_dump_av(Dwarf_Die die, Dwarf_AttrValue av)
{
	uint64_t i;

	printf("      %-30s : %-16s ",
	    get_attr_desc(av->av_attrib),
	    get_form_desc(av->av_form));

	switch (av->av_form) {
	case DW_FORM_addr:
		printf("0x%llx", (unsigned long long) av->u[0].u64);
		break;
	case DW_FORM_block:
	case DW_FORM_block1:
	case DW_FORM_block2:
	case DW_FORM_block4:
		printf("%lu byte block:", (u_long) av->u[0].u64);
		for (i = 0; i < av->u[0].u64; i++)
			printf(" %02x", av->u[1].u8p[i]);
		break;
	case DW_FORM_data1:
	case DW_FORM_data2:
	case DW_FORM_data4:
	case DW_FORM_data8:
	case DW_FORM_flag:
		printf("%llu", (unsigned long long) av->u[0].u64);
		break;
	case DW_FORM_ref1:
	case DW_FORM_ref2:
	case DW_FORM_ref4:
	case DW_FORM_ref8:
	case DW_FORM_ref_udata:
		printf("<%llx>", (unsigned long long) (av->u[0].u64 +
		    die->die_cu->cu_offset));
		break;
	case DW_FORM_string:
		printf("%s", av->u[0].s);
		break;
	case DW_FORM_strp:
		printf("(indirect string, offset 0x%llx): %s",
		    (unsigned long long) av->u[0].u64, av->u[1].s);
		break;
	default:
		printf("unknown form");
		break;
	}

	/* Dump any extra attribute-specific information. */
	dwarf_dump_av_attr(die, av);

	printf("\n");
}

void
dwarf_dump_die_at_offset(Dwarf_Debug dbg, Dwarf_Off off)
{
	Dwarf_CU cu;
	Dwarf_Die die;

	if (dbg == NULL)
		return;

	STAILQ_FOREACH(cu, &dbg->dbg_cu, cu_next) {
		STAILQ_FOREACH(die, &cu->cu_die, die_next) {
			if ((off_t) die->die_offset == off) {
				dwarf_dump_die(die);
				return;
			}
		}
	}
}

void
dwarf_dump_die(Dwarf_Die die)
{
	Dwarf_AttrValue av;

	printf("<%d><%llx>: Abbrev number: %llu (%s)\n",
	    die->die_level, (unsigned long long) die->die_offset,
	    (unsigned long long) die->die_abnum,
	    get_tag_desc(die->die_a->a_tag));

	STAILQ_FOREACH(av, &die->die_attrval, av_next)
		dwarf_dump_av(die, av);
}

void
dwarf_dump_raw(Dwarf_Debug dbg)
{
	Dwarf_CU cu;
	char *p = (char *) dbg;
	int i;

	printf("dbg %p\n",dbg);

	if (dbg == NULL)
		return;

	for (i = 0; i < (int) sizeof(*dbg); i++) {
		if (*p >= 0x20 && *p < 0x7f) {
			printf(" %c",*p++ & 0xff);
		} else {
			printf(" %02x",*p++ & 0xff);
		}
	}
	printf("\n");

	STAILQ_FOREACH(cu, &dbg->dbg_cu, cu_next) {
		p = (char *) cu;
		printf("cu %p\n",cu);
		for (i = 0; i < (int) sizeof(*cu); i++) {
			if (*p >= 0x20 && *p < 0x7f) {
				printf(" %c",*p++ & 0xff);
			} else {
				printf(" %02x",*p++ & 0xff);
			}
		}
		printf("\n");
	}
}

static void
dwarf_dump_tree_dies(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Error *error)
{
	Dwarf_Die child;
	int ret;

	do {
		dwarf_dump_die(die);

		if ((ret = dwarf_child(die, &child, error) == DWARF_E_NO_ENTRY)) {
			/* No children. */
		} else if (ret != DWARF_E_NONE) {
			printf("Error %s\n", dwarf_errmsg(error));
			return;
		} else
			dwarf_dump_tree_dies(dbg, child, error);

		if (dwarf_siblingof(dbg, die, &die, error) != DWARF_E_NONE)
			die = NULL;
		
	} while (die != NULL);
}

void
dwarf_dump_tree(Dwarf_Debug dbg)
{
	Dwarf_CU cu;
	Dwarf_Die die;
	Dwarf_Error error;
	Dwarf_Half cu_pointer_size;
	Dwarf_Half cu_version;
	Dwarf_Unsigned cu_abbrev_offset;
	Dwarf_Unsigned cu_header_length;
	Dwarf_Unsigned cu_next_offset;

	STAILQ_FOREACH(cu, &dbg->dbg_cu, cu_next) {
		printf ("\nCompilation Unit @ offset %llx:\n",
		    (unsigned long long) cu->cu_offset);
		printf ("    Length:          %lu\n", (u_long) cu->cu_length);
		printf ("    Version:         %hu\n", cu->cu_version);
		printf ("    Abbrev Offset:   %lu\n", (u_long) cu->cu_abbrev_offset);
		printf ("    Pointer Size:    %u\n", (u_int) cu->cu_pointer_size);

		if (dwarf_next_cu_header(dbg, &cu_header_length,
		    &cu_version, &cu_abbrev_offset, &cu_pointer_size,
		    &cu_next_offset, &error) != DWARF_E_NONE) {
			printf("Error %s\n", dwarf_errmsg(&error));
			return;
		}

		if (dwarf_siblingof(dbg, NULL, &die, &error) != DWARF_E_NONE) {
			printf("Error %s\n", dwarf_errmsg(&error));
			return;
		}

		dwarf_dump_tree_dies(dbg, die, &error);

	}
}

void
dwarf_dump_info(Dwarf_Debug dbg)
{
	Dwarf_CU cu;
	Dwarf_Die die;

	printf("Contents of the .debug_info section:\n");

	STAILQ_FOREACH(cu, &dbg->dbg_cu, cu_next) {
		printf ("\nCompilation Unit @ offset %llx:\n",
		    (unsigned long long) cu->cu_offset);
		printf ("    Length:          %lu\n", (u_long) cu->cu_length);
		printf ("    Version:         %hu\n", cu->cu_version);
		printf ("    Abbrev Offset:   %lu\n", (u_long) cu->cu_abbrev_offset);
		printf ("    Pointer Size:    %u\n", (u_int) cu->cu_pointer_size);

		STAILQ_FOREACH(die, &cu->cu_die, die_next)
			dwarf_dump_die(die);
	}
}


void
dwarf_dump_shstrtab(Dwarf_Debug dbg)
{
	char *name;
	int indx = 0;

	printf("---------------------\nSection header string table contents:\n");
	while ((name = elf_strptr(dbg->dbg_elf, dbg->dbg_stnum, indx)) != NULL) {
		printf("%5d '%s'\n",indx,name);
		indx += strlen(name) + 1;
	}
}

void
dwarf_dump_strtab(Dwarf_Debug dbg)
{
	char *name;
	int indx = 0;

	printf("---------------------\nString table contents:\n");
	while ((name = elf_strptr(dbg->dbg_elf, dbg->dbg_s[DWARF_strtab].s_shnum, indx)) != NULL) {
		printf("%5d '%s'\n",indx,name);
		indx += strlen(name) + 1;
	}
}

void
dwarf_dump_dbgstr(Dwarf_Debug dbg)
{
	char *name;
	int indx = 0;

	printf("---------------------\nDebug string table contents:\n");
	while ((name = elf_strptr(dbg->dbg_elf, dbg->dbg_s[DWARF_debug_str].s_shnum, indx)) != NULL) {
		printf("%5d '%s'\n",indx,name);
		indx += strlen(name) + 1;
	}
}

void
dwarf_dump_symtab(Dwarf_Debug dbg)
{
	GElf_Sym sym;
	char *name;
	int indx = 0;

	printf("---------------------\nSymbol table contents:\n");
	while (gelf_getsym(dbg->dbg_s[DWARF_symtab].s_data,  indx++, &sym) != NULL) {
		if ((name = elf_strptr(dbg->dbg_elf, dbg->dbg_s[DWARF_strtab].s_shnum, sym.st_name)) == NULL)
			printf("sym.st_name %u indx %d sym.st_size %lu\n",sym.st_name,indx,(u_long) sym.st_size);
		else
			printf("'%s' sym.st_name %u indx %d sym.st_size %lu\n",name,sym.st_name,indx,(u_long) sym.st_size);
	}
}

void
dwarf_dump(Dwarf_Debug dbg)
{
	dwarf_dump_strtab(dbg);
	dwarf_dump_shstrtab(dbg);
	dwarf_dump_dbgstr(dbg);
	dwarf_dump_symtab(dbg);
	dwarf_dump_info(dbg);
}

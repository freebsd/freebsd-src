#!/bin/sh -u

# Architecture commands for GDB, the GNU debugger.
# Copyright 1998, 1999, 2000, 2001, 2002 Free Software Foundation, Inc.
#
# This file is part of GDB.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

compare_new ()
{
    file=$1
    if test ! -r ${file}
    then
	echo "${file} missing? cp new-${file} ${file}" 1>&2
    elif diff -u ${file} new-${file}
    then
	echo "${file} unchanged" 1>&2
    else
	echo "${file} has changed? cp new-${file} ${file}" 1>&2
    fi
}


# Format of the input table
read="class level macro returntype function formal actual attrib staticdefault predefault postdefault invalid_p fmt print print_p description"

do_read ()
{
    comment=""
    class=""
    while read line
    do
	if test "${line}" = ""
	then
	    continue
	elif test "${line}" = "#" -a "${comment}" = ""
	then
	    continue
	elif expr "${line}" : "#" > /dev/null
	then
	    comment="${comment}
${line}"
	else

	    # The semantics of IFS varies between different SH's.  Some
	    # treat ``::' as three fields while some treat it as just too.
	    # Work around this by eliminating ``::'' ....
	    line="`echo "${line}" | sed -e 's/::/: :/g' -e 's/::/: :/g'`"

	    OFS="${IFS}" ; IFS="[:]"
	    eval read ${read} <<EOF
${line}
EOF
	    IFS="${OFS}"

	    # .... and then going back through each field and strip out those
	    # that ended up with just that space character.
	    for r in ${read}
	    do
		if eval test \"\${${r}}\" = \"\ \"
		then
		    eval ${r}=""
		fi
	    done

	    case "${level}" in
		1 ) gt_level=">= GDB_MULTI_ARCH_PARTIAL" ;;
		2 ) gt_level="> GDB_MULTI_ARCH_PARTIAL" ;;
		"" ) ;;
		* ) error "Error: bad level for ${function}" 1>&2 ; kill $$ ; exit 1 ;;
	    esac

	    case "${class}" in
		m ) staticdefault="${predefault}" ;;
		M ) staticdefault="0" ;;
		* ) test "${staticdefault}" || staticdefault=0 ;;
	    esac
	    # NOT YET: Breaks BELIEVE_PCC_PROMOTION and confuses non-
	    # multi-arch defaults.
	    # test "${predefault}" || predefault=0

	    # come up with a format, use a few guesses for variables
	    case ":${class}:${fmt}:${print}:" in
		:[vV]::: )
		    if [ "${returntype}" = int ]
		    then
			fmt="%d"
			print="${macro}"
		    elif [ "${returntype}" = long ]
		    then
			fmt="%ld"
			print="${macro}"
		    fi
		    ;;
	    esac
	    test "${fmt}" || fmt="%ld"
	    test "${print}" || print="(long) ${macro}"

	    case "${invalid_p}" in
		0 ) valid_p=1 ;;
		"" )
		    if [ -n "${predefault}" ]
		    then
			#invalid_p="gdbarch->${function} == ${predefault}"
			valid_p="gdbarch->${function} != ${predefault}"
		    else
			#invalid_p="gdbarch->${function} == 0"
			valid_p="gdbarch->${function} != 0"
		    fi
		    ;;
		* ) valid_p="!(${invalid_p})"
	    esac

	    # PREDEFAULT is a valid fallback definition of MEMBER when
	    # multi-arch is not enabled.  This ensures that the
	    # default value, when multi-arch is the same as the
	    # default value when not multi-arch.  POSTDEFAULT is
	    # always a valid definition of MEMBER as this again
	    # ensures consistency.

	    if [ -n "${postdefault}" ]
	    then
		fallbackdefault="${postdefault}"
	    elif [ -n "${predefault}" ]
	    then
		fallbackdefault="${predefault}"
	    else
		fallbackdefault="0"
	    fi

	    #NOT YET: See gdbarch.log for basic verification of
	    # database

	    break
	fi
    done
    if [ -n "${class}" ]
    then
	true
    else
	false
    fi
}


fallback_default_p ()
{
    [ -n "${postdefault}" -a "x${invalid_p}" != "x0" ] \
	|| [ -n "${predefault}" -a "x${invalid_p}" = "x0" ]
}

class_is_variable_p ()
{
    case "${class}" in
	*v* | *V* ) true ;;
	* ) false ;;
    esac
}

class_is_function_p ()
{
    case "${class}" in
	*f* | *F* | *m* | *M* ) true ;;
	* ) false ;;
    esac
}

class_is_multiarch_p ()
{
    case "${class}" in
	*m* | *M* ) true ;;
	* ) false ;;
    esac
}

class_is_predicate_p ()
{
    case "${class}" in
	*F* | *V* | *M* ) true ;;
	* ) false ;;
    esac
}

class_is_info_p ()
{
    case "${class}" in
	*i* ) true ;;
	* ) false ;;
    esac
}


# dump out/verify the doco
for field in ${read}
do
  case ${field} in

    class ) : ;;

	# # -> line disable
	# f -> function
	#   hiding a function
	# F -> function + predicate
	#   hiding a function + predicate to test function validity
	# v -> variable
	#   hiding a variable
	# V -> variable + predicate
	#   hiding a variable + predicate to test variables validity
	# i -> set from info
	#   hiding something from the ``struct info'' object
	# m -> multi-arch function
	#   hiding a multi-arch function (parameterised with the architecture)
        # M -> multi-arch function + predicate
	#   hiding a multi-arch function + predicate to test function validity

    level ) : ;;

	# See GDB_MULTI_ARCH description.  Having GDB_MULTI_ARCH >=
	# LEVEL is a predicate on checking that a given method is
	# initialized (using INVALID_P).

    macro ) : ;;

	# The name of the MACRO that this method is to be accessed by.

    returntype ) : ;;

	# For functions, the return type; for variables, the data type

    function ) : ;;

	# For functions, the member function name; for variables, the
	# variable name.  Member function names are always prefixed with
	# ``gdbarch_'' for name-space purity.

    formal ) : ;;

	# The formal argument list.  It is assumed that the formal
	# argument list includes the actual name of each list element.
	# A function with no arguments shall have ``void'' as the
	# formal argument list.

    actual ) : ;;

	# The list of actual arguments.  The arguments specified shall
	# match the FORMAL list given above.  Functions with out
	# arguments leave this blank.

    attrib ) : ;;

	# Any GCC attributes that should be attached to the function
	# declaration.  At present this field is unused.

    staticdefault ) : ;;

	# To help with the GDB startup a static gdbarch object is
	# created.  STATICDEFAULT is the value to insert into that
	# static gdbarch object.  Since this a static object only
	# simple expressions can be used.

	# If STATICDEFAULT is empty, zero is used.

    predefault ) : ;;

	# An initial value to assign to MEMBER of the freshly
	# malloc()ed gdbarch object.  After initialization, the
	# freshly malloc()ed object is passed to the target
	# architecture code for further updates.

	# If PREDEFAULT is empty, zero is used.

	# A non-empty PREDEFAULT, an empty POSTDEFAULT and a zero
	# INVALID_P are specified, PREDEFAULT will be used as the
	# default for the non- multi-arch target.

	# A zero PREDEFAULT function will force the fallback to call
	# internal_error().

	# Variable declarations can refer to ``gdbarch'' which will
	# contain the current architecture.  Care should be taken.

    postdefault ) : ;;

	# A value to assign to MEMBER of the new gdbarch object should
	# the target architecture code fail to change the PREDEFAULT
	# value.

	# If POSTDEFAULT is empty, no post update is performed.

	# If both INVALID_P and POSTDEFAULT are non-empty then
	# INVALID_P will be used to determine if MEMBER should be
	# changed to POSTDEFAULT.

	# If a non-empty POSTDEFAULT and a zero INVALID_P are
	# specified, POSTDEFAULT will be used as the default for the
	# non- multi-arch target (regardless of the value of
	# PREDEFAULT).

	# You cannot specify both a zero INVALID_P and a POSTDEFAULT.

	# Variable declarations can refer to ``gdbarch'' which will
	# contain the current architecture.  Care should be taken.

    invalid_p ) : ;;

	# A predicate equation that validates MEMBER.  Non-zero is
	# returned if the code creating the new architecture failed to
	# initialize MEMBER or the initialized the member is invalid.
	# If POSTDEFAULT is non-empty then MEMBER will be updated to
	# that value.  If POSTDEFAULT is empty then internal_error()
	# is called.

	# If INVALID_P is empty, a check that MEMBER is no longer
	# equal to PREDEFAULT is used.

	# The expression ``0'' disables the INVALID_P check making
	# PREDEFAULT a legitimate value.

	# See also PREDEFAULT and POSTDEFAULT.

    fmt ) : ;;

	# printf style format string that can be used to print out the
	# MEMBER.  Sometimes "%s" is useful.  For functions, this is
	# ignored and the function address is printed.

	# If FMT is empty, ``%ld'' is used.  

    print ) : ;;

	# An optional equation that casts MEMBER to a value suitable
	# for formatting by FMT.

	# If PRINT is empty, ``(long)'' is used.

    print_p ) : ;;

	# An optional indicator for any predicte to wrap around the
	# print member code.

	#   () -> Call a custom function to do the dump.
	#   exp -> Wrap print up in ``if (${print_p}) ...
	#   ``'' -> No predicate

	# If PRINT_P is empty, ``1'' is always used.

    description ) : ;;

	# Currently unused.

    *)
	echo "Bad field ${field}"
	exit 1;;
  esac
done


function_list ()
{
  # See below (DOCO) for description of each field
  cat <<EOF
i:2:TARGET_ARCHITECTURE:const struct bfd_arch_info *:bfd_arch_info::::&bfd_default_arch_struct::::%s:TARGET_ARCHITECTURE->printable_name:TARGET_ARCHITECTURE != NULL
#
i:2:TARGET_BYTE_ORDER:int:byte_order::::BFD_ENDIAN_BIG
# Number of bits in a char or unsigned char for the target machine.
# Just like CHAR_BIT in <limits.h> but describes the target machine.
# v::TARGET_CHAR_BIT:int:char_bit::::8 * sizeof (char):8::0:
#
# Number of bits in a short or unsigned short for the target machine.
v::TARGET_SHORT_BIT:int:short_bit::::8 * sizeof (short):2*TARGET_CHAR_BIT::0
# Number of bits in an int or unsigned int for the target machine.
v::TARGET_INT_BIT:int:int_bit::::8 * sizeof (int):4*TARGET_CHAR_BIT::0
# Number of bits in a long or unsigned long for the target machine.
v::TARGET_LONG_BIT:int:long_bit::::8 * sizeof (long):4*TARGET_CHAR_BIT::0
# Number of bits in a long long or unsigned long long for the target
# machine.
v::TARGET_LONG_LONG_BIT:int:long_long_bit::::8 * sizeof (LONGEST):2*TARGET_LONG_BIT::0
# Number of bits in a float for the target machine.
v::TARGET_FLOAT_BIT:int:float_bit::::8 * sizeof (float):4*TARGET_CHAR_BIT::0
# Number of bits in a double for the target machine.
v::TARGET_DOUBLE_BIT:int:double_bit::::8 * sizeof (double):8*TARGET_CHAR_BIT::0
# Number of bits in a long double for the target machine.
v::TARGET_LONG_DOUBLE_BIT:int:long_double_bit::::8 * sizeof (long double):8*TARGET_CHAR_BIT::0
# For most targets, a pointer on the target and its representation as an
# address in GDB have the same size and "look the same".  For such a
# target, you need only set TARGET_PTR_BIT / ptr_bit and TARGET_ADDR_BIT
# / addr_bit will be set from it.
#
# If TARGET_PTR_BIT and TARGET_ADDR_BIT are different, you'll probably
# also need to set POINTER_TO_ADDRESS and ADDRESS_TO_POINTER as well.
#
# ptr_bit is the size of a pointer on the target
v::TARGET_PTR_BIT:int:ptr_bit::::8 * sizeof (void*):TARGET_INT_BIT::0
# addr_bit is the size of a target address as represented in gdb
v::TARGET_ADDR_BIT:int:addr_bit::::8 * sizeof (void*):0:TARGET_PTR_BIT:
# Number of bits in a BFD_VMA for the target object file format.
v::TARGET_BFD_VMA_BIT:int:bfd_vma_bit::::8 * sizeof (void*):TARGET_ARCHITECTURE->bits_per_address::0
#
# One if \`char' acts like \`signed char', zero if \`unsigned char'.
v::TARGET_CHAR_SIGNED:int:char_signed::::1:-1:1::::
#
f::TARGET_READ_PC:CORE_ADDR:read_pc:ptid_t ptid:ptid::0:generic_target_read_pc::0
f::TARGET_WRITE_PC:void:write_pc:CORE_ADDR val, ptid_t ptid:val, ptid::0:generic_target_write_pc::0
f::TARGET_READ_FP:CORE_ADDR:read_fp:void:::0:generic_target_read_fp::0
f::TARGET_WRITE_FP:void:write_fp:CORE_ADDR val:val::0:generic_target_write_fp::0
f::TARGET_READ_SP:CORE_ADDR:read_sp:void:::0:generic_target_read_sp::0
f::TARGET_WRITE_SP:void:write_sp:CORE_ADDR val:val::0:generic_target_write_sp::0
# Function for getting target's idea of a frame pointer.  FIXME: GDB's
# whole scheme for dealing with "frames" and "frame pointers" needs a
# serious shakedown.
f::TARGET_VIRTUAL_FRAME_POINTER:void:virtual_frame_pointer:CORE_ADDR pc, int *frame_regnum, LONGEST *frame_offset:pc, frame_regnum, frame_offset::0:legacy_virtual_frame_pointer::0
#
M:::void:register_read:int regnum, char *buf:regnum, buf:
M:::void:register_write:int regnum, char *buf:regnum, buf:
#
v:2:NUM_REGS:int:num_regs::::0:-1
# This macro gives the number of pseudo-registers that live in the
# register namespace but do not get fetched or stored on the target.
# These pseudo-registers may be aliases for other registers,
# combinations of other registers, or they may be computed by GDB.
v:2:NUM_PSEUDO_REGS:int:num_pseudo_regs::::0:0::0:::
v:2:SP_REGNUM:int:sp_regnum::::0:-1
v:2:FP_REGNUM:int:fp_regnum::::0:-1
v:2:PC_REGNUM:int:pc_regnum::::0:-1
v:2:FP0_REGNUM:int:fp0_regnum::::0:-1::0
v:2:NPC_REGNUM:int:npc_regnum::::0:-1::0
v:2:NNPC_REGNUM:int:nnpc_regnum::::0:-1::0
# Convert stab register number (from \`r\' declaration) to a gdb REGNUM.
f:2:STAB_REG_TO_REGNUM:int:stab_reg_to_regnum:int stab_regnr:stab_regnr:::no_op_reg_to_regnum::0
# Provide a default mapping from a ecoff register number to a gdb REGNUM.
f:2:ECOFF_REG_TO_REGNUM:int:ecoff_reg_to_regnum:int ecoff_regnr:ecoff_regnr:::no_op_reg_to_regnum::0
# Provide a default mapping from a DWARF register number to a gdb REGNUM.
f:2:DWARF_REG_TO_REGNUM:int:dwarf_reg_to_regnum:int dwarf_regnr:dwarf_regnr:::no_op_reg_to_regnum::0
# Convert from an sdb register number to an internal gdb register number.
# This should be defined in tm.h, if REGISTER_NAMES is not set up
# to map one to one onto the sdb register numbers.
f:2:SDB_REG_TO_REGNUM:int:sdb_reg_to_regnum:int sdb_regnr:sdb_regnr:::no_op_reg_to_regnum::0
f:2:DWARF2_REG_TO_REGNUM:int:dwarf2_reg_to_regnum:int dwarf2_regnr:dwarf2_regnr:::no_op_reg_to_regnum::0
f:2:REGISTER_NAME:char *:register_name:int regnr:regnr:::legacy_register_name::0
v:2:REGISTER_SIZE:int:register_size::::0:-1
v:2:REGISTER_BYTES:int:register_bytes::::0:-1
f:2:REGISTER_BYTE:int:register_byte:int reg_nr:reg_nr::0:0
f:2:REGISTER_RAW_SIZE:int:register_raw_size:int reg_nr:reg_nr::generic_register_raw_size:0
v:2:MAX_REGISTER_RAW_SIZE:int:max_register_raw_size::::0:-1
f:2:REGISTER_VIRTUAL_SIZE:int:register_virtual_size:int reg_nr:reg_nr::generic_register_virtual_size:0
v:2:MAX_REGISTER_VIRTUAL_SIZE:int:max_register_virtual_size::::0:-1
f:2:REGISTER_VIRTUAL_TYPE:struct type *:register_virtual_type:int reg_nr:reg_nr::0:0
f:2:DO_REGISTERS_INFO:void:do_registers_info:int reg_nr, int fpregs:reg_nr, fpregs:::do_registers_info::0
f:2:PRINT_FLOAT_INFO:void:print_float_info:void::::default_print_float_info::0
# MAP a GDB RAW register number onto a simulator register number.  See
# also include/...-sim.h.
f:2:REGISTER_SIM_REGNO:int:register_sim_regno:int reg_nr:reg_nr:::default_register_sim_regno::0
F:2:REGISTER_BYTES_OK:int:register_bytes_ok:long nr_bytes:nr_bytes::0:0
f:2:CANNOT_FETCH_REGISTER:int:cannot_fetch_register:int regnum:regnum:::cannot_register_not::0
f:2:CANNOT_STORE_REGISTER:int:cannot_store_register:int regnum:regnum:::cannot_register_not::0
# setjmp/longjmp support.
F:2:GET_LONGJMP_TARGET:int:get_longjmp_target:CORE_ADDR *pc:pc::0:0
#
# Non multi-arch DUMMY_FRAMES are a mess (multi-arch ones are not that
# much better but at least they are vaguely consistent).  The headers
# and body contain convoluted #if/#else sequences for determine how
# things should be compiled.  Instead of trying to mimic that
# behaviour here (and hence entrench it further) gdbarch simply
# reqires that these methods be set up from the word go.  This also
# avoids any potential problems with moving beyond multi-arch partial.
v:1:USE_GENERIC_DUMMY_FRAMES:int:use_generic_dummy_frames::::0:-1
v:1:CALL_DUMMY_LOCATION:int:call_dummy_location::::0:0
f:2:CALL_DUMMY_ADDRESS:CORE_ADDR:call_dummy_address:void:::0:0::gdbarch->call_dummy_location == AT_ENTRY_POINT && gdbarch->call_dummy_address == 0
v:2:CALL_DUMMY_START_OFFSET:CORE_ADDR:call_dummy_start_offset::::0:-1:::0x%08lx
v:2:CALL_DUMMY_BREAKPOINT_OFFSET:CORE_ADDR:call_dummy_breakpoint_offset::::0:-1::gdbarch->call_dummy_breakpoint_offset_p && gdbarch->call_dummy_breakpoint_offset == -1:0x%08lx::CALL_DUMMY_BREAKPOINT_OFFSET_P
v:1:CALL_DUMMY_BREAKPOINT_OFFSET_P:int:call_dummy_breakpoint_offset_p::::0:-1
v:2:CALL_DUMMY_LENGTH:int:call_dummy_length::::0:-1:::::CALL_DUMMY_LOCATION == BEFORE_TEXT_END || CALL_DUMMY_LOCATION == AFTER_TEXT_END
f:1:PC_IN_CALL_DUMMY:int:pc_in_call_dummy:CORE_ADDR pc, CORE_ADDR sp, CORE_ADDR frame_address:pc, sp, frame_address::0:0
v:1:CALL_DUMMY_P:int:call_dummy_p::::0:-1
v:2:CALL_DUMMY_WORDS:LONGEST *:call_dummy_words::::0:legacy_call_dummy_words::0:0x%08lx
v:2:SIZEOF_CALL_DUMMY_WORDS:int:sizeof_call_dummy_words::::0:legacy_sizeof_call_dummy_words::0:0x%08lx
v:1:CALL_DUMMY_STACK_ADJUST_P:int:call_dummy_stack_adjust_p::::0:-1:::0x%08lx
v:2:CALL_DUMMY_STACK_ADJUST:int:call_dummy_stack_adjust::::0:::gdbarch->call_dummy_stack_adjust_p && gdbarch->call_dummy_stack_adjust == 0:0x%08lx::CALL_DUMMY_STACK_ADJUST_P
f:2:FIX_CALL_DUMMY:void:fix_call_dummy:char *dummy, CORE_ADDR pc, CORE_ADDR fun, int nargs, struct value **args, struct type *type, int gcc_p:dummy, pc, fun, nargs, args, type, gcc_p:::0
f:2:INIT_FRAME_PC_FIRST:void:init_frame_pc_first:int fromleaf, struct frame_info *prev:fromleaf, prev:::init_frame_pc_noop::0
f:2:INIT_FRAME_PC:void:init_frame_pc:int fromleaf, struct frame_info *prev:fromleaf, prev:::init_frame_pc_default::0
#
v:2:BELIEVE_PCC_PROMOTION:int:believe_pcc_promotion:::::::
v:2:BELIEVE_PCC_PROMOTION_TYPE:int:believe_pcc_promotion_type:::::::
f:2:COERCE_FLOAT_TO_DOUBLE:int:coerce_float_to_double:struct type *formal, struct type *actual:formal, actual:::default_coerce_float_to_double::0
# GET_SAVED_REGISTER is like DUMMY_FRAMES.  It is at level one as the
# old code has strange #ifdef interaction.  So far no one has found
# that default_get_saved_register() is the default they are after.
f:1:GET_SAVED_REGISTER:void:get_saved_register:char *raw_buffer, int *optimized, CORE_ADDR *addrp, struct frame_info *frame, int regnum, enum lval_type *lval:raw_buffer, optimized, addrp, frame, regnum, lval::generic_get_saved_register:0
#
f:2:REGISTER_CONVERTIBLE:int:register_convertible:int nr:nr:::generic_register_convertible_not::0
f:2:REGISTER_CONVERT_TO_VIRTUAL:void:register_convert_to_virtual:int regnum, struct type *type, char *from, char *to:regnum, type, from, to:::0::0
f:2:REGISTER_CONVERT_TO_RAW:void:register_convert_to_raw:struct type *type, int regnum, char *from, char *to:type, regnum, from, to:::0::0
# This function is called when the value of a pseudo-register needs to
# be updated.  Typically it will be defined on a per-architecture
# basis.
F:2:FETCH_PSEUDO_REGISTER:void:fetch_pseudo_register:int regnum:regnum:
# This function is called when the value of a pseudo-register needs to
# be set or stored.  Typically it will be defined on a
# per-architecture basis.
F:2:STORE_PSEUDO_REGISTER:void:store_pseudo_register:int regnum:regnum:
#
f:2:POINTER_TO_ADDRESS:CORE_ADDR:pointer_to_address:struct type *type, void *buf:type, buf:::unsigned_pointer_to_address::0
f:2:ADDRESS_TO_POINTER:void:address_to_pointer:struct type *type, void *buf, CORE_ADDR addr:type, buf, addr:::unsigned_address_to_pointer::0
F:2:INTEGER_TO_ADDRESS:CORE_ADDR:integer_to_address:struct type *type, void *buf:type, buf
#
f:2:RETURN_VALUE_ON_STACK:int:return_value_on_stack:struct type *type:type:::generic_return_value_on_stack_not::0
f:2:EXTRACT_RETURN_VALUE:void:extract_return_value:struct type *type, char *regbuf, char *valbuf:type, regbuf, valbuf::0:0
f:2:PUSH_ARGUMENTS:CORE_ADDR:push_arguments:int nargs, struct value **args, CORE_ADDR sp, int struct_return, CORE_ADDR struct_addr:nargs, args, sp, struct_return, struct_addr:::default_push_arguments::0
f:2:PUSH_DUMMY_FRAME:void:push_dummy_frame:void:-:::0
F:2:PUSH_RETURN_ADDRESS:CORE_ADDR:push_return_address:CORE_ADDR pc, CORE_ADDR sp:pc, sp:::0
f:2:POP_FRAME:void:pop_frame:void:-:::0
#
f:2:STORE_STRUCT_RETURN:void:store_struct_return:CORE_ADDR addr, CORE_ADDR sp:addr, sp:::0
f:2:STORE_RETURN_VALUE:void:store_return_value:struct type *type, char *valbuf:type, valbuf:::0
F:2:EXTRACT_STRUCT_VALUE_ADDRESS:CORE_ADDR:extract_struct_value_address:char *regbuf:regbuf:::0
f:2:USE_STRUCT_CONVENTION:int:use_struct_convention:int gcc_p, struct type *value_type:gcc_p, value_type:::generic_use_struct_convention::0
#
f:2:FRAME_INIT_SAVED_REGS:void:frame_init_saved_regs:struct frame_info *frame:frame::0:0
F:2:INIT_EXTRA_FRAME_INFO:void:init_extra_frame_info:int fromleaf, struct frame_info *frame:fromleaf, frame:::0
#
f:2:SKIP_PROLOGUE:CORE_ADDR:skip_prologue:CORE_ADDR ip:ip::0:0
f:2:PROLOGUE_FRAMELESS_P:int:prologue_frameless_p:CORE_ADDR ip:ip::0:generic_prologue_frameless_p::0
f:2:INNER_THAN:int:inner_than:CORE_ADDR lhs, CORE_ADDR rhs:lhs, rhs::0:0
f:2:BREAKPOINT_FROM_PC:unsigned char *:breakpoint_from_pc:CORE_ADDR *pcptr, int *lenptr:pcptr, lenptr:::legacy_breakpoint_from_pc::0
f:2:MEMORY_INSERT_BREAKPOINT:int:memory_insert_breakpoint:CORE_ADDR addr, char *contents_cache:addr, contents_cache::0:default_memory_insert_breakpoint::0
f:2:MEMORY_REMOVE_BREAKPOINT:int:memory_remove_breakpoint:CORE_ADDR addr, char *contents_cache:addr, contents_cache::0:default_memory_remove_breakpoint::0
v:2:DECR_PC_AFTER_BREAK:CORE_ADDR:decr_pc_after_break::::0:-1
f::PREPARE_TO_PROCEED:int:prepare_to_proceed:int select_it:select_it::0:default_prepare_to_proceed::0
v:2:FUNCTION_START_OFFSET:CORE_ADDR:function_start_offset::::0:-1
#
f:2:REMOTE_TRANSLATE_XFER_ADDRESS:void:remote_translate_xfer_address:CORE_ADDR gdb_addr, int gdb_len, CORE_ADDR *rem_addr, int *rem_len:gdb_addr, gdb_len, rem_addr, rem_len:::generic_remote_translate_xfer_address::0
#
v:2:FRAME_ARGS_SKIP:CORE_ADDR:frame_args_skip::::0:-1
f:2:FRAMELESS_FUNCTION_INVOCATION:int:frameless_function_invocation:struct frame_info *fi:fi:::generic_frameless_function_invocation_not::0
f:2:FRAME_CHAIN:CORE_ADDR:frame_chain:struct frame_info *frame:frame::0:0
# Define a default FRAME_CHAIN_VALID, in the form that is suitable for
# most targets.  If FRAME_CHAIN_VALID returns zero it means that the
# given frame is the outermost one and has no caller.
#
# XXXX - both default and alternate frame_chain_valid functions are
# deprecated.  New code should use dummy frames and one of the generic
# functions.
f:2:FRAME_CHAIN_VALID:int:frame_chain_valid:CORE_ADDR chain, struct frame_info *thisframe:chain, thisframe:::func_frame_chain_valid::0
f:2:FRAME_SAVED_PC:CORE_ADDR:frame_saved_pc:struct frame_info *fi:fi::0:0
f:2:FRAME_ARGS_ADDRESS:CORE_ADDR:frame_args_address:struct frame_info *fi:fi::0:0
f:2:FRAME_LOCALS_ADDRESS:CORE_ADDR:frame_locals_address:struct frame_info *fi:fi::0:0
f:2:SAVED_PC_AFTER_CALL:CORE_ADDR:saved_pc_after_call:struct frame_info *frame:frame::0:0
f:2:FRAME_NUM_ARGS:int:frame_num_args:struct frame_info *frame:frame::0:0
#
F:2:STACK_ALIGN:CORE_ADDR:stack_align:CORE_ADDR sp:sp::0:0
v:2:EXTRA_STACK_ALIGNMENT_NEEDED:int:extra_stack_alignment_needed::::0:1::0:::
F:2:REG_STRUCT_HAS_ADDR:int:reg_struct_has_addr:int gcc_p, struct type *type:gcc_p, type::0:0
F:2:SAVE_DUMMY_FRAME_TOS:void:save_dummy_frame_tos:CORE_ADDR sp:sp::0:0
v:2:PARM_BOUNDARY:int:parm_boundary
#
v:2:TARGET_FLOAT_FORMAT:const struct floatformat *:float_format::::::default_float_format (gdbarch)
v:2:TARGET_DOUBLE_FORMAT:const struct floatformat *:double_format::::::default_double_format (gdbarch)
v:2:TARGET_LONG_DOUBLE_FORMAT:const struct floatformat *:long_double_format::::::default_double_format (gdbarch)
f:2:CONVERT_FROM_FUNC_PTR_ADDR:CORE_ADDR:convert_from_func_ptr_addr:CORE_ADDR addr:addr:::core_addr_identity::0
# On some machines there are bits in addresses which are not really
# part of the address, but are used by the kernel, the hardware, etc.
# for special purposes.  ADDR_BITS_REMOVE takes out any such bits so
# we get a "real" address such as one would find in a symbol table.
# This is used only for addresses of instructions, and even then I'm
# not sure it's used in all contexts.  It exists to deal with there
# being a few stray bits in the PC which would mislead us, not as some
# sort of generic thing to handle alignment or segmentation (it's
# possible it should be in TARGET_READ_PC instead).
f:2:ADDR_BITS_REMOVE:CORE_ADDR:addr_bits_remove:CORE_ADDR addr:addr:::core_addr_identity::0
# It is not at all clear why SMASH_TEXT_ADDRESS is not folded into 
# ADDR_BITS_REMOVE.
f:2:SMASH_TEXT_ADDRESS:CORE_ADDR:smash_text_address:CORE_ADDR addr:addr:::core_addr_identity::0
# FIXME/cagney/2001-01-18: This should be split in two.  A target method that indicates if
# the target needs software single step.  An ISA method to implement it.
#
# FIXME/cagney/2001-01-18: This should be replaced with something that inserts breakpoints
# using the breakpoint system instead of blatting memory directly (as with rs6000).
#
# FIXME/cagney/2001-01-18: The logic is backwards.  It should be asking if the target can
# single step.  If not, then implement single step using breakpoints.
F:2:SOFTWARE_SINGLE_STEP:void:software_single_step:enum target_signal sig, int insert_breakpoints_p:sig, insert_breakpoints_p::0:0
f:2:TARGET_PRINT_INSN:int:print_insn:bfd_vma vma, disassemble_info *info:vma, info:::legacy_print_insn::0
f:2:SKIP_TRAMPOLINE_CODE:CORE_ADDR:skip_trampoline_code:CORE_ADDR pc:pc:::generic_skip_trampoline_code::0
# For SVR4 shared libraries, each call goes through a small piece of
# trampoline code in the ".plt" section.  IN_SOLIB_CALL_TRAMPOLINE evaluates
# to nonzero if we are current stopped in one of these.
f:2:IN_SOLIB_CALL_TRAMPOLINE:int:in_solib_call_trampoline:CORE_ADDR pc, char *name:pc, name:::generic_in_solib_call_trampoline::0
# A target might have problems with watchpoints as soon as the stack
# frame of the current function has been destroyed.  This mostly happens
# as the first action in a funtion's epilogue.  in_function_epilogue_p()
# is defined to return a non-zero value if either the given addr is one
# instruction after the stack destroying instruction up to the trailing
# return instruction or if we can figure out that the stack frame has
# already been invalidated regardless of the value of addr.  Targets
# which don't suffer from that problem could just let this functionality
# untouched.
m:::int:in_function_epilogue_p:CORE_ADDR addr:addr::0:generic_in_function_epilogue_p::0
# Given a vector of command-line arguments, return a newly allocated
# string which, when passed to the create_inferior function, will be
# parsed (on Unix systems, by the shell) to yield the same vector.
# This function should call error() if the argument vector is not
# representable for this target or if this target does not support
# command-line arguments.
# ARGC is the number of elements in the vector.
# ARGV is an array of strings, one per argument.
m::CONSTRUCT_INFERIOR_ARGUMENTS:char *:construct_inferior_arguments:int argc, char **argv:argc, argv:::construct_inferior_arguments::0
F:2:DWARF2_BUILD_FRAME_INFO:void:dwarf2_build_frame_info:struct objfile *objfile:objfile:::0
f:2:ELF_MAKE_MSYMBOL_SPECIAL:void:elf_make_msymbol_special:asymbol *sym, struct minimal_symbol *msym:sym, msym:::default_elf_make_msymbol_special::0
f:2:COFF_MAKE_MSYMBOL_SPECIAL:void:coff_make_msymbol_special:int val, struct minimal_symbol *msym:val, msym:::default_coff_make_msymbol_special::0
EOF
}

#
# The .log file
#
exec > new-gdbarch.log
function_list | while do_read
do
    cat <<EOF
${class} ${macro}(${actual})
  ${returntype} ${function} ($formal)${attrib}
EOF
    for r in ${read}
    do
	eval echo \"\ \ \ \ ${r}=\${${r}}\"
    done
#    #fallbackdefault=${fallbackdefault}
#    #valid_p=${valid_p}
#EOF
    if class_is_predicate_p && fallback_default_p
    then
	echo "Error: predicate function ${macro} can not have a non- multi-arch default" 1>&2
	kill $$
	exit 1
    fi
    if [ "x${invalid_p}" = "x0" -a -n "${postdefault}" ]
    then
	echo "Error: postdefault is useless when invalid_p=0" 1>&2
	kill $$
	exit 1
    fi
    if class_is_multiarch_p
    then
	if class_is_predicate_p ; then :
	elif test "x${predefault}" = "x"
	then
	    echo "Error: pure multi-arch function must have a predefault" 1>&2
	    kill $$
	    exit 1
	fi
    fi
    echo ""
done

exec 1>&2
compare_new gdbarch.log


copyright ()
{
cat <<EOF
/* *INDENT-OFF* */ /* THIS FILE IS GENERATED */

/* Dynamic architecture support for GDB, the GNU debugger.
   Copyright 1998, 1999, 2000, 2001, 2002 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* This file was created with the aid of \`\`gdbarch.sh''.

   The Bourne shell script \`\`gdbarch.sh'' creates the files
   \`\`new-gdbarch.c'' and \`\`new-gdbarch.h and then compares them
   against the existing \`\`gdbarch.[hc]''.  Any differences found
   being reported.

   If editing this file, please also run gdbarch.sh and merge any
   changes into that script. Conversely, when making sweeping changes
   to this file, modifying gdbarch.sh and using its output may prove
   easier. */

EOF
}

#
# The .h file
#

exec > new-gdbarch.h
copyright
cat <<EOF
#ifndef GDBARCH_H
#define GDBARCH_H

#include "dis-asm.h" /* Get defs for disassemble_info, which unfortunately is a typedef. */
#if !GDB_MULTI_ARCH
#include "value.h" /* For default_coerce_float_to_double which is referenced by a macro.  */
#endif

struct frame_info;
struct value;
struct objfile;
struct minimal_symbol;

extern struct gdbarch *current_gdbarch;


/* If any of the following are defined, the target wasn't correctly
   converted. */

#if GDB_MULTI_ARCH
#if defined (EXTRA_FRAME_INFO)
#error "EXTRA_FRAME_INFO: replaced by struct frame_extra_info"
#endif
#endif

#if GDB_MULTI_ARCH
#if defined (FRAME_FIND_SAVED_REGS)
#error "FRAME_FIND_SAVED_REGS: replaced by FRAME_INIT_SAVED_REGS"
#endif
#endif

#if (GDB_MULTI_ARCH >= GDB_MULTI_ARCH_PURE) && defined (GDB_TM_FILE)
#error "GDB_TM_FILE: Pure multi-arch targets do not have a tm.h file."
#endif
EOF

# function typedef's
printf "\n"
printf "\n"
printf "/* The following are pre-initialized by GDBARCH. */\n"
function_list | while do_read
do
    if class_is_info_p
    then
	printf "\n"
	printf "extern ${returntype} gdbarch_${function} (struct gdbarch *gdbarch);\n"
	printf "/* set_gdbarch_${function}() - not applicable - pre-initialized. */\n"
	printf "#if (GDB_MULTI_ARCH ${gt_level}) && defined (${macro})\n"
	printf "#error \"Non multi-arch definition of ${macro}\"\n"
	printf "#endif\n"
	printf "#if GDB_MULTI_ARCH\n"
	printf "#if (GDB_MULTI_ARCH ${gt_level}) || !defined (${macro})\n"
	printf "#define ${macro} (gdbarch_${function} (current_gdbarch))\n"
	printf "#endif\n"
	printf "#endif\n"
    fi
done

# function typedef's
printf "\n"
printf "\n"
printf "/* The following are initialized by the target dependent code. */\n"
function_list | while do_read
do
    if [ -n "${comment}" ]
    then
	echo "${comment}" | sed \
	    -e '2 s,#,/*,' \
	    -e '3,$ s,#,  ,' \
	    -e '$ s,$, */,'
    fi
    if class_is_multiarch_p
    then
	if class_is_predicate_p
	then
	    printf "\n"
	    printf "extern int gdbarch_${function}_p (struct gdbarch *gdbarch);\n"
	fi
    else
	if class_is_predicate_p
	then
	    printf "\n"
	    printf "#if defined (${macro})\n"
	    printf "/* Legacy for systems yet to multi-arch ${macro} */\n"
	    #printf "#if (GDB_MULTI_ARCH <= GDB_MULTI_ARCH_PARTIAL) && defined (${macro})\n"
	    printf "#if !defined (${macro}_P)\n"
	    printf "#define ${macro}_P() (1)\n"
	    printf "#endif\n"
	    printf "#endif\n"
	    printf "\n"
	    printf "/* Default predicate for non- multi-arch targets. */\n"
	    printf "#if (!GDB_MULTI_ARCH) && !defined (${macro}_P)\n"
	    printf "#define ${macro}_P() (0)\n"
	    printf "#endif\n"
	    printf "\n"
	    printf "extern int gdbarch_${function}_p (struct gdbarch *gdbarch);\n"
	    printf "#if (GDB_MULTI_ARCH ${gt_level}) && defined (${macro}_P)\n"
	    printf "#error \"Non multi-arch definition of ${macro}\"\n"
	    printf "#endif\n"
	    printf "#if (GDB_MULTI_ARCH ${gt_level}) || !defined (${macro}_P)\n"
	    printf "#define ${macro}_P() (gdbarch_${function}_p (current_gdbarch))\n"
	    printf "#endif\n"
	fi
    fi
    if class_is_variable_p
    then
	if fallback_default_p || class_is_predicate_p
	then
	    printf "\n"
	    printf "/* Default (value) for non- multi-arch platforms. */\n"
	    printf "#if (!GDB_MULTI_ARCH) && !defined (${macro})\n"
	    echo "#define ${macro} (${fallbackdefault})" \
		| sed -e 's/\([^a-z_]\)\(gdbarch[^a-z_]\)/\1current_\2/g'
	    printf "#endif\n"
	fi
	printf "\n"
	printf "extern ${returntype} gdbarch_${function} (struct gdbarch *gdbarch);\n"
	printf "extern void set_gdbarch_${function} (struct gdbarch *gdbarch, ${returntype} ${function});\n"
	printf "#if (GDB_MULTI_ARCH ${gt_level}) && defined (${macro})\n"
	printf "#error \"Non multi-arch definition of ${macro}\"\n"
	printf "#endif\n"
	printf "#if GDB_MULTI_ARCH\n"
	printf "#if (GDB_MULTI_ARCH ${gt_level}) || !defined (${macro})\n"
	printf "#define ${macro} (gdbarch_${function} (current_gdbarch))\n"
	printf "#endif\n"
	printf "#endif\n"
    fi
    if class_is_function_p
    then
	if class_is_multiarch_p ; then :
	elif fallback_default_p || class_is_predicate_p
	then
	    printf "\n"
	    printf "/* Default (function) for non- multi-arch platforms. */\n"
	    printf "#if (!GDB_MULTI_ARCH) && !defined (${macro})\n"
	    if [ "x${fallbackdefault}" = "x0" ]
	    then
		printf "#define ${macro}(${actual}) (internal_error (__FILE__, __LINE__, \"${macro}\"), 0)\n"
	    else
		# FIXME: Should be passing current_gdbarch through!
		echo "#define ${macro}(${actual}) (${fallbackdefault} (${actual}))" \
		    | sed -e 's/\([^a-z_]\)\(gdbarch[^a-z_]\)/\1current_\2/g'
	    fi
	    printf "#endif\n"
	fi
	printf "\n"
	if [ "x${formal}" = "xvoid" ] && class_is_multiarch_p
	then
	    printf "typedef ${returntype} (gdbarch_${function}_ftype) (struct gdbarch *gdbarch);\n"
	elif class_is_multiarch_p
	then
	    printf "typedef ${returntype} (gdbarch_${function}_ftype) (struct gdbarch *gdbarch, ${formal});\n"
	else
	    printf "typedef ${returntype} (gdbarch_${function}_ftype) (${formal});\n"
	fi
	if [ "x${formal}" = "xvoid" ]
	then
	  printf "extern ${returntype} gdbarch_${function} (struct gdbarch *gdbarch);\n"
	else
	  printf "extern ${returntype} gdbarch_${function} (struct gdbarch *gdbarch, ${formal});\n"
	fi
	printf "extern void set_gdbarch_${function} (struct gdbarch *gdbarch, gdbarch_${function}_ftype *${function});\n"
	if class_is_multiarch_p ; then :
	else
	    printf "#if (GDB_MULTI_ARCH ${gt_level}) && defined (${macro})\n"
	    printf "#error \"Non multi-arch definition of ${macro}\"\n"
	    printf "#endif\n"
	    printf "#if GDB_MULTI_ARCH\n"
	    printf "#if (GDB_MULTI_ARCH ${gt_level}) || !defined (${macro})\n"
	    if [ "x${actual}" = "x" ]
	    then
		printf "#define ${macro}() (gdbarch_${function} (current_gdbarch))\n"
	    elif [ "x${actual}" = "x-" ]
	    then
		printf "#define ${macro} (gdbarch_${function} (current_gdbarch))\n"
	    else
		printf "#define ${macro}(${actual}) (gdbarch_${function} (current_gdbarch, ${actual}))\n"
	    fi
	    printf "#endif\n"
	    printf "#endif\n"
	fi
    fi
done

# close it off
cat <<EOF

extern struct gdbarch_tdep *gdbarch_tdep (struct gdbarch *gdbarch);


/* Mechanism for co-ordinating the selection of a specific
   architecture.

   GDB targets (*-tdep.c) can register an interest in a specific
   architecture.  Other GDB components can register a need to maintain
   per-architecture data.

   The mechanisms below ensures that there is only a loose connection
   between the set-architecture command and the various GDB
   components.  Each component can independently register their need
   to maintain architecture specific data with gdbarch.

   Pragmatics:

   Previously, a single TARGET_ARCHITECTURE_HOOK was provided.  It
   didn't scale.

   The more traditional mega-struct containing architecture specific
   data for all the various GDB components was also considered.  Since
   GDB is built from a variable number of (fairly independent)
   components it was determined that the global aproach was not
   applicable. */


/* Register a new architectural family with GDB.

   Register support for the specified ARCHITECTURE with GDB.  When
   gdbarch determines that the specified architecture has been
   selected, the corresponding INIT function is called.

   --

   The INIT function takes two parameters: INFO which contains the
   information available to gdbarch about the (possibly new)
   architecture; ARCHES which is a list of the previously created
   \`\`struct gdbarch'' for this architecture.

   The INIT function parameter INFO shall, as far as possible, be
   pre-initialized with information obtained from INFO.ABFD or
   previously selected architecture (if similar).

   The INIT function shall return any of: NULL - indicating that it
   doesn't recognize the selected architecture; an existing \`\`struct
   gdbarch'' from the ARCHES list - indicating that the new
   architecture is just a synonym for an earlier architecture (see
   gdbarch_list_lookup_by_info()); a newly created \`\`struct gdbarch''
   - that describes the selected architecture (see gdbarch_alloc()).

   The DUMP_TDEP function shall print out all target specific values.
   Care should be taken to ensure that the function works in both the
   multi-arch and non- multi-arch cases. */

struct gdbarch_list
{
  struct gdbarch *gdbarch;
  struct gdbarch_list *next;
};

struct gdbarch_info
{
  /* Use default: NULL (ZERO). */
  const struct bfd_arch_info *bfd_arch_info;

  /* Use default: BFD_ENDIAN_UNKNOWN (NB: is not ZERO).  */
  int byte_order;

  /* Use default: NULL (ZERO). */
  bfd *abfd;

  /* Use default: NULL (ZERO). */
  struct gdbarch_tdep_info *tdep_info;
};

typedef struct gdbarch *(gdbarch_init_ftype) (struct gdbarch_info info, struct gdbarch_list *arches);
typedef void (gdbarch_dump_tdep_ftype) (struct gdbarch *gdbarch, struct ui_file *file);

/* DEPRECATED - use gdbarch_register() */
extern void register_gdbarch_init (enum bfd_architecture architecture, gdbarch_init_ftype *);

extern void gdbarch_register (enum bfd_architecture architecture,
                              gdbarch_init_ftype *,
                              gdbarch_dump_tdep_ftype *);


/* Return a freshly allocated, NULL terminated, array of the valid
   architecture names.  Since architectures are registered during the
   _initialize phase this function only returns useful information
   once initialization has been completed. */

extern const char **gdbarch_printable_names (void);


/* Helper function.  Search the list of ARCHES for a GDBARCH that
   matches the information provided by INFO. */

extern struct gdbarch_list *gdbarch_list_lookup_by_info (struct gdbarch_list *arches,  const struct gdbarch_info *info);


/* Helper function.  Create a preliminary \`\`struct gdbarch''.  Perform
   basic initialization using values obtained from the INFO andTDEP
   parameters.  set_gdbarch_*() functions are called to complete the
   initialization of the object. */

extern struct gdbarch *gdbarch_alloc (const struct gdbarch_info *info, struct gdbarch_tdep *tdep);


/* Helper function.  Free a partially-constructed \`\`struct gdbarch''.
   It is assumed that the caller freeds the \`\`struct
   gdbarch_tdep''. */

extern void gdbarch_free (struct gdbarch *);


/* Helper function. Force an update of the current architecture.

   The actual architecture selected is determined by INFO, \`\`(gdb) set
   architecture'' et.al., the existing architecture and BFD's default
   architecture.  INFO should be initialized to zero and then selected
   fields should be updated.

   Returns non-zero if the update succeeds */

extern int gdbarch_update_p (struct gdbarch_info info);



/* Register per-architecture data-pointer.

   Reserve space for a per-architecture data-pointer.  An identifier
   for the reserved data-pointer is returned.  That identifer should
   be saved in a local static variable.

   The per-architecture data-pointer can be initialized in one of two
   ways: The value can be set explicitly using a call to
   set_gdbarch_data(); the value can be set implicitly using the value
   returned by a non-NULL INIT() callback.  INIT(), when non-NULL is
   called after the basic architecture vector has been created.

   When a previously created architecture is re-selected, the
   per-architecture data-pointer for that previous architecture is
   restored.  INIT() is not called.

   During initialization, multiple assignments of the data-pointer are
   allowed, non-NULL values are deleted by calling FREE().  If the
   architecture is deleted using gdbarch_free() all non-NULL data
   pointers are also deleted using FREE().

   Multiple registrarants for any architecture are allowed (and
   strongly encouraged).  */

struct gdbarch_data;

typedef void *(gdbarch_data_init_ftype) (struct gdbarch *gdbarch);
typedef void (gdbarch_data_free_ftype) (struct gdbarch *gdbarch,
					void *pointer);
extern struct gdbarch_data *register_gdbarch_data (gdbarch_data_init_ftype *init,
						   gdbarch_data_free_ftype *free);
extern void set_gdbarch_data (struct gdbarch *gdbarch,
			      struct gdbarch_data *data,
			      void *pointer);

extern void *gdbarch_data (struct gdbarch_data*);


/* Register per-architecture memory region.

   Provide a memory-region swap mechanism.  Per-architecture memory
   region are created.  These memory regions are swapped whenever the
   architecture is changed.  For a new architecture, the memory region
   is initialized with zero (0) and the INIT function is called.

   Memory regions are swapped / initialized in the order that they are
   registered.  NULL DATA and/or INIT values can be specified.

   New code should use register_gdbarch_data(). */

typedef void (gdbarch_swap_ftype) (void);
extern void register_gdbarch_swap (void *data, unsigned long size, gdbarch_swap_ftype *init);
#define REGISTER_GDBARCH_SWAP(VAR) register_gdbarch_swap (&(VAR), sizeof ((VAR)), NULL)



/* The target-system-dependent byte order is dynamic */

extern int target_byte_order;
#ifndef TARGET_BYTE_ORDER
#define TARGET_BYTE_ORDER (target_byte_order + 0)
#endif

extern int target_byte_order_auto;
#ifndef TARGET_BYTE_ORDER_AUTO
#define TARGET_BYTE_ORDER_AUTO (target_byte_order_auto + 0)
#endif



/* The target-system-dependent BFD architecture is dynamic */

extern int target_architecture_auto;
#ifndef TARGET_ARCHITECTURE_AUTO
#define TARGET_ARCHITECTURE_AUTO (target_architecture_auto + 0)
#endif

extern const struct bfd_arch_info *target_architecture;
#ifndef TARGET_ARCHITECTURE
#define TARGET_ARCHITECTURE (target_architecture + 0)
#endif


/* The target-system-dependent disassembler is semi-dynamic */

extern int dis_asm_read_memory (bfd_vma memaddr, bfd_byte *myaddr,
				unsigned int len, disassemble_info *info);

extern void dis_asm_memory_error (int status, bfd_vma memaddr,
				  disassemble_info *info);

extern void dis_asm_print_address (bfd_vma addr,
				   disassemble_info *info);

extern int (*tm_print_insn) (bfd_vma, disassemble_info*);
extern disassemble_info tm_print_insn_info;
#ifndef TARGET_PRINT_INSN_INFO
#define TARGET_PRINT_INSN_INFO (&tm_print_insn_info)
#endif



/* Set the dynamic target-system-dependent parameters (architecture,
   byte-order, ...) using information found in the BFD */

extern void set_gdbarch_from_file (bfd *);


/* Initialize the current architecture to the "first" one we find on
   our list.  */

extern void initialize_current_architecture (void);

/* For non-multiarched targets, do any initialization of the default
   gdbarch object necessary after the _initialize_MODULE functions
   have run.  */
extern void initialize_non_multiarch ();

/* gdbarch trace variable */
extern int gdbarch_debug;

extern void gdbarch_dump (struct gdbarch *gdbarch, struct ui_file *file);

#endif
EOF
exec 1>&2
#../move-if-change new-gdbarch.h gdbarch.h
compare_new gdbarch.h


#
# C file
#

exec > new-gdbarch.c
copyright
cat <<EOF

#include "defs.h"
#include "arch-utils.h"

#if GDB_MULTI_ARCH
#include "gdbcmd.h"
#include "inferior.h" /* enum CALL_DUMMY_LOCATION et.al. */
#else
/* Just include everything in sight so that the every old definition
   of macro is visible. */
#include "gdb_string.h"
#include <ctype.h>
#include "symtab.h"
#include "frame.h"
#include "inferior.h"
#include "breakpoint.h"
#include "gdb_wait.h"
#include "gdbcore.h"
#include "gdbcmd.h"
#include "target.h"
#include "gdbthread.h"
#include "annotate.h"
#include "symfile.h"		/* for overlay functions */
#include "value.h"		/* For old tm.h/nm.h macros.  */
#endif
#include "symcat.h"

#include "floatformat.h"

#include "gdb_assert.h"
#include "gdb-events.h"

/* Static function declarations */

static void verify_gdbarch (struct gdbarch *gdbarch);
static void alloc_gdbarch_data (struct gdbarch *);
static void init_gdbarch_data (struct gdbarch *);
static void free_gdbarch_data (struct gdbarch *);
static void init_gdbarch_swap (struct gdbarch *);
static void swapout_gdbarch_swap (struct gdbarch *);
static void swapin_gdbarch_swap (struct gdbarch *);

/* Convenience macro for allocting typesafe memory. */

#ifndef XMALLOC
#define XMALLOC(TYPE) (TYPE*) xmalloc (sizeof (TYPE))
#endif


/* Non-zero if we want to trace architecture code.  */

#ifndef GDBARCH_DEBUG
#define GDBARCH_DEBUG 0
#endif
int gdbarch_debug = GDBARCH_DEBUG;

EOF

# gdbarch open the gdbarch object
printf "\n"
printf "/* Maintain the struct gdbarch object */\n"
printf "\n"
printf "struct gdbarch\n"
printf "{\n"
printf "  /* basic architectural information */\n"
function_list | while do_read
do
    if class_is_info_p
    then
	printf "  ${returntype} ${function};\n"
    fi
done
printf "\n"
printf "  /* target specific vector. */\n"
printf "  struct gdbarch_tdep *tdep;\n"
printf "  gdbarch_dump_tdep_ftype *dump_tdep;\n"
printf "\n"
printf "  /* per-architecture data-pointers */\n"
printf "  unsigned nr_data;\n"
printf "  void **data;\n"
printf "\n"
printf "  /* per-architecture swap-regions */\n"
printf "  struct gdbarch_swap *swap;\n"
printf "\n"
cat <<EOF
  /* Multi-arch values.

     When extending this structure you must:

     Add the field below.

     Declare set/get functions and define the corresponding
     macro in gdbarch.h.

     gdbarch_alloc(): If zero/NULL is not a suitable default,
     initialize the new field.

     verify_gdbarch(): Confirm that the target updated the field
     correctly.

     gdbarch_dump(): Add a fprintf_unfiltered call so that the new
     field is dumped out

     \`\`startup_gdbarch()'': Append an initial value to the static
     variable (base values on the host's c-type system).

     get_gdbarch(): Implement the set/get functions (probably using
     the macro's as shortcuts).

     */

EOF
function_list | while do_read
do
    if class_is_variable_p
    then
	printf "  ${returntype} ${function};\n"
    elif class_is_function_p
    then
	printf "  gdbarch_${function}_ftype *${function}${attrib};\n"
    fi
done
printf "};\n"

# A pre-initialized vector
printf "\n"
printf "\n"
cat <<EOF
/* The default architecture uses host values (for want of a better
   choice). */
EOF
printf "\n"
printf "extern const struct bfd_arch_info bfd_default_arch_struct;\n"
printf "\n"
printf "struct gdbarch startup_gdbarch =\n"
printf "{\n"
printf "  /* basic architecture information */\n"
function_list | while do_read
do
    if class_is_info_p
    then
	printf "  ${staticdefault},\n"
    fi
done
cat <<EOF
  /* target specific vector and its dump routine */
  NULL, NULL,
  /*per-architecture data-pointers and swap regions */
  0, NULL, NULL,
  /* Multi-arch values */
EOF
function_list | while do_read
do
    if class_is_function_p || class_is_variable_p
    then
	printf "  ${staticdefault},\n"
    fi
done
cat <<EOF
  /* startup_gdbarch() */
};

struct gdbarch *current_gdbarch = &startup_gdbarch;

/* Do any initialization needed for a non-multiarch configuration
   after the _initialize_MODULE functions have been run.  */
void
initialize_non_multiarch ()
{
  alloc_gdbarch_data (&startup_gdbarch);
  init_gdbarch_data (&startup_gdbarch);
}
EOF

# Create a new gdbarch struct
printf "\n"
printf "\n"
cat <<EOF
/* Create a new \`\`struct gdbarch'' based on information provided by
   \`\`struct gdbarch_info''. */
EOF
printf "\n"
cat <<EOF
struct gdbarch *
gdbarch_alloc (const struct gdbarch_info *info,
               struct gdbarch_tdep *tdep)
{
  /* NOTE: The new architecture variable is named \`\`current_gdbarch''
     so that macros such as TARGET_DOUBLE_BIT, when expanded, refer to
     the current local architecture and not the previous global
     architecture.  This ensures that the new architectures initial
     values are not influenced by the previous architecture.  Once
     everything is parameterised with gdbarch, this will go away.  */
  struct gdbarch *current_gdbarch = XMALLOC (struct gdbarch);
  memset (current_gdbarch, 0, sizeof (*current_gdbarch));

  alloc_gdbarch_data (current_gdbarch);

  current_gdbarch->tdep = tdep;
EOF
printf "\n"
function_list | while do_read
do
    if class_is_info_p
    then
	printf "  current_gdbarch->${function} = info->${function};\n"
    fi
done
printf "\n"
printf "  /* Force the explicit initialization of these. */\n"
function_list | while do_read
do
    if class_is_function_p || class_is_variable_p
    then
	if [ -n "${predefault}" -a "x${predefault}" != "x0" ]
	then
	  printf "  current_gdbarch->${function} = ${predefault};\n"
	fi
    fi
done
cat <<EOF
  /* gdbarch_alloc() */

  return current_gdbarch;
}
EOF

# Free a gdbarch struct.
printf "\n"
printf "\n"
cat <<EOF
/* Free a gdbarch struct.  This should never happen in normal
   operation --- once you've created a gdbarch, you keep it around.
   However, if an architecture's init function encounters an error
   building the structure, it may need to clean up a partially
   constructed gdbarch.  */

void
gdbarch_free (struct gdbarch *arch)
{
  gdb_assert (arch != NULL);
  free_gdbarch_data (arch);
  xfree (arch);
}
EOF

# verify a new architecture
printf "\n"
printf "\n"
printf "/* Ensure that all values in a GDBARCH are reasonable. */\n"
printf "\n"
cat <<EOF
static void
verify_gdbarch (struct gdbarch *gdbarch)
{
  struct ui_file *log;
  struct cleanup *cleanups;
  long dummy;
  char *buf;
  /* Only perform sanity checks on a multi-arch target. */
  if (!GDB_MULTI_ARCH)
    return;
  log = mem_fileopen ();
  cleanups = make_cleanup_ui_file_delete (log);
  /* fundamental */
  if (gdbarch->byte_order == BFD_ENDIAN_UNKNOWN)
    fprintf_unfiltered (log, "\n\tbyte-order");
  if (gdbarch->bfd_arch_info == NULL)
    fprintf_unfiltered (log, "\n\tbfd_arch_info");
  /* Check those that need to be defined for the given multi-arch level. */
EOF
function_list | while do_read
do
    if class_is_function_p || class_is_variable_p
    then
	if [ "x${invalid_p}" = "x0" ]
	then
	    printf "  /* Skip verify of ${function}, invalid_p == 0 */\n"
	elif class_is_predicate_p
	then
	    printf "  /* Skip verify of ${function}, has predicate */\n"
	# FIXME: See do_read for potential simplification
 	elif [ -n "${invalid_p}" -a -n "${postdefault}" ]
	then
	    printf "  if (${invalid_p})\n"
	    printf "    gdbarch->${function} = ${postdefault};\n"
	elif [ -n "${predefault}" -a -n "${postdefault}" ]
	then
	    printf "  if (gdbarch->${function} == ${predefault})\n"
	    printf "    gdbarch->${function} = ${postdefault};\n"
	elif [ -n "${postdefault}" ]
	then
	    printf "  if (gdbarch->${function} == 0)\n"
	    printf "    gdbarch->${function} = ${postdefault};\n"
	elif [ -n "${invalid_p}" ]
	then
	    printf "  if ((GDB_MULTI_ARCH ${gt_level})\n"
	    printf "      && (${invalid_p}))\n"
	    printf "    fprintf_unfiltered (log, \"\\\\n\\\\t${function}\");\n"
	elif [ -n "${predefault}" ]
	then
	    printf "  if ((GDB_MULTI_ARCH ${gt_level})\n"
	    printf "      && (gdbarch->${function} == ${predefault}))\n"
	    printf "    fprintf_unfiltered (log, \"\\\\n\\\\t${function}\");\n"
	fi
    fi
done
cat <<EOF
  buf = ui_file_xstrdup (log, &dummy);
  make_cleanup (xfree, buf);
  if (strlen (buf) > 0)
    internal_error (__FILE__, __LINE__,
                    "verify_gdbarch: the following are invalid ...%s",
                    buf);
  do_cleanups (cleanups);
}
EOF

# dump the structure
printf "\n"
printf "\n"
cat <<EOF
/* Print out the details of the current architecture. */

/* NOTE/WARNING: The parameter is called \`\`current_gdbarch'' so that it
   just happens to match the global variable \`\`current_gdbarch''.  That
   way macros refering to that variable get the local and not the global
   version - ulgh.  Once everything is parameterised with gdbarch, this
   will go away. */

void
gdbarch_dump (struct gdbarch *gdbarch, struct ui_file *file)
{
  fprintf_unfiltered (file,
                      "gdbarch_dump: GDB_MULTI_ARCH = %d\\n",
                      GDB_MULTI_ARCH);
EOF
function_list | sort -t: +2 | while do_read
do
    # multiarch functions don't have macros.
    if class_is_multiarch_p
    then
	printf "  if (GDB_MULTI_ARCH)\n"
	printf "    fprintf_unfiltered (file,\n"
	printf "                        \"gdbarch_dump: ${function} = 0x%%08lx\\\\n\",\n"
	printf "                        (long) current_gdbarch->${function});\n"
	continue
    fi
    # Print the macro definition.
    printf "#ifdef ${macro}\n"
    if [ "x${returntype}" = "xvoid" ]
    then
	printf "#if GDB_MULTI_ARCH\n"
	printf "  /* Macro might contain \`[{}]' when not multi-arch */\n"
    fi
    if class_is_function_p
    then
	printf "  fprintf_unfiltered (file,\n"
	printf "                      \"gdbarch_dump: %%s # %%s\\\\n\",\n"
	printf "                      \"${macro}(${actual})\",\n"
	printf "                      XSTRING (${macro} (${actual})));\n"
    else
	printf "  fprintf_unfiltered (file,\n"
	printf "                      \"gdbarch_dump: ${macro} # %%s\\\\n\",\n"
	printf "                      XSTRING (${macro}));\n"
    fi
    # Print the architecture vector value
    if [ "x${returntype}" = "xvoid" ]
    then
	printf "#endif\n"
    fi
    if [ "x${print_p}" = "x()" ]
    then
        printf "  gdbarch_dump_${function} (current_gdbarch);\n"
    elif [ "x${print_p}" = "x0" ]
    then
        printf "  /* skip print of ${macro}, print_p == 0. */\n"
    elif [ -n "${print_p}" ]
    then
        printf "  if (${print_p})\n"
	printf "    fprintf_unfiltered (file,\n"
	printf "                        \"gdbarch_dump: ${macro} = %s\\\\n\",\n" "${fmt}"
	printf "                        ${print});\n"
    elif class_is_function_p
    then
	printf "  if (GDB_MULTI_ARCH)\n"
	printf "    fprintf_unfiltered (file,\n"
	printf "                        \"gdbarch_dump: ${macro} = 0x%%08lx\\\\n\",\n"
	printf "                        (long) current_gdbarch->${function}\n"
	printf "                        /*${macro} ()*/);\n"
    else
	printf "  fprintf_unfiltered (file,\n"
	printf "                      \"gdbarch_dump: ${macro} = %s\\\\n\",\n" "${fmt}"
	printf "                      ${print});\n"
    fi
    printf "#endif\n"
done
cat <<EOF
  if (current_gdbarch->dump_tdep != NULL)
    current_gdbarch->dump_tdep (current_gdbarch, file);
}
EOF


# GET/SET
printf "\n"
cat <<EOF
struct gdbarch_tdep *
gdbarch_tdep (struct gdbarch *gdbarch)
{
  if (gdbarch_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "gdbarch_tdep called\\n");
  return gdbarch->tdep;
}
EOF
printf "\n"
function_list | while do_read
do
    if class_is_predicate_p
    then
	printf "\n"
	printf "int\n"
	printf "gdbarch_${function}_p (struct gdbarch *gdbarch)\n"
	printf "{\n"
	if [ -n "${valid_p}" ]
	then
	    printf "  return ${valid_p};\n"
	else
	    printf "#error \"gdbarch_${function}_p: not defined\"\n"
	fi
	printf "}\n"
    fi
    if class_is_function_p
    then
	printf "\n"
	printf "${returntype}\n"
	if [ "x${formal}" = "xvoid" ]
	then
	  printf "gdbarch_${function} (struct gdbarch *gdbarch)\n"
	else
	  printf "gdbarch_${function} (struct gdbarch *gdbarch, ${formal})\n"
	fi
	printf "{\n"
        printf "  if (gdbarch->${function} == 0)\n"
        printf "    internal_error (__FILE__, __LINE__,\n"
	printf "                    \"gdbarch: gdbarch_${function} invalid\");\n"
	printf "  if (gdbarch_debug >= 2)\n"
	printf "    fprintf_unfiltered (gdb_stdlog, \"gdbarch_${function} called\\\\n\");\n"
	if [ "x${actual}" = "x-" -o "x${actual}" = "x" ]
	then
	    if class_is_multiarch_p
	    then
		params="gdbarch"
	    else
		params=""
	    fi
	else
	    if class_is_multiarch_p
	    then
		params="gdbarch, ${actual}"
	    else
		params="${actual}"
	    fi
        fi
       	if [ "x${returntype}" = "xvoid" ]
	then
	  printf "  gdbarch->${function} (${params});\n"
	else
	  printf "  return gdbarch->${function} (${params});\n"
	fi
	printf "}\n"
	printf "\n"
	printf "void\n"
	printf "set_gdbarch_${function} (struct gdbarch *gdbarch,\n"
        printf "            `echo ${function} | sed -e 's/./ /g'`  gdbarch_${function}_ftype ${function})\n"
	printf "{\n"
	printf "  gdbarch->${function} = ${function};\n"
	printf "}\n"
    elif class_is_variable_p
    then
	printf "\n"
	printf "${returntype}\n"
	printf "gdbarch_${function} (struct gdbarch *gdbarch)\n"
	printf "{\n"
	if [ "x${invalid_p}" = "x0" ]
	then
	    printf "  /* Skip verify of ${function}, invalid_p == 0 */\n"
	elif [ -n "${invalid_p}" ]
	then
	  printf "  if (${invalid_p})\n"
	  printf "    internal_error (__FILE__, __LINE__,\n"
	  printf "                    \"gdbarch: gdbarch_${function} invalid\");\n"
	elif [ -n "${predefault}" ]
	then
	  printf "  if (gdbarch->${function} == ${predefault})\n"
	  printf "    internal_error (__FILE__, __LINE__,\n"
	  printf "                    \"gdbarch: gdbarch_${function} invalid\");\n"
	fi
	printf "  if (gdbarch_debug >= 2)\n"
	printf "    fprintf_unfiltered (gdb_stdlog, \"gdbarch_${function} called\\\\n\");\n"
	printf "  return gdbarch->${function};\n"
	printf "}\n"
	printf "\n"
	printf "void\n"
	printf "set_gdbarch_${function} (struct gdbarch *gdbarch,\n"
        printf "            `echo ${function} | sed -e 's/./ /g'`  ${returntype} ${function})\n"
	printf "{\n"
	printf "  gdbarch->${function} = ${function};\n"
	printf "}\n"
    elif class_is_info_p
    then
	printf "\n"
	printf "${returntype}\n"
	printf "gdbarch_${function} (struct gdbarch *gdbarch)\n"
	printf "{\n"
	printf "  if (gdbarch_debug >= 2)\n"
	printf "    fprintf_unfiltered (gdb_stdlog, \"gdbarch_${function} called\\\\n\");\n"
	printf "  return gdbarch->${function};\n"
	printf "}\n"
    fi
done

# All the trailing guff
cat <<EOF


/* Keep a registry of per-architecture data-pointers required by GDB
   modules. */

struct gdbarch_data
{
  unsigned index;
  gdbarch_data_init_ftype *init;
  gdbarch_data_free_ftype *free;
};

struct gdbarch_data_registration
{
  struct gdbarch_data *data;
  struct gdbarch_data_registration *next;
};

struct gdbarch_data_registry
{
  unsigned nr;
  struct gdbarch_data_registration *registrations;
};

struct gdbarch_data_registry gdbarch_data_registry =
{
  0, NULL,
};

struct gdbarch_data *
register_gdbarch_data (gdbarch_data_init_ftype *init,
                       gdbarch_data_free_ftype *free)
{
  struct gdbarch_data_registration **curr;
  for (curr = &gdbarch_data_registry.registrations;
       (*curr) != NULL;
       curr = &(*curr)->next);
  (*curr) = XMALLOC (struct gdbarch_data_registration);
  (*curr)->next = NULL;
  (*curr)->data = XMALLOC (struct gdbarch_data);
  (*curr)->data->index = gdbarch_data_registry.nr++;
  (*curr)->data->init = init;
  (*curr)->data->free = free;
  return (*curr)->data;
}


/* Walk through all the registered users initializing each in turn. */

static void
init_gdbarch_data (struct gdbarch *gdbarch)
{
  struct gdbarch_data_registration *rego;
  for (rego = gdbarch_data_registry.registrations;
       rego != NULL;
       rego = rego->next)
    {
      struct gdbarch_data *data = rego->data;
      gdb_assert (data->index < gdbarch->nr_data);
      if (data->init != NULL)
        {
          void *pointer = data->init (gdbarch);
          set_gdbarch_data (gdbarch, data, pointer);
        }
    }
}

/* Create/delete the gdbarch data vector. */

static void
alloc_gdbarch_data (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch->data == NULL);
  gdbarch->nr_data = gdbarch_data_registry.nr;
  gdbarch->data = xcalloc (gdbarch->nr_data, sizeof (void*));
}

static void
free_gdbarch_data (struct gdbarch *gdbarch)
{
  struct gdbarch_data_registration *rego;
  gdb_assert (gdbarch->data != NULL);
  for (rego = gdbarch_data_registry.registrations;
       rego != NULL;
       rego = rego->next)
    {
      struct gdbarch_data *data = rego->data;
      gdb_assert (data->index < gdbarch->nr_data);
      if (data->free != NULL && gdbarch->data[data->index] != NULL)
        {
          data->free (gdbarch, gdbarch->data[data->index]);
          gdbarch->data[data->index] = NULL;
        }
    }
  xfree (gdbarch->data);
  gdbarch->data = NULL;
}


/* Initialize the current value of thee specified per-architecture
   data-pointer. */

void
set_gdbarch_data (struct gdbarch *gdbarch,
                  struct gdbarch_data *data,
                  void *pointer)
{
  gdb_assert (data->index < gdbarch->nr_data);
  if (data->free != NULL && gdbarch->data[data->index] != NULL)
    data->free (gdbarch, gdbarch->data[data->index]);
  gdbarch->data[data->index] = pointer;
}

/* Return the current value of the specified per-architecture
   data-pointer. */

void *
gdbarch_data (struct gdbarch_data *data)
{
  gdb_assert (data->index < current_gdbarch->nr_data);
  return current_gdbarch->data[data->index];
}



/* Keep a registry of swapped data required by GDB modules. */

struct gdbarch_swap
{
  void *swap;
  struct gdbarch_swap_registration *source;
  struct gdbarch_swap *next;
};

struct gdbarch_swap_registration
{
  void *data;
  unsigned long sizeof_data;
  gdbarch_swap_ftype *init;
  struct gdbarch_swap_registration *next;
};

struct gdbarch_swap_registry
{
  int nr;
  struct gdbarch_swap_registration *registrations;
};

struct gdbarch_swap_registry gdbarch_swap_registry = 
{
  0, NULL,
};

void
register_gdbarch_swap (void *data,
		       unsigned long sizeof_data,
		       gdbarch_swap_ftype *init)
{
  struct gdbarch_swap_registration **rego;
  for (rego = &gdbarch_swap_registry.registrations;
       (*rego) != NULL;
       rego = &(*rego)->next);
  (*rego) = XMALLOC (struct gdbarch_swap_registration);
  (*rego)->next = NULL;
  (*rego)->init = init;
  (*rego)->data = data;
  (*rego)->sizeof_data = sizeof_data;
}


static void
init_gdbarch_swap (struct gdbarch *gdbarch)
{
  struct gdbarch_swap_registration *rego;
  struct gdbarch_swap **curr = &gdbarch->swap;
  for (rego = gdbarch_swap_registry.registrations;
       rego != NULL;
       rego = rego->next)
    {
      if (rego->data != NULL)
	{
	  (*curr) = XMALLOC (struct gdbarch_swap);
	  (*curr)->source = rego;
	  (*curr)->swap = xmalloc (rego->sizeof_data);
	  (*curr)->next = NULL;
	  memset (rego->data, 0, rego->sizeof_data);
	  curr = &(*curr)->next;
	}
      if (rego->init != NULL)
	rego->init ();
    }
}

static void
swapout_gdbarch_swap (struct gdbarch *gdbarch)
{
  struct gdbarch_swap *curr;
  for (curr = gdbarch->swap;
       curr != NULL;
       curr = curr->next)
    memcpy (curr->swap, curr->source->data, curr->source->sizeof_data);
}

static void
swapin_gdbarch_swap (struct gdbarch *gdbarch)
{
  struct gdbarch_swap *curr;
  for (curr = gdbarch->swap;
       curr != NULL;
       curr = curr->next)
    memcpy (curr->source->data, curr->swap, curr->source->sizeof_data);
}


/* Keep a registry of the architectures known by GDB. */

struct gdbarch_registration
{
  enum bfd_architecture bfd_architecture;
  gdbarch_init_ftype *init;
  gdbarch_dump_tdep_ftype *dump_tdep;
  struct gdbarch_list *arches;
  struct gdbarch_registration *next;
};

static struct gdbarch_registration *gdbarch_registry = NULL;

static void
append_name (const char ***buf, int *nr, const char *name)
{
  *buf = xrealloc (*buf, sizeof (char**) * (*nr + 1));
  (*buf)[*nr] = name;
  *nr += 1;
}

const char **
gdbarch_printable_names (void)
{
  if (GDB_MULTI_ARCH)
    {
      /* Accumulate a list of names based on the registed list of
         architectures. */
      enum bfd_architecture a;
      int nr_arches = 0;
      const char **arches = NULL;
      struct gdbarch_registration *rego;
      for (rego = gdbarch_registry;
	   rego != NULL;
	   rego = rego->next)
	{
	  const struct bfd_arch_info *ap;
	  ap = bfd_lookup_arch (rego->bfd_architecture, 0);
	  if (ap == NULL)
	    internal_error (__FILE__, __LINE__,
                            "gdbarch_architecture_names: multi-arch unknown");
	  do
	    {
	      append_name (&arches, &nr_arches, ap->printable_name);
	      ap = ap->next;
	    }
	  while (ap != NULL);
	}
      append_name (&arches, &nr_arches, NULL);
      return arches;
    }
  else
    /* Just return all the architectures that BFD knows.  Assume that
       the legacy architecture framework supports them. */
    return bfd_arch_list ();
}


void
gdbarch_register (enum bfd_architecture bfd_architecture,
                  gdbarch_init_ftype *init,
		  gdbarch_dump_tdep_ftype *dump_tdep)
{
  struct gdbarch_registration **curr;
  const struct bfd_arch_info *bfd_arch_info;
  /* Check that BFD recognizes this architecture */
  bfd_arch_info = bfd_lookup_arch (bfd_architecture, 0);
  if (bfd_arch_info == NULL)
    {
      internal_error (__FILE__, __LINE__,
                      "gdbarch: Attempt to register unknown architecture (%d)",
                      bfd_architecture);
    }
  /* Check that we haven't seen this architecture before */
  for (curr = &gdbarch_registry;
       (*curr) != NULL;
       curr = &(*curr)->next)
    {
      if (bfd_architecture == (*curr)->bfd_architecture)
	internal_error (__FILE__, __LINE__,
                        "gdbarch: Duplicate registraration of architecture (%s)",
	                bfd_arch_info->printable_name);
    }
  /* log it */
  if (gdbarch_debug)
    fprintf_unfiltered (gdb_stdlog, "register_gdbarch_init (%s, 0x%08lx)\n",
			bfd_arch_info->printable_name,
			(long) init);
  /* Append it */
  (*curr) = XMALLOC (struct gdbarch_registration);
  (*curr)->bfd_architecture = bfd_architecture;
  (*curr)->init = init;
  (*curr)->dump_tdep = dump_tdep;
  (*curr)->arches = NULL;
  (*curr)->next = NULL;
  /* When non- multi-arch, install whatever target dump routine we've
     been provided - hopefully that routine has been written correctly
     and works regardless of multi-arch. */
  if (!GDB_MULTI_ARCH && dump_tdep != NULL
      && startup_gdbarch.dump_tdep == NULL)
    startup_gdbarch.dump_tdep = dump_tdep;
}

void
register_gdbarch_init (enum bfd_architecture bfd_architecture,
		       gdbarch_init_ftype *init)
{
  gdbarch_register (bfd_architecture, init, NULL);
}


/* Look for an architecture using gdbarch_info.  Base search on only
   BFD_ARCH_INFO and BYTE_ORDER. */

struct gdbarch_list *
gdbarch_list_lookup_by_info (struct gdbarch_list *arches,
                             const struct gdbarch_info *info)
{
  for (; arches != NULL; arches = arches->next)
    {
      if (info->bfd_arch_info != arches->gdbarch->bfd_arch_info)
	continue;
      if (info->byte_order != arches->gdbarch->byte_order)
	continue;
      return arches;
    }
  return NULL;
}


/* Update the current architecture. Return ZERO if the update request
   failed. */

int
gdbarch_update_p (struct gdbarch_info info)
{
  struct gdbarch *new_gdbarch;
  struct gdbarch_list **list;
  struct gdbarch_registration *rego;

  /* Fill in missing parts of the INFO struct using a number of
     sources: \`\`set ...''; INFOabfd supplied; existing target.  */

  /* \`\`(gdb) set architecture ...'' */
  if (info.bfd_arch_info == NULL
      && !TARGET_ARCHITECTURE_AUTO)
    info.bfd_arch_info = TARGET_ARCHITECTURE;
  if (info.bfd_arch_info == NULL
      && info.abfd != NULL
      && bfd_get_arch (info.abfd) != bfd_arch_unknown
      && bfd_get_arch (info.abfd) != bfd_arch_obscure)
    info.bfd_arch_info = bfd_get_arch_info (info.abfd);
  if (info.bfd_arch_info == NULL)
    info.bfd_arch_info = TARGET_ARCHITECTURE;

  /* \`\`(gdb) set byte-order ...'' */
  if (info.byte_order == BFD_ENDIAN_UNKNOWN
      && !TARGET_BYTE_ORDER_AUTO)
    info.byte_order = TARGET_BYTE_ORDER;
  /* From the INFO struct. */
  if (info.byte_order == BFD_ENDIAN_UNKNOWN
      && info.abfd != NULL)
    info.byte_order = (bfd_big_endian (info.abfd) ? BFD_ENDIAN_BIG
		       : bfd_little_endian (info.abfd) ? BFD_ENDIAN_LITTLE
		       : BFD_ENDIAN_UNKNOWN);
  /* From the current target. */
  if (info.byte_order == BFD_ENDIAN_UNKNOWN)
    info.byte_order = TARGET_BYTE_ORDER;

  /* Must have found some sort of architecture. */
  gdb_assert (info.bfd_arch_info != NULL);

  if (gdbarch_debug)
    {
      fprintf_unfiltered (gdb_stdlog,
			  "gdbarch_update: info.bfd_arch_info %s\n",
			  (info.bfd_arch_info != NULL
			   ? info.bfd_arch_info->printable_name
			   : "(null)"));
      fprintf_unfiltered (gdb_stdlog,
			  "gdbarch_update: info.byte_order %d (%s)\n",
			  info.byte_order,
			  (info.byte_order == BFD_ENDIAN_BIG ? "big"
			   : info.byte_order == BFD_ENDIAN_LITTLE ? "little"
			   : "default"));
      fprintf_unfiltered (gdb_stdlog,
			  "gdbarch_update: info.abfd 0x%lx\n",
			  (long) info.abfd);
      fprintf_unfiltered (gdb_stdlog,
			  "gdbarch_update: info.tdep_info 0x%lx\n",
			  (long) info.tdep_info);
    }

  /* Find the target that knows about this architecture. */
  for (rego = gdbarch_registry;
       rego != NULL;
       rego = rego->next)
    if (rego->bfd_architecture == info.bfd_arch_info->arch)
      break;
  if (rego == NULL)
    {
      if (gdbarch_debug)
	fprintf_unfiltered (gdb_stdlog, "gdbarch_update: No matching architecture\\n");
      return 0;
    }

  /* Ask the target for a replacement architecture. */
  new_gdbarch = rego->init (info, rego->arches);

  /* Did the target like it?  No. Reject the change. */
  if (new_gdbarch == NULL)
    {
      if (gdbarch_debug)
	fprintf_unfiltered (gdb_stdlog, "gdbarch_update: Target rejected architecture\\n");
      return 0;
    }

  /* Did the architecture change?  No. Do nothing. */
  if (current_gdbarch == new_gdbarch)
    {
      if (gdbarch_debug)
	fprintf_unfiltered (gdb_stdlog, "gdbarch_update: Architecture 0x%08lx (%s) unchanged\\n",
			    (long) new_gdbarch,
			    new_gdbarch->bfd_arch_info->printable_name);
      return 1;
    }

  /* Swap all data belonging to the old target out */
  swapout_gdbarch_swap (current_gdbarch);

  /* Is this a pre-existing architecture?  Yes. Swap it in.  */
  for (list = &rego->arches;
       (*list) != NULL;
       list = &(*list)->next)
    {
      if ((*list)->gdbarch == new_gdbarch)
	{
	  if (gdbarch_debug)
	    fprintf_unfiltered (gdb_stdlog,
                                "gdbarch_update: Previous architecture 0x%08lx (%s) selected\\n",
				(long) new_gdbarch,
				new_gdbarch->bfd_arch_info->printable_name);
	  current_gdbarch = new_gdbarch;
	  swapin_gdbarch_swap (new_gdbarch);
	  architecture_changed_event ();
	  return 1;
	}
    }

  /* Append this new architecture to this targets list. */
  (*list) = XMALLOC (struct gdbarch_list);
  (*list)->next = NULL;
  (*list)->gdbarch = new_gdbarch;

  /* Switch to this new architecture.  Dump it out. */
  current_gdbarch = new_gdbarch;
  if (gdbarch_debug)
    {
      fprintf_unfiltered (gdb_stdlog,
			  "gdbarch_update: New architecture 0x%08lx (%s) selected\\n",
			  (long) new_gdbarch,
			  new_gdbarch->bfd_arch_info->printable_name);
    }
  
  /* Check that the newly installed architecture is valid.  Plug in
     any post init values.  */
  new_gdbarch->dump_tdep = rego->dump_tdep;
  verify_gdbarch (new_gdbarch);

  /* Initialize the per-architecture memory (swap) areas.
     CURRENT_GDBARCH must be update before these modules are
     called. */
  init_gdbarch_swap (new_gdbarch);
  
  /* Initialize the per-architecture data-pointer of all parties that
     registered an interest in this architecture.  CURRENT_GDBARCH
     must be updated before these modules are called. */
  init_gdbarch_data (new_gdbarch);
  architecture_changed_event ();

  if (gdbarch_debug)
    gdbarch_dump (current_gdbarch, gdb_stdlog);

  return 1;
}


/* Disassembler */

/* Pointer to the target-dependent disassembly function.  */
int (*tm_print_insn) (bfd_vma, disassemble_info *);
disassemble_info tm_print_insn_info;


extern void _initialize_gdbarch (void);

void
_initialize_gdbarch (void)
{
  struct cmd_list_element *c;

  INIT_DISASSEMBLE_INFO_NO_ARCH (tm_print_insn_info, gdb_stdout, (fprintf_ftype)fprintf_filtered);
  tm_print_insn_info.flavour = bfd_target_unknown_flavour;
  tm_print_insn_info.read_memory_func = dis_asm_read_memory;
  tm_print_insn_info.memory_error_func = dis_asm_memory_error;
  tm_print_insn_info.print_address_func = dis_asm_print_address;

  add_show_from_set (add_set_cmd ("arch",
				  class_maintenance,
				  var_zinteger,
				  (char *)&gdbarch_debug,
				  "Set architecture debugging.\\n\\
When non-zero, architecture debugging is enabled.", &setdebuglist),
		     &showdebuglist);
  c = add_set_cmd ("archdebug",
		   class_maintenance,
		   var_zinteger,
		   (char *)&gdbarch_debug,
		   "Set architecture debugging.\\n\\
When non-zero, architecture debugging is enabled.", &setlist);

  deprecate_cmd (c, "set debug arch");
  deprecate_cmd (add_show_from_set (c, &showlist), "show debug arch");
}
EOF

# close things off
exec 1>&2
#../move-if-change new-gdbarch.c gdbarch.c
compare_new gdbarch.c

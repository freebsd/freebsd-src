/* <apollo/dst.h> */
/* Apollo object module DST (debug symbol table) description */

#ifndef apollo_dst_h
#define apollo_dst_h

#if defined(apollo) && !defined(__GNUC__)
#define ALIGNED1  __attribute( (aligned(1)) )
#else
/* Remove attribute directives from non-Apollo code: */
#define ALIGNED1		/* nil */
#endif



/* Identification of this version of the debug symbol table.  Producers of the
   debug symbol table must write these values into the version number field of
   the compilation unit record in .blocks .
 */
#define dst_version_major    1
#define dst_version_minor    3


/*
   ** Enumeration of debug record types appearing in .blocks and .symbols ...
 */
typedef enum
  {
    dst_typ_pad,		/*  0 */
    dst_typ_comp_unit,		/*  1 */
    dst_typ_section_tab,	/*  2 */
    dst_typ_file_tab,		/*  3 */
    dst_typ_block,		/*  4 */
    dst_typ_5,
    dst_typ_var,
    dst_typ_pointer,		/*  7 */
    dst_typ_array,		/*  8 */
    dst_typ_subrange,		/*  9 */
    dst_typ_set,		/* 10 */
    dst_typ_implicit_enum,	/* 11 */
    dst_typ_explicit_enum,	/* 12 */
    dst_typ_short_rec,		/* 13 */
    dst_typ_old_record,
    dst_typ_short_union,	/* 15 */
    dst_typ_old_union,
    dst_typ_file,		/* 17 */
    dst_typ_offset,		/* 18 */
    dst_typ_alias,		/* 19 */
    dst_typ_signature,		/* 20 */
    dst_typ_21,
    dst_typ_old_label,		/* 22 */
    dst_typ_scope,		/* 23 */
    dst_typ_end_scope,		/* 24 */
    dst_typ_25,
    dst_typ_26,
    dst_typ_string_tab,		/* 27 */
    dst_typ_global_name_tab,	/* 28 */
    dst_typ_forward,		/* 29 */
    dst_typ_aux_size,		/* 30 */
    dst_typ_aux_align,		/* 31 */
    dst_typ_aux_field_size,	/* 32 */
    dst_typ_aux_field_off,	/* 33 */
    dst_typ_aux_field_align,	/* 34 */
    dst_typ_aux_qual,		/* 35 */
    dst_typ_aux_var_bound,	/* 36 */
    dst_typ_extension,		/* 37 */
    dst_typ_string,		/* 38 */
    dst_typ_old_entry,
    dst_typ_const,		/* 40 */
    dst_typ_reference,		/* 41 */
    dst_typ_record,		/* 42 */
    dst_typ_union,		/* 43 */
    dst_typ_aux_type_deriv,	/* 44 */
    dst_typ_locpool,		/* 45 */
    dst_typ_variable,		/* 46 */
    dst_typ_label,		/* 47 */
    dst_typ_entry,		/* 48 */
    dst_typ_aux_lifetime,	/* 49 */
    dst_typ_aux_ptr_base,	/* 50 */
    dst_typ_aux_src_range,	/* 51 */
    dst_typ_aux_reg_val,	/* 52 */
    dst_typ_aux_unit_names,	/* 53 */
    dst_typ_aux_sect_info,	/* 54 */
    dst_typ_END_OF_ENUM
  }
dst_rec_type_t;


/*
   ** Dummy bounds for variably dimensioned arrays:
 */
#define dst_dummy_array_size  100


/*
   ** Reference to another item in the symbol table.
   **
   ** The value of a dst_rel_offset_t is the relative offset from the start of the
   ** referencing record to the start of the referenced record, string, etc. 
   **
   ** The value of a NIL dst_rel_offset_t is zero.
 */

typedef long dst_rel_offset_t ALIGNED1;


/* FIXME: Here and many places we make assumptions about sizes of host
   data types, structure layout, etc.  Only needs to be fixed if we care
   about cross-debugging, though.  */

/*
   ** Section-relative reference. 
   **
   ** The section index field is an index into the local compilation unit's
   ** section table (see dst_rec_section_tab_t)--NOT into the object module
   ** section table!
   **
   ** The sect_offset field is the offset in bytes into the section.
   **
   ** A NIL dst_sect_ref_t has a sect_index field of zero.  Indexes originate
   ** at one.
 */

typedef struct
  {
    unsigned short sect_index;
    unsigned long sect_offset ALIGNED1;
  }
dst_sect_ref_t;

#define dst_sect_index_nil    0
#define dst_sect_index_origin 1


/*
   ** Source location descriptor.
   **
   ** The file_index field is an index into the local compilation unit's
   ** file table (see dst_rec_file_tab_t).
   **
   ** A NIL dst_src_loc_t has a file_index field of zero.  Indexes originate
   ** at one.
 */

typedef struct
  {
    boolean reserved:1;		/* reserved for future use */
    int file_index:11;		/* index into .blocks source file list */
    int line_number:20;		/* source line number */
  }
dst_src_loc_t;

#define dst_file_index_nil    0
#define dst_file_index_origin 1


/*
   ** Standard (primitive) type codes.
 */

typedef enum
  {
    dst_non_std_type,
    dst_int8_type,		/* 8 bit integer */
    dst_int16_type,		/* 16 bit integer */
    dst_int32_type,		/* 32 bit integer */
    dst_uint8_type,		/* 8 bit unsigned integer */
    dst_uint16_type,		/* 16 bit unsigned integer */
    dst_uint32_type,		/* 32 bit unsigned integer */
    dst_real32_type,		/* single precision ieee floatining point */
    dst_real64_type,		/* double precision ieee floatining point */
    dst_complex_type,		/* single precision complex */
    dst_dcomplex_type,		/* double precision complex */
    dst_bool8_type,		/* boolean =logical*1 */
    dst_bool16_type,		/* boolean =logical*2 */
    dst_bool32_type,		/* boolean =logical*4 */
    dst_char_type,		/* 8 bit ascii character */
    dst_string_type,		/* string of 8 bit ascii characters */
    dst_ptr_type,		/* univ_pointer */
    dst_set_type,		/* generic 256 bit set */
    dst_proc_type,		/* generic procedure (signature not specified) */
    dst_func_type,		/* generic function (signature not specified) */
    dst_void_type,		/* c void type */
    dst_uchar_type,		/* c unsigned char */
    dst_std_type_END_OF_ENUM
  }
dst_std_type_t;


/*
   ** General data type descriptor
   **
   ** If the user_defined_type bit is clear, then the type is a standard type, and
   ** the remaining bits contain the dst_std_type_t of the type.  If the bit is
   ** set, then the type is defined in a separate dst record, which is referenced
   ** by the remaining bits as a dst_rel_offset_t.
 */

typedef union
  {
    struct
      {
	boolean user_defined_type:1;	/* tag field */
	int must_be_zero:23;	/* 23 bits of pad */
	dst_std_type_t dtc:8;	/* 8 bit primitive data */
      }
    std_type;

    struct
      {
	boolean user_defined_type:1;	/* tag field */
	int doffset:31;		/* offset to type record */
      }
    user_type;
  }
dst_type_t ALIGNED1;

/* The user_type.doffset field is a 31-bit signed value.  Some versions of C
   do not support signed bit fields.  The following macro will extract that
   field as a signed value:
 */
#define dst_user_type_offset(type_rec) \
    ( ((int) ((type_rec).user_type.doffset << 1)) >> 1 )


/*================================================*/
/*========== RECORDS IN .blocks SECTION ==========*/
/*================================================*/

/*-----------------------
  COMPILATION UNIT record 
  -----------------------
  This must be the first record in each .blocks section.
  Provides a set of information describing the output of a single compilation
  and pointers to additional information for the compilation unit.
*/

typedef enum
  {
    dst_pc_code_locs,		/* ranges in loc strings are pc ranges */
    dst_comp_unit_END_OF_ENUM
  }
dst_comp_unit_flag_t;

typedef enum
  {
    dst_lang_unk,		/* unknown language */
    dst_lang_pas,		/* Pascal */
    dst_lang_ftn,		/* FORTRAN */
    dst_lang_c,			/* C */
    dst_lang_mod2,		/* Modula-2 */
    dst_lang_asm_m68k,		/* 68K assembly language */
    dst_lang_asm_a88k,		/* AT assembly language */
    dst_lang_ada,		/* Ada */
    dst_lang_cxx,		/* C++ */
    dst_lang_END_OF_ENUM
  }
dst_lang_type_t;

typedef struct
  {
    struct
      {
	unsigned char major_part;	/* = dst_version_major */
	unsigned char minor_part;	/* = dst_version_minor */
      }
    version;			/* version of dst */
    unsigned short flags;	/* mask of dst_comp_unit_flag_t */
    unsigned short lang_type;	/* source language */
    unsigned short number_of_blocks;	/* number of blocks records */
    dst_rel_offset_t root_block_offset;		/* offset to root block (module?) */
    dst_rel_offset_t section_table /* offset to section table record */ ;
    dst_rel_offset_t file_table;	/* offset to file table record */
    unsigned long data_size;	/* total size of .blocks data */
  }
dst_rec_comp_unit_t ALIGNED1;


/*--------------------
  SECTION TABLE record
  --------------------
  There must be one section table associated with each compilation unit.
  Other debug records refer to sections via their index in this table.  The
  section base addresses in the table are virtual addresses of the sections,
  relocated by the linker.
*/

typedef struct
  {
    unsigned short number_of_sections;	/* size of array: */
    unsigned long section_base[dst_dummy_array_size] ALIGNED1;
  }
dst_rec_section_tab_t ALIGNED1;


/*-----------------
  FILE TABLE record
  -----------------
  There must be one file table associated with each compilation unit describing
  the source (and include) files used by each compilation unit.  Other debug 
  records refer to files via their index in this table.  The first entry is the
  primary source file.
*/

typedef struct
  {
    long dtm;			/* time last modified (time_$clock_t) */
    dst_rel_offset_t noffset;	/* offset to name string for source file */
  }
dst_file_desc_t;

typedef struct
  {
    unsigned short number_of_files;	/* size of array: */
    dst_file_desc_t files[dst_dummy_array_size] ALIGNED1;
  }
dst_rec_file_tab_t ALIGNED1;


/*-----------------
  NAME TABLE record
  -----------------
  A name table record may appear as an auxiliary record to the file table,
  providing additional qualification of the file indexes for languages that 
  need it (i.e. Ada).  Name table entries parallel file table entries of the
  same file index.
*/

typedef struct
  {
    unsigned short number_of_names;	/* size of array: */
    dst_rel_offset_t names[dst_dummy_array_size] ALIGNED1;
  }
dst_rec_name_tab_t ALIGNED1;


/*--------------
  BLOCK record
  --------------
  Describes a lexical program block--a procedure, function, module, etc.
*/

/* Block types.  These may be used in any way desired by the compiler writers. 
   The debugger uses them only to give a description to the user of the type of
   a block.  The debugger makes no other assumptions about the meaning of any
   of these.  For example, the fact that a block is executable (e.g., program)
   or not (e.g., module) is expressed in block attributes (see below), not
   guessed at from the block type.
 */
typedef enum
  {
    dst_block_module,		/* some pascal = modula = ada types */
    dst_block_program,
    dst_block_procedure,
    dst_block_function,		/* C function */
    dst_block_subroutine,	/* some fortran block types */
    dst_block_block_data,
    dst_block_stmt_function,
    dst_block_package,		/* a few particular to Ada */
    dst_block_package_body,
    dst_block_subunit,
    dst_block_task,
    dst_block_file,		/* a C outer scope? */
    dst_block_class,		/* C++ or Simula */
    dst_block_END_OF_ENUM
  }
dst_block_type_t;

/* Block attributes.  This is the information used by the debugger to represent
   the semantics of blocks.
 */
typedef enum
  {
    dst_block_main_entry,	/* the block's entry point is a main entry into
				   the compilation unit */
    dst_block_executable,	/* the block has an entry point */
    dst_block_attr_END_OF_ENUM
  }
dst_block_attr_t;

/* Code range.  Each block has associated with it one or more code ranges. An
   individual code range is identified by a range of source (possibly nil) and
   a range of executable code.  For example, a block which has its executable
   code spread over multiple sections will have one code range per section.
 */
typedef struct
  {
    unsigned long code_size;	/* size of executable code (in bytes ) */
    dst_sect_ref_t code_start;	/* starting address of executable code */
    dst_sect_ref_t lines_start;	/* start of line number tables */
  }
dst_code_range_t;

typedef struct
  {
    dst_block_type_t block_type:8;
    unsigned short flags:8;	/* mask of dst_block_attr_t flags */
    dst_rel_offset_t sibling_block_off;		/* offset to next sibling block */
    dst_rel_offset_t child_block_off;	/* offset to first contained block */
    dst_rel_offset_t noffset;	/* offset to block name string */
    dst_sect_ref_t symbols_start;	/* start of debug symbols  */
    unsigned short n_of_code_ranges;	/* size of array... */
    dst_code_range_t code_ranges[dst_dummy_array_size] ALIGNED1;
  }
dst_rec_block_t ALIGNED1;


/*--------------------------
  AUX SECT INFO TABLE record
  --------------------------
  Appears as an auxiliary to a block record.  Expands code range information
  by providing references into additional, language-dependent sections for 
  information related to specific code ranges of the block.  Sect info table
  entries parallel code range array entries of the same index.
*/

typedef struct
  {
    unsigned char tag;		/* currently can only be zero */
    unsigned char number_of_refs;	/* size of array: */
    dst_sect_ref_t refs[dst_dummy_array_size] ALIGNED1;
  }
dst_rec_sect_info_tab_t ALIGNED1;

/*=================================================*/
/*========== RECORDS IN .symbols SECTION ==========*/
/*=================================================*/

/*-----------------
  CONSTANT record
  -----------------
  Describes a symbolic constant.
*/

typedef struct
  {
    float r;			/* real part */
    float i;			/* imaginary part */
  }
dst_complex_t;

typedef struct
  {
    double dr;			/* real part */
    double di;			/* imaginary part */
  }
dst_double_complex_t;

/* The following record provides a way of describing constant values with 
   non-standard type and no limit on size. 
 */
typedef union
  {
    char char_data[dst_dummy_array_size];
    short int_data[dst_dummy_array_size];
    long long_data[dst_dummy_array_size];
  }
dst_big_kon_t;

/* Representation of the value of a general constant.
 */
typedef struct
  {
    unsigned short length;	/* size of constant value (bytes) */

    union
      {
	unsigned short kon_int8;
	short kon_int16;
	long kon_int32 ALIGNED1;
	float kon_real ALIGNED1;
	double kon_dbl ALIGNED1;
	dst_complex_t kon_cplx ALIGNED1;
	dst_double_complex_t kon_dcplx ALIGNED1;
	char kon_char;
	dst_big_kon_t kon ALIGNED1;
      }
    val;			/* value data of constant */
  }
dst_const_t ALIGNED1;

typedef struct
  {
    dst_rel_offset_t noffset;	/* offset to name string */
    dst_src_loc_t src_loc;	/* file/line of const definition */
    dst_type_t type_desc;	/* type of this (manifest) constant */
    dst_const_t value;
  }
dst_rec_const_t ALIGNED1;

/*----------------
  VARIABLE record
  ----------------
  Describes a program variable.
*/

/* Variable attributes.  These define certain variable semantics to the
   debugger.
 */
typedef enum
  {
    dst_var_attr_read_only,	/* is read-only (a program literal) */
    dst_var_attr_volatile,	/* same as compiler's VOLATILE attribute */
    dst_var_attr_global,	/* is a global definition or reference */
    dst_var_attr_compiler_gen,	/* is compiler-generated */
    dst_var_attr_static,	/* has static location */
    dst_var_attr_END_OF_ENUM
  }
dst_var_attr_t;

typedef struct
  {
    dst_rel_offset_t noffset;	/* offset to name string */
    dst_rel_offset_t loffset;	/* offset to loc string */
    dst_src_loc_t src_loc;	/* file/line of variable definition */
    dst_type_t type_desc;	/* type descriptor */
    unsigned short attributes;	/* mask of dst_var_attr_t flags */
  }
dst_rec_variable_t ALIGNED1;


/*----------------
  old VAR record
 -----------------
 Used by older compilers to describe a variable
*/

typedef enum
  {
    dst_var_loc_unknown,	/* Actually defined as "unknown" */
    dst_var_loc_abs,		/* Absolute address */
    dst_var_loc_sect_off,	/* Absolute address as a section offset */
    dst_var_loc_ind_sect_off,	/* An indexed section offset ???? */
    dst_var_loc_reg,		/* register */
    dst_var_loc_reg_rel,	/* register relative - usually fp */
    dst_var_loc_ind_reg_rel,	/* Indexed register relative */
    dst_var_loc_ftn_ptr_based,	/* Fortran pointer based */
    dst_var_loc_pc_rel,		/* PC relative. Really. */
    dst_var_loc_external,	/* External */
    dst_var_loc_END_OF_ENUM
  }
dst_var_loc_t;

/* Locations come in two versions. The short, and the long. The difference
 * between the short and the long is the addition of a statement number
 * field to the start andend of the range of the long, and and unkown
 * purpose field in the middle. Also, loc_type and loc_index aren't
 * bitfields in the long version.
 */

typedef struct
  {
    unsigned short loc_type:4;
    unsigned short loc_index:12;
    long location;
    short start_line;		/* start_line and end_line? */
    short end_line;		/* I'm guessing here.       */
  }
dst_var_loc_short_t;

typedef struct
  {
    unsigned short loc_type;
    unsigned short loc_index;
    long location;
    short unknown;		/* Always 0003 or 3b3c. Why? */
    short start_statement;
    short start_line;
    short end_statement;
    short end_line;
  }
dst_var_loc_long_t;


typedef struct
  {
    dst_rel_offset_t noffset;	/* offset to name string */
    dst_src_loc_t src_loc;	/* file/line of description */
    dst_type_t type_desc;	/* Type description */
    unsigned short attributes;	/* mask of dst_var_attr_t flags */
    unsigned short no_of_locs:15;	/* Number of locations */
    unsigned short short_locs:1;	/* True if short locations. */
    union
      {
	dst_var_loc_short_t shorts[dst_dummy_array_size];
	dst_var_loc_long_t longs[dst_dummy_array_size];
      }
    locs;
  }
dst_rec_var_t;

/*----------------
  old LABEL record
 -----------------
 Used by older compilers to describe a label
*/

typedef struct
  {
    dst_rel_offset_t noffset;	/* offset to name string */
    dst_src_loc_t src_loc;	/* file/line of description */
    char location[12];		/* location string */
  }
dst_rec_old_label_t ALIGNED1;

/*----------------
  POINTER record
  ----------------
  Describes a pointer type.
*/

typedef struct
  {
    dst_rel_offset_t noffset;	/* offset to the name string for this type */
    dst_src_loc_t src_loc;	/* file/line of definition */
    dst_type_t type_desc;	/* base type of this pointer */
  }
dst_rec_pointer_t ALIGNED1;


/*-------------
  ARRAY record
  -------------
  Describes an array type.

  Multidimensional arrays are described with a number of dst_rec_array_t 
  records, one per array dimension, each linked to the next through the
  elem_type_desc.doffset field.  Each record must have its multi_dim flag
  set.

  If column_major is true (as with FORTRAN arrays) then the last array bound in
  the declaration is the first array index in memory, which is the opposite of
  the usual case (as with Pascal and C arrays).

  Variable array bounds are described by auxiliary records; if aux_var_bound
  records are present, the lo_bound and hi_bound fields of this record are
  ignored by the debugger.

  span_comp identifies one of the language-dependent ways in which the distance
  between successive array elements (span) is calculated.  
     dst_use_span_field    -- the span is the value of span field.
     dst_compute_from_prev -- the span is the size of the previous dimension.
     dst_compute_from_next -- the span is the size of the next dimension.
  In the latter two cases, the span field contains an amount of padding to add
  to the size of the appropriate dimension to calculate the span.
*/

typedef enum
  {
    dst_use_span_field,
    dst_compute_from_prev,
    dst_compute_from_next,
    dst_span_comp_END_OF_ENUM
  }
dst_span_comp_t;

typedef struct
  {
    dst_rel_offset_t noffset;	/* offset to name string */
    dst_src_loc_t src_loc;	/* file/line of definition */
    dst_type_t elem_type_desc;	/* array element type */
    dst_type_t indx_type_desc;	/* array index type */
    long lo_bound;		/* lower bound of index */
    long hi_bound;		/* upper bound of index */
    unsigned long span;		/* see above */
    unsigned long size;		/* total array size (bytes) */
    boolean multi_dim:1;
    boolean is_packed:1;	/* true if packed array */
    boolean is_signed:1;	/* true if packed elements are signed */
    dst_span_comp_t span_comp:2;	/* how to compute span */
    boolean column_major:1;
    unsigned short reserved:2;	/* must be zero */
    unsigned short elem_size:8;	/* element size if packed (bits) */
  }
dst_rec_array_t ALIGNED1;


/*-----------------
  SUBRANGE record
  -----------------
  Describes a subrange type.
*/

/* Variable subrange bounds are described by auxiliary records; if aux_var_bound
   records are present, the lo_bound and hi_bound fields of this record are
   ignored by the debugger.
 */

typedef struct
  {
    dst_rel_offset_t noffset;	/* offset to name string */
    dst_src_loc_t src_loc;	/* file/line of subrange definition */
    dst_type_t type_desc;	/* parent type */
    long lo_bound;		/* lower bound of subrange */
    long hi_bound;		/* upper bound of subrange */
    unsigned short size;	/* storage size (bytes) */
  }
dst_rec_subrange_t ALIGNED1;


/*---------------
  STRING record 
  ---------------
  Describes a string type.
*/

/* Variable subrange bounds are described by auxiliary records; if aux_var_bound
   records are present, the lo_bound and hi_bound fields of this record are
   ignored by the debugger.
 */

typedef struct
  {
    dst_rel_offset_t noffset;	/* offset to name string */
    dst_src_loc_t src_loc;	/* file/line of string definition */
    dst_type_t elem_type_desc;	/* element type */
    dst_type_t indx_type_desc;	/* index type */
    long lo_bound;		/* lower bound */
    long hi_bound;		/* upper bound */
    unsigned long size;		/* total string size (bytes) if fixed */
  }
dst_rec_string_t ALIGNED1;


/*---------------
  SET record 
  ---------------
  Describes a set type.
*/

typedef struct
  {
    dst_rel_offset_t noffset;	/* offset to name string */
    dst_src_loc_t src_loc;	/* file/line of definition */
    dst_type_t type_desc;	/* element type */
    unsigned short nbits;	/* number of bits in set */
    unsigned short size;	/* storage size (bytes) */
  }
dst_rec_set_t ALIGNED1;


/*-----------------------------
  IMPLICIT ENUMERATION record 
  -----------------------------
  Describes an enumeration type with implicit element values = 0, 1, 2, ...
  (Pascal-style).
*/

typedef struct
  {
    dst_rel_offset_t noffset;	/* offset to name string */
    dst_src_loc_t src_loc;	/* file/line of definition */
    unsigned short nelems;	/* number of elements in enumeration */
    unsigned short size;	/* storage size (bytes) */
    /* offsets to name strings of elements 0, 1, 2, ... */
    dst_rel_offset_t elem_noffsets[dst_dummy_array_size];
  }
dst_rec_implicit_enum_t ALIGNED1;


/*-----------------------------
  EXPLICIT ENUMERATION record 
  -----------------------------
  Describes an enumeration type with explicitly assigned element values
  (C-style).
*/

typedef struct
  {
    dst_rel_offset_t noffset;	/* offset to element name string */
    long value;			/* element value */
  }
dst_enum_elem_t;

typedef struct
  {
    dst_rel_offset_t noffset;	/* offset to name string */
    dst_src_loc_t src_loc;	/* file/line of definition */
    unsigned short nelems;	/* number of elements in enumeration */
    unsigned short size;	/* storage size (bytes) */
    /* name/value pairs, one describing each enumeration value: */
    dst_enum_elem_t elems[dst_dummy_array_size];
  }
dst_rec_explicit_enum_t ALIGNED1;


/*-----------------------
  RECORD / UNION record 
  -----------------------
  Describes a record (struct) or union.

  If the record is larger than 2**16 bytes then an attached aux record
  specifies its size.  Also, if the record is stored in short form then
  attached records specify field offsets larger than 2**16 bytes.

  Whether the fields[] array or sfields[] array is used is selected by
  the dst_rec_type_t of the overall dst record.
*/

/*
   Record field descriptor, short form.  This form handles only fields which
   are an even number of bytes long, located some number of bytes from the
   start of the record.
 */
typedef struct
  {
    dst_rel_offset_t noffset;	/* offset to field name string */
    dst_type_t type_desc;	/* field type */
    unsigned short foffset;	/* field offset from start of record (bytes) */
  }
dst_short_field_t ALIGNED1;

typedef struct
  {
    dst_rel_offset_t noffset;	/* offset to name string */
    dst_type_t type_desc;	/* field type */
    unsigned short foffset;	/* byte offset */
    unsigned short is_packed:1;	/* True if field is packed */
    unsigned short bit_offset:6;	/* Bit offset */
    unsigned short size:6;	/* Size in bits */
    unsigned short sign:1;	/* True if signed */
    unsigned short pad:2;	/* Padding. Must be 0 */
  }
dst_old_field_t ALIGNED1;

/* Tag enumeration for long record field descriptor:
 */
typedef enum
  {
    dst_field_byte,
    dst_field_bit,
    dst_field_loc,
    dst_field_END_OF_ENUM
  }
dst_field_format_t;

/*
   Record field descriptor, long form.  The format of the field information
   is identified by the format_tag, which contains one of the above values.
   The field_byte variant is equivalent to the short form of field descriptor.
   The field_bit variant handles fields which are any number of bits long,
   located some number of bits from the start of the record.  The field_loc
   variant allows the location of the field to be described by a general loc
   string.
 */
typedef struct
  {
    dst_rel_offset_t noffset;	/* offset to name of field */
    dst_type_t type_desc;	/* type of field */
    union
      {
	struct
	  {
	    dst_field_format_t format_tag:2;	/* dst_field_byte */
	    unsigned long offset:30;	/* offset of field in bytes */
	  }
	field_byte ALIGNED1;
	struct
	  {
	    dst_field_format_t format_tag:2;	/* dst_field_bit */
	    unsigned long nbits:6;	/* bit size of field */
	    unsigned long is_signed:1;	/* signed/unsigned attribute */
	    unsigned long bit_offset:3;		/* bit offset from byte boundary */
	    int pad:4;		/* must be zero */
	    unsigned short byte_offset;		/* offset of byte boundary */
	  }
	field_bit ALIGNED1;
	struct
	  {
	    dst_field_format_t format_tag:2;	/* dst_field_loc */
	    int loffset:30;	/* dst_rel_offset_t to loc string */
	  }
	field_loc ALIGNED1;
      }
    f ALIGNED1;
  }
dst_field_t;

/* The field_loc.loffset field is a 30-bit signed value.  Some versions of C do
   not support signed bit fields.  The following macro will extract that field
   as a signed value:
 */
#define dst_field_loffset(field_rec) \
    ( ((int) ((field_rec).f.field_loc.loffset << 2)) >> 2 )


typedef struct
  {
    dst_rel_offset_t noffset;	/* offset to record name string */
    dst_src_loc_t src_loc;	/* file/line where this record is defined */
    unsigned short size;	/* storage size (bytes) */
    unsigned short nfields;	/* number of fields in this record */
    union
      {
	dst_field_t fields[dst_dummy_array_size];
	dst_short_field_t sfields[dst_dummy_array_size];
	dst_old_field_t ofields[dst_dummy_array_size];
      }
    f;				/* array of fields */
  }
dst_rec_record_t ALIGNED1;


/*-------------
  FILE record
  -------------
  Describes a file type.
*/

typedef struct
  {
    dst_rel_offset_t noffset;	/* offset to name string */
    dst_src_loc_t src_loc;	/* file/line where type was defined */
    dst_type_t type_desc;	/* file element type */
  }
dst_rec_file_t ALIGNED1;


/*---------------
  OFFSET record 
  ---------------
   Describes a Pascal offset type.
   (This type, an undocumented Domain Pascal extension, is currently not
   supported by the debugger)
*/

typedef struct
  {
    dst_rel_offset_t noffset;	/* offset to the name string */
    dst_src_loc_t src_loc;	/* file/line of definition */
    dst_type_t area_type_desc;	/* area type */
    dst_type_t base_type_desc;	/* base type */
    long lo_bound;		/* low bound of the offset range */
    long hi_bound;		/* high bound of the offset range */
    long bias;			/* bias */
    unsigned short scale;	/* scale factor */
    unsigned short size;	/* storage size (bytes) */
  }
dst_rec_offset_t ALIGNED1;


/*--------------
  ALIAS record 
  --------------
  Describes a type alias (e.g., typedef).
*/

typedef struct
  {
    dst_rel_offset_t noffset;	/* offset to name string */
    dst_src_loc_t src_loc;	/* file/line of definition */
    dst_type_t type_desc;	/* parent type */
  }
dst_rec_alias_t ALIGNED1;


/*------------------
  SIGNATURE record
  ------------------
  Describes a procedure/function type.
*/

/* Enumeration of argument semantics.  Note that most are mutually
   exclusive.
 */
typedef enum
  {
    dst_arg_attr_val,		/* passed by value */
    dst_arg_attr_ref,		/* passed by reference */
    dst_arg_attr_name,		/* passed by name */
    dst_arg_attr_in,		/* readable in the callee */
    dst_arg_attr_out,		/* writable in the callee */
    dst_arg_attr_hidden,	/* not visible in the caller */
    dst_arg_attr_END_OF_ENUM
  }
dst_arg_attr_t;

/* Argument descriptor.  Actually points to a variable record for most of the
   information.
 */
typedef struct
  {
    dst_rel_offset_t var_offset;	/* offset to variable record */
    unsigned short attributes;	/* a mask of dst_arg_attr_t flags */
  }
dst_arg_t ALIGNED1;

typedef struct
  {
    dst_rel_offset_t noffset;	/* offset to name string */
    dst_src_loc_t src_loc;	/* file/line of function definition */
    dst_rel_offset_t result;	/* offset to function result variable record */
    unsigned short nargs;	/* number of arguments */
    dst_arg_t args[dst_dummy_array_size];
  }
dst_rec_signature_t ALIGNED1;

/*--------------
  SCOPE record
  --------------
  Obsolete. Use the new ENTRY type instead.
  Old compilers may put this in as the first entry in a function,
  terminated by an end of scope entry.
*/

typedef struct
  {
    dst_rel_offset_t noffset;	/* Name offset */
    dst_src_loc_t start_line;	/* Starting line */
    dst_src_loc_t end_line;	/* Ending line */
  }
dst_rec_scope_t ALIGNED1;

/*--------------
  ENTRY record
  --------------
  Describes a procedure/function entry point.  An entry record is to a
  signature record roughly as a variable record is to a type descriptor record.

  The entry_number field is keyed to the entry numbers in .lines -- the 
  debugger locates the code location of an entry by searching the line
  number table for an entry numbered with the value of entry_number.  The
  main entry is numbered zero.
*/

typedef struct
  {
    dst_rel_offset_t noffset;	/* offset to entry name string */
    dst_rel_offset_t loffset;	/* where to jump to call this entry */
    dst_src_loc_t src_loc;	/* file/line of definition */
    dst_rel_offset_t sig_desc;	/* offset to signature descriptor */
    unsigned int entry_number:8;
    int pad:8;			/* must be zero */
  }
dst_rec_entry_t ALIGNED1;

/*-----------------------
  Old format ENTRY record
  -----------------------
  Supposedly obsolete but still used by some compilers.
 */

typedef struct
  {
    dst_rel_offset_t noffset;	/* Offset to entry name string */
    dst_src_loc_t src_loc;	/* Location in source */
    dst_rel_offset_t sig_desc;	/* Signature description */
    char unknown[36];
  }
dst_rec_old_entry_t ALIGNED1;

/*--------------
  LABEL record 
  --------------
  Describes a program label.
*/

typedef struct
  {
    dst_rel_offset_t noffset;	/* offset to label string */
    dst_rel_offset_t loffset;	/* offset to loc string */
    dst_src_loc_t src_loc;	/* file/line of definition */
  }
dst_rec_label_t ALIGNED1;


/*-----------------------
  AUXILIARY SIZE record
  -----------------------
  May appear in the auxiliary record list of any type or variable record to
  modify the default size of the type or variable.
*/

typedef struct
  {
    unsigned long size;		/* size (bytes) */
  }
dst_rec_aux_size_t ALIGNED1;


/*-----------------------
  AUXILIARY ALIGN record
  -----------------------
  May appear in the auxiliary record list of any type or variable record to
  modify the default alignment of the type or variable.
*/

typedef struct
  {
    unsigned short alignment;	/* # of low order zero bits */
  }
dst_rec_aux_align_t ALIGNED1;


/*-----------------------------
  AUXILIARY FIELD SIZE record
  -----------------------------
  May appear in the auxiliary record list of any RECORD/UNION record to 
  modify the default size of a field.
*/

typedef struct
  {
    unsigned short field_no;	/* field number */
    unsigned long size;		/* size (bits) */
  }
dst_rec_aux_field_size_t ALIGNED1;



/*-----------------------------
  AUXILIARY FIELD OFFSET record
  -----------------------------
  May appear in the auxiliary record list of any RECORD/UNION record to 
  specify a field offset larger than 2**16.
*/

typedef struct
  {
    unsigned short field_no;	/* field number */
    unsigned long foffset;	/* offset */
  }
dst_rec_aux_field_off_t ALIGNED1;


/*-----------------------------
  AUXILIARY FIELD ALIGN record
  -----------------------------
  May appear in the auxiliary record list of any RECORD/UNION record to 
  modify the default alignment of a field.
*/

typedef struct
  {
    unsigned short field_no;	/* field number */
    unsigned short alignment;	/* number of low order zero bits */
  }
dst_rec_aux_field_align_t ALIGNED1;


/*----------------------------
  AUXILIARY VAR BOUND record
  ----------------------------
  May appear in the auxiliary record list of any ARRAY, SUBRANGE or STRING
  record to describe a variable bound for the range of the type.
*/

typedef enum
  {
    dst_low_bound,		/* the low bound is variable */
    dst_high_bound,		/* the high bound is variable */
    dst_var_bound_END_OF_ENUM
  }
dst_var_bound_t;

typedef struct
  {
    unsigned short which;	/* which bound */
    dst_rel_offset_t voffset ALIGNED1;	/* variable that defines bound */
  }
dst_rec_aux_var_bound_t ALIGNED1;


/*----------------------------------
  AUXILIARY TYPE DERIVATION record 
  ----------------------------------
  May appear in the auxiliary record list of any RECORD/UNION record to denote
  class inheritance of that type from a parent type.

  Inheritance implies that it is possible to convert the inheritor type to the
  inherited type, retaining those fields which were inherited.  To allow this,
  orig_field_no, a field number into the record type, is provided.  If 
  orig_is_pointer is false, then the start of the inherited record is located
  at the location of the field indexed by orig_field_no.  If orig_is_pointer
  is true, then it is located at the address contained in the field indexed
  by orig_field_no (assumed to be a pointer).
*/

typedef struct
  {
    dst_type_t parent_type;	/* reference to inherited type */
    unsigned short orig_field_no;
    boolean orig_is_pointer:1;
    int unused:15;		/* must be zero */
  }
dst_rec_aux_type_deriv_t ALIGNED1;


/*------------------------------------
  AUXILIARY VARIABLE LIFETIME record
  ------------------------------------
  May appear in the auxiliary record list of a VARIABLE record to add location
  information for an additional variable lifetime.
*/

typedef struct
  {
    dst_rel_offset_t loffset;
  }
dst_rec_aux_lifetime_t ALIGNED1;


/*-------------------------------
  AUXILIARY POINTER BASE record 
  -------------------------------
  May appear in the auxiliary record list of a VARIABLE record to provide a
  pointer base to substitute for references to any such bases in the location
  string of the variable.  A pointer base is another VARIABLE record.  When
  the variable is evaluated by the debugger, it uses the current value of the
  pointer base variable in computing its location.

  This is useful for representing FORTRAN pointer-based variables.
*/

typedef struct
  {
    dst_rel_offset_t voffset;
  }
dst_rec_aux_ptr_base_t ALIGNED1;


/*---------------------------------
  AUXILIARY REGISTER VALUE record 
  ---------------------------------
  May appear in the auxiliary record list of an ENTRY record to specify
  a register that must be set to a specific value before jumping to the entry
  point in a debugger "call".  The debugger must set the debuggee register,
  specified by the register code, to the value of the *address* to which the
  location string resolves.  If the address is register-relative, then the
  call cannot be made unless the current stack frame is the lexical parent
  of the entry.  An example of this is when a (Pascal) nested procedure
  contains references to its parent's variables, which it accesses through
  a static link register.  The static link register must be set to some
  address relative to the parent's stack base register.
*/

typedef struct
  {
    unsigned short reg;		/* identifies register to set (isp enum) */
    dst_rel_offset_t loffset;	/* references a location string */
  }
dst_rec_aux_reg_val_t ALIGNED1;


/*==========================================================*/
/*========== RECORDS USED IN .blocks AND .symbols ==========*/
/*==========================================================*/

/*---------------------
  STRING TABLE record
  ---------------------
  A string table record contains any number of null-terminated, variable length
  strings.   The length field gives the size in bytes of the text field, which
  can be any size.

  The global name table shares this format.  This record appears in the
  .blocks section.  Each string in the table identifies a global defined in
  the current compilation unit.

  The loc pool record shares this format as well.  Loc strings are described
  elsewhere.
*/

typedef struct
  {
    unsigned long length;
    char text[dst_dummy_array_size];
  }
dst_rec_string_tab_t ALIGNED1;


/*-----------------------
  AUXILIARY QUAL record 
  -----------------------
  May appear in the auxiliary record list of any BLOCK, VARIABLE, or type record
  to provide it with a fully-qualified, language-dependent name.
*/

typedef struct
  {
    dst_rel_offset_t lang_qual_name;
  }
dst_rec_aux_qual_t ALIGNED1;


/*----------------
  FORWARD record
  ----------------
  Reference to a record somewhere else.  This allows identical definitions in
  different scopes to share data.
*/

typedef struct
  {
    dst_rel_offset_t rec_off;
  }
dst_rec_forward_t ALIGNED1;


/*-------------------------------
  AUXILIARY SOURCE RANGE record
  -------------------------------
  May appear in the auxiliary record list of any BLOCK record to specify a
  range of source lines over which the block is active.
*/

typedef struct
  {
    dst_src_loc_t first_line;	/* first source line */
    dst_src_loc_t last_line;	/* last source line */
  }
dst_rec_aux_src_range_t ALIGNED1;


/*------------------
  EXTENSION record 
  ------------------
  Provision for "foreign" records, such as might be generated by a non-Apollo
  compiler.  Apollo software will ignore these.
*/

typedef struct
  {
    unsigned short rec_size;	/* record size (bytes) */
    unsigned short ext_type;	/* defined by whoever generates it */
    unsigned short ext_data;	/* place-holder for arbitrary amount of data */
  }
dst_rec_extension_t ALIGNED1;


/*
   ** DEBUG SYMBOL record -- The wrapper for all .blocks and .symbols records.
   **
   ** This record ties together all previous .blocks and .symbols records 
   ** together in a union with a common header.  The rec_type field of the
   ** header identifies the record type.  The rec_flags field currently only
   ** defines auxiliary record lists. 
   **
   ** If a record carries with it a non-null auxiliary record list, its
   ** dst_flag_has_aux_recs flag is set, and each of the records that follow
   ** it are treated as its auxiliary records, until the end of the compilation
   ** unit or scope is reached, or until an auxiliary record with its
   ** dst_flag_last_aux_rec flag set is reached.
 */

typedef enum
  {
    dst_flag_has_aux_recs,
    dst_flag_last_aux_rec,
    dst_rec_flag_END_OF_ENUM
  }
dst_rec_flags_t;

typedef struct
  {
    dst_rec_type_t rec_type:8;	/* record type */
    int rec_flags:8;		/* mask of dst_rec_flags_t */
    union			/* switched on rec_type field above */
      {
	/* dst_typ_pad requires no additional fields */
	dst_rec_comp_unit_t comp_unit_;
	dst_rec_section_tab_t section_tab_;
	dst_rec_file_tab_t file_tab_;
	dst_rec_block_t block_;
	dst_rec_var_t var_;
	dst_rec_pointer_t pointer_;
	dst_rec_array_t array_;
	dst_rec_subrange_t subrange_;
	dst_rec_set_t set_;
	dst_rec_implicit_enum_t implicit_enum_;
	dst_rec_explicit_enum_t explicit_enum_;
	/* dst_typ_short_{rec,union} are represented by 'rec' (below) */
	dst_rec_file_t file_;
	dst_rec_offset_t offset_;
	dst_rec_alias_t alias_;
	dst_rec_signature_t signature_;
	dst_rec_old_label_t old_label_;
	dst_rec_scope_t scope_;
	/* dst_typ_end_scope requires no additional fields */
	dst_rec_string_tab_t string_tab_;
	/* dst_typ_global_name_tab is represented by 'string_tab' (above) */
	dst_rec_forward_t forward_;
	dst_rec_aux_size_t aux_size_;
	dst_rec_aux_align_t aux_align_;
	dst_rec_aux_field_size_t aux_field_size_;
	dst_rec_aux_field_off_t aux_field_off_;
	dst_rec_aux_field_align_t aux_field_align_;
	dst_rec_aux_qual_t aux_qual_;
	dst_rec_aux_var_bound_t aux_var_bound_;
	dst_rec_extension_t extension_;
	dst_rec_string_t string_;
	dst_rec_const_t const_;
	/* dst_typ_reference is represented by 'pointer' (above) */
	dst_rec_record_t record_;
	/* dst_typ_union is represented by 'record' (above) */
	dst_rec_aux_type_deriv_t aux_type_deriv_;
	/* dst_typ_locpool is represented by 'string_tab' (above) */
	dst_rec_variable_t variable_;
	dst_rec_label_t label_;
	dst_rec_entry_t entry_;
	dst_rec_aux_lifetime_t aux_lifetime_;
	dst_rec_aux_ptr_base_t aux_ptr_base_;
	dst_rec_aux_src_range_t aux_src_range_;
	dst_rec_aux_reg_val_t aux_reg_val_;
	dst_rec_name_tab_t aux_unit_names_;
	dst_rec_sect_info_tab_t aux_sect_info_;
      }
    rec_data ALIGNED1;
  }
dst_rec_t, *dst_rec_ptr_t;


/*===============================================*/
/*========== .lines SECTION DEFINITIONS =========*/
/*===============================================*/
/*
   The .lines section contains a sequence of line number tables.  There is no
   record structure within the section.  The start of the table for a routine
   is pointed to by the block record, and the end of the table is signaled by
   an escape code.

   A line number table is a sequence of bytes.  The default entry contains a line
   number delta (-7..+7) in the high 4 bits and a pc delta (0..15) in the low 4 
   bits. Special cases, including when one or both of the values is too large
   to fit in 4 bits and other special cases are handled through escape entries.
   Escape entries are identified by the value 0x8 in the high 4 bits.  The low 4
   bits are occupied by a function code.  Some escape entries are followed by
   additional arguments, which may be bytes, words, or longwords.  This data is
   not aligned. 

   The initial PC offset, file number and line number are zero.  Normally, the
   table begins with a dst_ln_file escape which establishes the initial file
   and line number.  All PC deltas are unsigned (thus the table is ordered by
   increasing PC); line number deltas are signed.  The table ends with a 
   dst_ln_end escape, which is followed by a final table entry whose PC delta
   gives the code size of the last statement.

   Escape     Semantic
   ---------  ------------------------------------------------------------
   file       Changes file state.  The current source file remains constant
   until another file escape.  Though the line number state is
   also updated by a file escape, a file escape does NOT 
   constitute a line table entry.

   statement  Alters the statement number of the next table entry.  By 
   default, all table entries refer to the first statement on a
   line.  Statement number one is the second statement, and so on.

   entry      Identifies the next table entry as the position of an entry 
   point for the current block.  The PC position should follow 
   any procedure prologue code.  An argument specifies the entry
   number, which is keyed to the entry number of the corresponding
   .symbols ENTRY record.

   exit       Identifies the next table entry as the last position within 
   the current block before a procedure epiloge and subsequent
   procedure exit.

   gap        By default, the executable code corresponding to a table entry 
   is assumed to extend to the beginning of the next table entry.
   If this is not the case--there is a "hole" in the table--then
   a gap escape should follow the first table entry to specify
   where the code for that entry ends.
 */

#define dst_ln_escape_flag    -8

/*
   Escape function codes:
 */
typedef enum
  {
    dst_ln_pad,			/* pad byte */
    dst_ln_file,		/* file escape.  Next 4 bytes are a dst_src_loc_t */
    dst_ln_dln1_dpc1,		/* 1 byte line delta, 1 byte pc delta */
    dst_ln_dln2_dpc2,		/* 2 bytes line delta, 2 bytes pc delta */
    dst_ln_ln4_pc4,		/* 4 bytes ABSOLUTE line number, 4 bytes ABSOLUTE pc */
    dst_ln_dln1_dpc0,		/* 1 byte line delta, pc delta = 0 */
    dst_ln_ln_off_1,		/* statement escape, stmt # = 1 (2nd stmt on line) */
    dst_ln_ln_off,		/* statement escape, stmt # = next byte */
    dst_ln_entry,		/* entry escape, next byte is entry number */
    dst_ln_exit,		/* exit escape */
    dst_ln_stmt_end,		/* gap escape, 4 bytes pc delta */
    dst_ln_escape_11,		/* reserved */
    dst_ln_escape_12,		/* reserved */
    dst_ln_escape_13,		/* reserved */
    dst_ln_nxt_byte,		/* next byte contains the real escape code */
    dst_ln_end,			/* end escape, final entry follows */
    dst_ln_escape_END_OF_ENUM
  }
dst_ln_escape_t;

/*
   Line number table entry
 */
typedef union
  {
    struct
      {
	unsigned int ln_delta:4;	/* 4 bit line number delta */
	unsigned int pc_delta:4;	/* 4 bit pc delta */
      }
    delta;

    struct
      {
	unsigned int esc_flag:4;	/* alias for ln_delta */
	dst_ln_escape_t esc_code:4;	/* escape function code */
      }
    esc;

    char sdata;			/* signed data byte */
    unsigned char udata;	/* unsigned data byte */
  }
dst_ln_entry_t,
 *dst_ln_entry_ptr_t,
  dst_ln_table_t[dst_dummy_array_size];

/* The following macro will extract the ln_delta field as a signed value:
 */
#define dst_ln_ln_delta(ln_rec) \
    ( ((short) ((ln_rec).delta.ln_delta << 12)) >> 12 )




typedef struct dst_sec_struct
  {
    char *buffer;
    long position;
    long size;
    long base;
  }
dst_sec;


/* Macros for access to the data */

#define DST_comp_unit(x) 	((x)->rec_data.comp_unit_)
#define DST_section_tab(x) 	((x)->rec_data.section_tab_)
#define DST_file_tab(x) 	((x)->rec_data.file_tab_)
#define DST_block(x) 		((x)->rec_data.block_)
#define	DST_var(x)		((x)->rec_data.var_)
#define DST_pointer(x) 		((x)->rec_data.pointer_)
#define DST_array(x) 		((x)->rec_data.array_)
#define DST_subrange(x) 	((x)->rec_data.subrange_)
#define DST_set(x)	 	((x)->rec_data.set_)
#define DST_implicit_enum(x) 	((x)->rec_data.implicit_enum_)
#define DST_explicit_enum(x) 	((x)->rec_data.explicit_enum_)
#define DST_short_rec(x) 	((x)->rec_data.record_)
#define DST_short_union(x) 	((x)->rec_data.record_)
#define DST_file(x) 		((x)->rec_data.file_)
#define DST_offset(x) 		((x)->rec_data.offset_)
#define DST_alias(x)	 	((x)->rec_data.alias_)
#define DST_signature(x) 	((x)->rec_data.signature_)
#define DST_old_label(x) 	((x)->rec_data.old_label_)
#define DST_scope(x) 		((x)->rec_data.scope_)
#define DST_string_tab(x) 	((x)->rec_data.string_tab_)
#define DST_global_name_tab(x) 	((x)->rec_data.string_tab_)
#define DST_forward(x) 		((x)->rec_data.forward_)
#define DST_aux_size(x) 	((x)->rec_data.aux_size_)
#define DST_aux_align(x) 	((x)->rec_data.aux_align_)
#define DST_aux_field_size(x) 	((x)->rec_data.aux_field_size_)
#define DST_aux_field_off(x) 	((x)->rec_data.aux_field_off_)
#define DST_aux_field_align(x) 	((x)->rec_data.aux_field_align_)
#define DST_aux_qual(x) 	((x)->rec_data.aux_qual_)
#define DST_aux_var_bound(x) 	((x)->rec_data.aux_var_bound_)
#define DST_extension(x) 	((x)->rec_data.extension_)
#define DST_string(x) 		((x)->rec_data.string_)
#define DST_const(x) 		((x)->rec_data.const_)
#define DST_reference(x) 	((x)->rec_data.pointer_)
#define DST_record(x) 		((x)->rec_data.record_)
#define DST_union(x) 		((x)->rec_data.record_)
#define DST_aux_type_deriv(x) 	((x)->rec_data.aux_type_deriv_)
#define DST_locpool(x) 		((x)->rec_data.string_tab_)
#define DST_variable(x) 	((x)->rec_data.variable_)
#define DST_label(x) 		((x)->rec_data.label_)
#define DST_entry(x) 		((x)->rec_data.entry_)
#define DST_aux_lifetime(x) 	((x)->rec_data.aux_lifetime_)
#define DST_aux_ptr_base(x) 	((x)->rec_data.aux_ptr_base_)
#define DST_aux_src_range(x) 	((x)->rec_data.aux_src_range_)
#define DST_aux_reg_val(x) 	((x)->rec_data.aux_reg_val_)
#define DST_aux_unit_names(x) 	((x)->rec_data.aux_unit_names_)
#define DST_aux_sect_info(x) 	((x)->rec_data.aux_sect_info_)


/*
 * Type codes for loc strings. I'm not entirely certain about all of
 * these, but they seem to work.
 *                              troy@cbme.unsw.EDU.AU
 * If you find a variable whose location can't be decoded, you should
 * find out it's code using "dstdump -s filename". It will record an
 * entry for the variable, and give a text representation of what
 * the locstring means. Before that explaination there will be a
 * number. In the LOCSTRING table, that number will appear before
 * the start of the location string. Location string codes are
 * five bit codes with a 3 bit argument. Check the high 5 bits of
 * the one byte code, and figure out where it goes in here.
 * Then figure out exactly what the meaning is and code it in
 * dstread.c
 *
 * Note that ranged locs mean that the variable is in different locations
 * depending on the current PC. We ignore these because (a) gcc can't handle
 * them, and (b), If you don't use high levels of optimisation they won't
 * occur.
 */
typedef enum
  {
    dst_lsc_end,		/* End of string */
    dst_lsc_indirect,		/* Indirect through previous. Arg == 6 */
    /* Or register ax (x=arg) */
    dst_lsc_dreg,		/* register dx (x=arg) */
    dst_lsc_03,
    dst_lsc_section,		/* Section (arg+1) */
    dst_lsc_05,
    dst_lsc_06,
    dst_lsc_add,		/* Add (arg+1)*2 */
    dst_lsc_sub,		/* Subtract (arg+1)*2 */
    dst_lsc_09,
    dst_lsc_0a,
    dst_lsc_sec_byte,		/* Section of next byte+1 */
    dst_lsc_add_byte,		/* Add next byte (arg == 5) or next word
				 * (arg == 6)
				 */
    dst_lsc_sub_byte,		/* Subtract next byte. (arg == 1) or next
				 * word (arg == 6 ?)
				 */
    dst_lsc_sbreg,		/* Stack base register (frame pointer). Arg==0 */
    dst_lsc_0f,
    dst_lsc_ranged,		/* location is pc dependent */
    dst_lsc_11,
    dst_lsc_12,
    dst_lsc_13,
    dst_lsc_14,
    dst_lsc_15,
    dst_lsc_16,
    dst_lsc_17,
    dst_lsc_18,
    dst_lsc_19,
    dst_lsc_1a,
    dst_lsc_1b,
    dst_lsc_1c,
    dst_lsc_1d,
    dst_lsc_1e,
    dst_lsc_1f
  }
dst_loc_string_code_t;

/* If the following occurs after an addition/subtraction, that addition
 * or subtraction should be multiplied by 256. It's a complete byte, not
 * a code.
 */

#define	dst_multiply_256	((char) 0x73)

typedef struct
  {
    char code:5;
    char arg:3;
  }
dst_loc_header_t ALIGNED1;

typedef union
  {
    dst_loc_header_t header;
    char data;
  }
dst_loc_entry_t ALIGNED1;

#undef ALIGNED1
#endif /* apollo_dst_h */

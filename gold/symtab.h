// symtab.h -- the gold symbol table   -*- C++ -*-

// Symbol_table
//   The symbol table.

#include <string>
#include <utility>
#include <vector>

#include "elfcpp.h"
#include "stringpool.h"
#include "object.h"

#ifndef GOLD_SYMTAB_H
#define GOLD_SYMTAB_H

namespace gold
{

class Object;
class Relobj;
template<int size, bool big_endian>
class Sized_relobj;
class Dynobj;
template<int size, bool big_endian>
class Sized_dynobj;
class Versions;
class Output_data;
class Output_section;
class Output_segment;
class Output_file;
class Target;

// The base class of an entry in the symbol table.  The symbol table
// can have a lot of entries, so we don't want this class to big.
// Size dependent fields can be found in the template class
// Sized_symbol.  Targets may support their own derived classes.

class Symbol
{
 public:
  // Because we want the class to be small, we don't use any virtual
  // functions.  But because symbols can be defined in different
  // places, we need to classify them.  This enum is the different
  // sources of symbols we support.
  enum Source
  {
    // Symbol defined in a relocatable or dynamic input file--this is
    // the most common case.
    FROM_OBJECT,
    // Symbol defined in an Output_data, a special section created by
    // the target.
    IN_OUTPUT_DATA,
    // Symbol defined in an Output_segment, with no associated
    // section.
    IN_OUTPUT_SEGMENT,
    // Symbol value is constant.
    CONSTANT
  };

  // When the source is IN_OUTPUT_SEGMENT, we need to describe what
  // the offset means.
  enum Segment_offset_base
  {
    // From the start of the segment.
    SEGMENT_START,
    // From the end of the segment.
    SEGMENT_END,
    // From the filesz of the segment--i.e., after the loaded bytes
    // but before the bytes which are allocated but zeroed.
    SEGMENT_BSS
  };

  // Return the symbol name.
  const char*
  name() const
  { return this->name_; }

  // Return the symbol version.  This will return NULL for an
  // unversioned symbol.
  const char*
  version() const
  { return this->version_; }

  // Return the symbol source.
  Source
  source() const
  { return this->source_; }

  // Return the object with which this symbol is associated.
  Object*
  object() const
  {
    gold_assert(this->source_ == FROM_OBJECT);
    return this->u_.from_object.object;
  }

  // Return the index of the section in the input relocatable or
  // dynamic object file.
  unsigned int
  shndx() const
  {
    gold_assert(this->source_ == FROM_OBJECT);
    return this->u_.from_object.shndx;
  }

  // Return the output data section with which this symbol is
  // associated, if the symbol was specially defined with respect to
  // an output data section.
  Output_data*
  output_data() const
  {
    gold_assert(this->source_ == IN_OUTPUT_DATA);
    return this->u_.in_output_data.output_data;
  }

  // If this symbol was defined with respect to an output data
  // section, return whether the value is an offset from end.
  bool
  offset_is_from_end() const
  {
    gold_assert(this->source_ == IN_OUTPUT_DATA);
    return this->u_.in_output_data.offset_is_from_end;
  }

  // Return the output segment with which this symbol is associated,
  // if the symbol was specially defined with respect to an output
  // segment.
  Output_segment*
  output_segment() const
  {
    gold_assert(this->source_ == IN_OUTPUT_SEGMENT);
    return this->u_.in_output_segment.output_segment;
  }

  // If this symbol was defined with respect to an output segment,
  // return the offset base.
  Segment_offset_base
  offset_base() const
  {
    gold_assert(this->source_ == IN_OUTPUT_SEGMENT);
    return this->u_.in_output_segment.offset_base;
  }

  // Return the symbol binding.
  elfcpp::STB
  binding() const
  { return this->binding_; }

  // Return the symbol type.
  elfcpp::STT
  type() const
  { return this->type_; }

  // Return the symbol visibility.
  elfcpp::STV
  visibility() const
  { return this->visibility_; }

  // Return the non-visibility part of the st_other field.
  unsigned char
  nonvis() const
  { return this->nonvis_; }

  // Return whether this symbol is a forwarder.  This will never be
  // true of a symbol found in the hash table, but may be true of
  // symbol pointers attached to object files.
  bool
  is_forwarder() const
  { return this->is_forwarder_; }

  // Mark this symbol as a forwarder.
  void
  set_forwarder()
  { this->is_forwarder_ = true; }

  // Return whether this symbol needs an entry in the dynamic symbol
  // table.
  bool
  needs_dynsym_entry() const
  { return this->needs_dynsym_entry_; }

  // Mark this symbol as needing an entry in the dynamic symbol table.
  void
  set_needs_dynsym_entry()
  { this->needs_dynsym_entry_ = true; }

  // Return whether this symbol has been seen in a regular object.
  bool
  in_reg() const
  { return this->in_reg_; }

  // Mark this symbol as having been seen in a regular object.
  void
  set_in_reg()
  { this->in_reg_ = true; }

  // Mark this symbol as having been seen in a dynamic object.
  void
  set_in_dyn()
  { this->in_dyn_ = true; }

  // Return the index of this symbol in the output file symbol table.
  // A value of -1U means that this symbol is not going into the
  // output file.  This starts out as zero, and is set to a non-zero
  // value by Symbol_table::finalize.  It is an error to ask for the
  // symbol table index before it has been set.
  unsigned int
  symtab_index() const
  {
    gold_assert(this->symtab_index_ != 0);
    return this->symtab_index_;
  }

  // Set the index of the symbol in the output file symbol table.
  void
  set_symtab_index(unsigned int index)
  {
    gold_assert(index != 0);
    this->symtab_index_ = index;
  }

  // Return whether this symbol already has an index in the output
  // file symbol table.
  bool
  has_symtab_index() const
  { return this->symtab_index_ != 0; }

  // Return the index of this symbol in the dynamic symbol table.  A
  // value of -1U means that this symbol is not going into the dynamic
  // symbol table.  This starts out as zero, and is set to a non-zero
  // during Layout::finalize.  It is an error to ask for the dynamic
  // symbol table index before it has been set.
  unsigned int
  dynsym_index() const
  {
    gold_assert(this->dynsym_index_ != 0);
    return this->dynsym_index_;
  }

  // Set the index of the symbol in the dynamic symbol table.
  void
  set_dynsym_index(unsigned int index)
  {
    gold_assert(index != 0);
    this->dynsym_index_ = index;
  }

  // Return whether this symbol already has an index in the dynamic
  // symbol table.
  bool
  has_dynsym_index() const
  { return this->dynsym_index_ != 0; }

  // Return whether this symbol has an entry in the GOT section.
  bool
  has_got_offset() const
  { return this->has_got_offset_; }

  // Return the offset into the GOT section of this symbol.
  unsigned int
  got_offset() const
  {
    gold_assert(this->has_got_offset());
    return this->got_offset_;
  }

  // Set the GOT offset of this symbol.
  void
  set_got_offset(unsigned int got_offset)
  {
    this->has_got_offset_ = true;
    this->got_offset_ = got_offset;
  }

  // Return whether this symbol has an entry in the PLT section.
  bool
  has_plt_offset() const
  { return this->has_plt_offset_; }

  // Return the offset into the PLT section of this symbol.
  unsigned int
  plt_offset() const
  {
    gold_assert(this->has_plt_offset());
    return this->plt_offset_;
  }

  // Set the PLT offset of this symbol.
  void
  set_plt_offset(unsigned int plt_offset)
  {
    this->has_plt_offset_ = true;
    this->plt_offset_ = plt_offset;
  }

  // Return true if the final value of this symbol is known at link
  // time.
  bool
  final_value_is_known(const General_options* options) const
  {
    if (options->is_shared())
      return false;
    return this->source_ != FROM_OBJECT || !this->object()->is_dynamic();
  }

  // Return whether this is a defined symbol (not undefined or
  // common).
  bool
  is_defined() const
  {
    return (this->source_ != FROM_OBJECT
	    || (this->shndx() != elfcpp::SHN_UNDEF
		&& this->shndx() != elfcpp::SHN_COMMON));
  }

  // Return true if this symbol is from a dynamic object.
  bool
  is_from_dynobj() const
  {
    return this->source_ == FROM_OBJECT && this->object()->is_dynamic();
  }

  // Return whether this is an undefined symbol.
  bool
  is_undefined() const
  {
    return this->source_ == FROM_OBJECT && this->shndx() == elfcpp::SHN_UNDEF;
  }

  // Return whether this is a common symbol.
  bool
  is_common() const
  {
    return (this->source_ == FROM_OBJECT
	    && (this->shndx() == elfcpp::SHN_COMMON
		|| this->type_ == elfcpp::STT_COMMON));
  }

  // Return whether there should be a warning for references to this
  // symbol.
  bool
  has_warning() const
  { return this->has_warning_; }

  // Mark this symbol as having a warning.
  void
  set_has_warning()
  { this->has_warning_ = true; }

 protected:
  // Instances of this class should always be created at a specific
  // size.
  Symbol()
  { memset(this, 0, sizeof *this); }

  // Initialize the general fields.
  void
  init_fields(const char* name, const char* version,
	      elfcpp::STT type, elfcpp::STB binding,
	      elfcpp::STV visibility, unsigned char nonvis);

  // Initialize fields from an ELF symbol in OBJECT.
  template<int size, bool big_endian>
  void
  init_base(const char *name, const char* version, Object* object,
	    const elfcpp::Sym<size, big_endian>&);

  // Initialize fields for an Output_data.
  void
  init_base(const char* name, Output_data*, elfcpp::STT, elfcpp::STB,
	    elfcpp::STV, unsigned char nonvis, bool offset_is_from_end);

  // Initialize fields for an Output_segment.
  void
  init_base(const char* name, Output_segment* os, elfcpp::STT type,
	    elfcpp::STB binding, elfcpp::STV visibility,
	    unsigned char nonvis, Segment_offset_base offset_base);

  // Initialize fields for a constant.
  void
  init_base(const char* name, elfcpp::STT type, elfcpp::STB binding,
	    elfcpp::STV visibility, unsigned char nonvis);

  // Override existing symbol.
  template<int size, bool big_endian>
  void
  override_base(const elfcpp::Sym<size, big_endian>&, Object* object,
		const char* version);

 private:
  Symbol(const Symbol&);
  Symbol& operator=(const Symbol&);

  // Symbol name (expected to point into a Stringpool).
  const char* name_;
  // Symbol version (expected to point into a Stringpool).  This may
  // be NULL.
  const char* version_;

  union
  {
    // This struct is used if SOURCE_ == FROM_OBJECT.
    struct
    {
      // Object in which symbol is defined, or in which it was first
      // seen.
      Object* object;
      // Section number in object_ in which symbol is defined.
      unsigned int shndx;
    } from_object;

    // This struct is used if SOURCE_ == IN_OUTPUT_DATA.
    struct
    {
      // Output_data in which symbol is defined.  Before
      // Layout::finalize the symbol's value is an offset within the
      // Output_data.
      Output_data* output_data;
      // True if the offset is from the end, false if the offset is
      // from the beginning.
      bool offset_is_from_end;
    } in_output_data;

    // This struct is used if SOURCE_ == IN_OUTPUT_SEGMENT.
    struct
    {
      // Output_segment in which the symbol is defined.  Before
      // Layout::finalize the symbol's value is an offset.
      Output_segment* output_segment;
      // The base to use for the offset before Layout::finalize.
      Segment_offset_base offset_base;
    } in_output_segment;
  } u_;

  // The index of this symbol in the output file.  If the symbol is
  // not going into the output file, this value is -1U.  This field
  // starts as always holding zero.  It is set to a non-zero value by
  // Symbol_table::finalize.
  unsigned int symtab_index_;

  // The index of this symbol in the dynamic symbol table.  If the
  // symbol is not going into the dynamic symbol table, this value is
  // -1U.  This field starts as always holding zero.  It is set to a
  // non-zero value during Layout::finalize.
  unsigned int dynsym_index_;

  // If this symbol has an entry in the GOT section (has_got_offset_
  // is true), this is the offset from the start of the GOT section.
  unsigned int got_offset_;

  // If this symbol has an entry in the PLT section (has_plt_offset_
  // is true), then this is the offset from the start of the PLT
  // section.
  unsigned int plt_offset_;

  // Symbol type.
  elfcpp::STT type_ : 4;
  // Symbol binding.
  elfcpp::STB binding_ : 4;
  // Symbol visibility.
  elfcpp::STV visibility_ : 2;
  // Rest of symbol st_other field.
  unsigned int nonvis_ : 6;
  // The type of symbol.
  Source source_ : 3;
  // True if this symbol always requires special target-specific
  // handling.
  bool is_target_special_ : 1;
  // True if this is the default version of the symbol.
  bool is_def_ : 1;
  // True if this symbol really forwards to another symbol.  This is
  // used when we discover after the fact that two different entries
  // in the hash table really refer to the same symbol.  This will
  // never be set for a symbol found in the hash table, but may be set
  // for a symbol found in the list of symbols attached to an Object.
  // It forwards to the symbol found in the forwarders_ map of
  // Symbol_table.
  bool is_forwarder_ : 1;
  // True if this symbol needs to be in the dynamic symbol table.
  bool needs_dynsym_entry_ : 1;
  // True if we've seen this symbol in a regular object.
  bool in_reg_ : 1;
  // True if we've seen this symbol in a dynamic object.
  bool in_dyn_ : 1;
  // True if the symbol has an entry in the GOT section.
  bool has_got_offset_ : 1;
  // True if the symbol has an entry in the PLT section.
  bool has_plt_offset_ : 1;
  // True if there is a warning for this symbol.
  bool has_warning_ : 1;
};

// The parts of a symbol which are size specific.  Using a template
// derived class like this helps us use less space on a 32-bit system.

template<int size>
class Sized_symbol : public Symbol
{
 public:
  typedef typename elfcpp::Elf_types<size>::Elf_Addr Value_type;
  typedef typename elfcpp::Elf_types<size>::Elf_WXword Size_type;

  Sized_symbol()
  { }

  // Initialize fields from an ELF symbol in OBJECT.
  template<bool big_endian>
  void
  init(const char *name, const char* version, Object* object,
       const elfcpp::Sym<size, big_endian>&);

  // Initialize fields for an Output_data.
  void
  init(const char* name, Output_data*, Value_type value, Size_type symsize,
       elfcpp::STT, elfcpp::STB, elfcpp::STV, unsigned char nonvis,
       bool offset_is_from_end);

  // Initialize fields for an Output_segment.
  void
  init(const char* name, Output_segment*, Value_type value, Size_type symsize,
       elfcpp::STT, elfcpp::STB, elfcpp::STV, unsigned char nonvis,
       Segment_offset_base offset_base);

  // Initialize fields for a constant.
  void
  init(const char* name, Value_type value, Size_type symsize,
       elfcpp::STT, elfcpp::STB, elfcpp::STV, unsigned char nonvis);

  // Override existing symbol.
  template<bool big_endian>
  void
  override(const elfcpp::Sym<size, big_endian>&, Object* object,
	   const char* version);

  // Return the symbol's value.
  Value_type
  value() const
  { return this->value_; }

  // Return the symbol's size (we can't call this 'size' because that
  // is a template parameter).
  Size_type
  symsize() const
  { return this->symsize_; }

  // Set the symbol size.  This is used when resolving common symbols.
  void
  set_symsize(Size_type symsize)
  { this->symsize_ = symsize; }

  // Set the symbol value.  This is called when we store the final
  // values of the symbols into the symbol table.
  void
  set_value(Value_type value)
  { this->value_ = value; }

 private:
  Sized_symbol(const Sized_symbol&);
  Sized_symbol& operator=(const Sized_symbol&);

  // Symbol value.  Before Layout::finalize this is the offset in the
  // input section.  This is set to the final value during
  // Layout::finalize.
  Value_type value_;
  // Symbol size.
  Size_type symsize_;
};

// A struct describing a symbol defined by the linker, where the value
// of the symbol is defined based on an output section.  This is used
// for symbols defined by the linker, like "_init_array_start".

struct Define_symbol_in_section
{
  // The symbol name.
  const char* name;
  // The name of the output section with which this symbol should be
  // associated.  If there is no output section with that name, the
  // symbol will be defined as zero.
  const char* output_section;
  // The offset of the symbol within the output section.  This is an
  // offset from the start of the output section, unless start_at_end
  // is true, in which case this is an offset from the end of the
  // output section.
  uint64_t value;
  // The size of the symbol.
  uint64_t size;
  // The symbol type.
  elfcpp::STT type;
  // The symbol binding.
  elfcpp::STB binding;
  // The symbol visibility.
  elfcpp::STV visibility;
  // The rest of the st_other field.
  unsigned char nonvis;
  // If true, the value field is an offset from the end of the output
  // section.
  bool offset_is_from_end;
  // If true, this symbol is defined only if we see a reference to it.
  bool only_if_ref;
};

// A struct describing a symbol defined by the linker, where the value
// of the symbol is defined based on a segment.  This is used for
// symbols defined by the linker, like "_end".  We describe the
// segment with which the symbol should be associated by its
// characteristics.  If no segment meets these characteristics, the
// symbol will be defined as zero.  If there is more than one segment
// which meets these characteristics, we will use the first one.

struct Define_symbol_in_segment
{
  // The symbol name.
  const char* name;
  // The segment type where the symbol should be defined, typically
  // PT_LOAD.
  elfcpp::PT segment_type;
  // Bitmask of segment flags which must be set.
  elfcpp::PF segment_flags_set;
  // Bitmask of segment flags which must be clear.
  elfcpp::PF segment_flags_clear;
  // The offset of the symbol within the segment.  The offset is
  // calculated from the position set by offset_base.
  uint64_t value;
  // The size of the symbol.
  uint64_t size;
  // The symbol type.
  elfcpp::STT type;
  // The symbol binding.
  elfcpp::STB binding;
  // The symbol visibility.
  elfcpp::STV visibility;
  // The rest of the st_other field.
  unsigned char nonvis;
  // The base from which we compute the offset.
  Symbol::Segment_offset_base offset_base;
  // If true, this symbol is defined only if we see a reference to it.
  bool only_if_ref;
};

// This class manages warnings.  Warnings are a GNU extension.  When
// we see a section named .gnu.warning.SYM in an object file, and if
// we wind using the definition of SYM from that object file, then we
// will issue a warning for any relocation against SYM from a
// different object file.  The text of the warning is the contents of
// the section.  This is not precisely the definition used by the old
// GNU linker; the old GNU linker treated an occurrence of
// .gnu.warning.SYM as defining a warning symbol.  A warning symbol
// would trigger a warning on any reference.  However, it was
// inconsistent in that a warning in a dynamic object only triggered
// if there was no definition in a regular object.  This linker is
// different in that we only issue a warning if we use the symbol
// definition from the same object file as the warning section.

class Warnings
{
 public:
  Warnings()
    : warnings_()
  { }

  // Add a warning for symbol NAME in section SHNDX in object OBJ.
  void
  add_warning(Symbol_table* symtab, const char* name, Object* obj,
	      unsigned int shndx);

  // For each symbol for which we should give a warning, make a note
  // on the symbol.
  void
  note_warnings(Symbol_table* symtab);

  // Issue a warning for a reference to SYM at LOCATION.
  void
  issue_warning(const Symbol* sym, const std::string& location) const;

 private:
  Warnings(const Warnings&);
  Warnings& operator=(const Warnings&);

  // What we need to know to get the warning text.
  struct Warning_location
  {
    // The object the warning is in.
    Object* object;
    // The index of the warning section.
    unsigned int shndx;
    // The warning text if we have already loaded it.
    std::string text;

    Warning_location()
      : object(NULL), shndx(0), text()
    { }

    void
    set(Object* o, unsigned int s)
    {
      this->object = o;
      this->shndx = s;
    }

    void
    set_text(const char* t, off_t l)
    { this->text.assign(t, l); }
  };

  // A mapping from warning symbol names (canonicalized in
  // Symbol_table's namepool_ field) to 
  typedef Unordered_map<const char*, Warning_location> Warning_table;

  Warning_table warnings_;
};

// The main linker symbol table.

class Symbol_table
{
 public:
  Symbol_table();

  ~Symbol_table();

  // Add COUNT external symbols from the relocatable object RELOBJ to
  // the symbol table.  SYMS is the symbols, SYM_NAMES is their names,
  // SYM_NAME_SIZE is the size of SYM_NAMES.  This sets SYMPOINTERS to
  // point to the symbols in the symbol table.
  template<int size, bool big_endian>
  void
  add_from_relobj(Sized_relobj<size, big_endian>* relobj,
		  const unsigned char* syms, size_t count,
		  const char* sym_names, size_t sym_name_size,
		  Symbol** sympointers);

  // Add COUNT dynamic symbols from the dynamic object DYNOBJ to the
  // symbol table.  SYMS is the symbols.  SYM_NAMES is their names.
  // SYM_NAME_SIZE is the size of SYM_NAMES.  The other parameters are
  // symbol version data.
  template<int size, bool big_endian>
  void
  add_from_dynobj(Sized_dynobj<size, big_endian>* dynobj,
		  const unsigned char* syms, size_t count,
		  const char* sym_names, size_t sym_name_size,
		  const unsigned char* versym, size_t versym_size,
		  const std::vector<const char*>*);

  // Define a special symbol based on an Output_data.  It is a
  // multiple definition error if this symbol is already defined.
  Symbol*
  define_in_output_data(const Target*, const char* name, const char* version,
			Output_data*, uint64_t value, uint64_t symsize,
			elfcpp::STT type, elfcpp::STB binding,
			elfcpp::STV visibility, unsigned char nonvis,
			bool offset_is_from_end, bool only_if_ref);

  // Define a special symbol based on an Output_segment.  It is a
  // multiple definition error if this symbol is already defined.
  Symbol*
  define_in_output_segment(const Target*, const char* name,
			   const char* version, Output_segment*,
			   uint64_t value, uint64_t symsize,
			   elfcpp::STT type, elfcpp::STB binding,
			   elfcpp::STV visibility, unsigned char nonvis,
			   Symbol::Segment_offset_base, bool only_if_ref);

  // Define a special symbol with a constant value.  It is a multiple
  // definition error if this symbol is already defined.
  Symbol*
  define_as_constant(const Target*, const char* name, const char* version,
		     uint64_t value, uint64_t symsize, elfcpp::STT type,
		     elfcpp::STB binding, elfcpp::STV visibility,
		     unsigned char nonvis, bool only_if_ref);

  // Define a set of symbols in output sections.
  void
  define_symbols(const Layout*, const Target*, int count,
		 const Define_symbol_in_section*);

  // Define a set of symbols in output segments.
  void
  define_symbols(const Layout*, const Target*, int count,
		 const Define_symbol_in_segment*);  

  // Look up a symbol.
  Symbol*
  lookup(const char*, const char* version = NULL) const;

  // Return the real symbol associated with the forwarder symbol FROM.
  Symbol*
  resolve_forwards(const Symbol* from) const;

  // Return the bitsize (32 or 64) of the symbols in the table.
  int
  get_size() const
  { return this->size_; }

  // Return the sized version of a symbol in this table.
  template<int size>
  Sized_symbol<size>*
  get_sized_symbol(Symbol* ACCEPT_SIZE) const;

  template<int size>
  const Sized_symbol<size>*
  get_sized_symbol(const Symbol* ACCEPT_SIZE) const;

  // Return the count of undefined symbols seen.
  int
  saw_undefined() const
  { return this->saw_undefined_; }

  // Allocate the common symbols
  void
  allocate_commons(const General_options&, Layout*);

  // Add a warning for symbol NAME in section SHNDX in object OBJ.
  void
  add_warning(const char* name, Object* obj, unsigned int shndx)
  { this->warnings_.add_warning(this, name, obj, shndx); }

  // Canonicalize a symbol name for use in the hash table.
  const char*
  canonicalize_name(const char* name)
  { return this->namepool_.add(name, NULL); }

  // Possibly issue a warning for a reference to SYM at LOCATION which
  // is in OBJ.
  void
  issue_warning(const Symbol* sym, const std::string& location) const
  { this->warnings_.issue_warning(sym, location); }

  // Set the dynamic symbol indexes.  INDEX is the index of the first
  // global dynamic symbol.  Pointers to the symbols are stored into
  // the vector.  The names are stored into the Stringpool.  This
  // returns an updated dynamic symbol index.
  unsigned int
  set_dynsym_indexes(const General_options*, const Target*, unsigned int index,
		     std::vector<Symbol*>*, Stringpool*, Versions*);

  // Finalize the symbol table after we have set the final addresses
  // of all the input sections.  This sets the final symbol indexes,
  // values and adds the names to *POOL.  INDEX is the index of the
  // first global symbol.  OFF is the file offset of the global symbol
  // table, DYNOFF is the offset of the globals in the dynamic symbol
  // table, DYN_GLOBAL_INDEX is the index of the first global dynamic
  // symbol, and DYNCOUNT is the number of global dynamic symbols.
  // This records the parameters, and returns the new file offset.
  off_t
  finalize(unsigned int index, off_t off, off_t dynoff,
	   size_t dyn_global_index, size_t dyncount, Stringpool* pool);

  // Write out the global symbols.
  void
  write_globals(const Target*, const Stringpool*, const Stringpool*,
		Output_file*) const;

  // Write out a section symbol.  Return the updated offset.
  void
  write_section_symbol(const Target*, const Output_section*, Output_file*,
		       off_t) const;

 private:
  Symbol_table(const Symbol_table&);
  Symbol_table& operator=(const Symbol_table&);

  // Set the size (32 or 64) of the symbols in the table.
  void
  set_size(int size)
  { this->size_ = size; }

  // Make FROM a forwarder symbol to TO.
  void
  make_forwarder(Symbol* from, Symbol* to);

  // Add a symbol.
  template<int size, bool big_endian>
  Symbol*
  add_from_object(Object*, const char *name, Stringpool::Key name_key,
		  const char *version, Stringpool::Key version_key,
		  bool def, const elfcpp::Sym<size, big_endian>& sym);

  // Resolve symbols.
  template<int size, bool big_endian>
  static void
  resolve(Sized_symbol<size>* to,
	  const elfcpp::Sym<size, big_endian>& sym,
	  Object*, const char* version);

  template<int size, bool big_endian>
  static void
  resolve(Sized_symbol<size>* to, const Sized_symbol<size>* from,
          const char* version ACCEPT_SIZE_ENDIAN);

  // Define a special symbol.
  template<int size, bool big_endian>
  Sized_symbol<size>*
  define_special_symbol(const Target* target, const char* name,
			const char* version, bool only_if_ref
			ACCEPT_SIZE_ENDIAN);

  // Define a symbol in an Output_data, sized version.
  template<int size>
  Sized_symbol<size>*
  do_define_in_output_data(const Target*, const char* name,
			   const char* version, Output_data*,
			   typename elfcpp::Elf_types<size>::Elf_Addr value,
			   typename elfcpp::Elf_types<size>::Elf_WXword ssize,
			   elfcpp::STT type, elfcpp::STB binding,
			   elfcpp::STV visibility, unsigned char nonvis,
			   bool offset_is_from_end, bool only_if_ref);

  // Define a symbol in an Output_segment, sized version.
  template<int size>
  Sized_symbol<size>*
  do_define_in_output_segment(
    const Target*, const char* name, const char* version, Output_segment* os,
    typename elfcpp::Elf_types<size>::Elf_Addr value,
    typename elfcpp::Elf_types<size>::Elf_WXword ssize,
    elfcpp::STT type, elfcpp::STB binding,
    elfcpp::STV visibility, unsigned char nonvis,
    Symbol::Segment_offset_base offset_base, bool only_if_ref);

  // Define a symbol as a constant, sized version.
  template<int size>
  Sized_symbol<size>*
  do_define_as_constant(
    const Target*, const char* name, const char* version,
    typename elfcpp::Elf_types<size>::Elf_Addr value,
    typename elfcpp::Elf_types<size>::Elf_WXword ssize,
    elfcpp::STT type, elfcpp::STB binding,
    elfcpp::STV visibility, unsigned char nonvis,
    bool only_if_ref);

  // Allocate the common symbols, sized version.
  template<int size>
  void
  do_allocate_commons(const General_options&, Layout*);

  // Finalize symbols specialized for size.
  template<int size>
  off_t
  sized_finalize(unsigned int, off_t, Stringpool*);

  // Write globals specialized for size and endianness.
  template<int size, bool big_endian>
  void
  sized_write_globals(const Target*, const Stringpool*, const Stringpool*,
		      Output_file*) const;

  // Write out a symbol to P.
  template<int size, bool big_endian>
  void
  sized_write_symbol(Sized_symbol<size>*, unsigned int shndx,
		     const Stringpool*, unsigned char* p
                     ACCEPT_SIZE_ENDIAN) const;

  // Write out a section symbol, specialized for size and endianness.
  template<int size, bool big_endian>
  void
  sized_write_section_symbol(const Output_section*, Output_file*, off_t) const;

  // The type of the symbol hash table.

  typedef std::pair<Stringpool::Key, Stringpool::Key> Symbol_table_key;

  struct Symbol_table_hash
  {
    size_t
    operator()(const Symbol_table_key&) const;
  };

  struct Symbol_table_eq
  {
    bool
    operator()(const Symbol_table_key&, const Symbol_table_key&) const;
  };

  typedef Unordered_map<Symbol_table_key, Symbol*, Symbol_table_hash,
			Symbol_table_eq> Symbol_table_type;

  // The type of the list of common symbols.

  typedef std::vector<Symbol*> Commons_type;

  // The size of the symbols in the symbol table (32 or 64).
  int size_;

  // We increment this every time we see a new undefined symbol, for
  // use in archive groups.
  int saw_undefined_;

  // The index of the first global symbol in the output file.
  unsigned int first_global_index_;

  // The file offset within the output symtab section where we should
  // write the table.
  off_t offset_;

  // The number of global symbols we want to write out.
  size_t output_count_;

  // The file offset of the global dynamic symbols, or 0 if none.
  off_t dynamic_offset_;

  // The index of the first global dynamic symbol.
  unsigned int first_dynamic_global_index_;

  // The number of global dynamic symbols, or 0 if none.
  off_t dynamic_count_;

  // The symbol hash table.
  Symbol_table_type table_;

  // A pool of symbol names.  This is used for all global symbols.
  // Entries in the hash table point into this pool.
  Stringpool namepool_;

  // Forwarding symbols.
  Unordered_map<const Symbol*, Symbol*> forwarders_;

  // We don't expect there to be very many common symbols, so we keep
  // a list of them.  When we find a common symbol we add it to this
  // list.  It is possible that by the time we process the list the
  // symbol is no longer a common symbol.  It may also have become a
  // forwarder.
  Commons_type commons_;

  // Manage symbol warnings.
  Warnings warnings_;
};

// We inline get_sized_symbol for efficiency.

template<int size>
Sized_symbol<size>*
Symbol_table::get_sized_symbol(Symbol* sym ACCEPT_SIZE) const
{
  gold_assert(size == this->get_size());
  return static_cast<Sized_symbol<size>*>(sym);
}

template<int size>
const Sized_symbol<size>*
Symbol_table::get_sized_symbol(const Symbol* sym ACCEPT_SIZE) const
{
  gold_assert(size == this->get_size());
  return static_cast<const Sized_symbol<size>*>(sym);
}

} // End namespace gold.

#endif // !defined(GOLD_SYMTAB_H)

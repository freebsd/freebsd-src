// output.h -- manage the output file for gold   -*- C++ -*-

#ifndef GOLD_OUTPUT_H
#define GOLD_OUTPUT_H

#include <list>
#include <vector>

#include "elfcpp.h"
#include "layout.h"
#include "reloc-types.h"

namespace gold
{

class General_options;
class Object;
class Symbol;
class Output_file;
class Output_section;
class Target;
template<int size, bool big_endian>
class Sized_target;
template<int size, bool big_endian>
class Sized_relobj;

// An abtract class for data which has to go into the output file.

class Output_data
{
 public:
  explicit Output_data(off_t data_size = 0)
    : address_(0), data_size_(data_size), offset_(-1)
  { }

  virtual
  ~Output_data();

  // Return the address.  This is only valid after Layout::finalize is
  // finished.
  uint64_t
  address() const
  { return this->address_; }

  // Return the size of the data.  This must be valid after
  // Layout::finalize calls set_address, but need not be valid before
  // then.
  off_t
  data_size() const
  { return this->data_size_; }

  // Return the file offset.  This is only valid after
  // Layout::finalize is finished.
  off_t
  offset() const
  { return this->offset_; }

  // Return the required alignment.
  uint64_t
  addralign() const
  { return this->do_addralign(); }

  // Return whether this is an Output_section.
  bool
  is_section() const
  { return this->do_is_section(); }

  // Return whether this is an Output_section of the specified type.
  bool
  is_section_type(elfcpp::Elf_Word stt) const
  { return this->do_is_section_type(stt); }

  // Return whether this is an Output_section with the specified flag
  // set.
  bool
  is_section_flag_set(elfcpp::Elf_Xword shf) const
  { return this->do_is_section_flag_set(shf); }

  // Return the output section index, if there is an output section.
  unsigned int
  out_shndx() const
  { return this->do_out_shndx(); }

  // Set the output section index, if this is an output section.
  void
  set_out_shndx(unsigned int shndx)
  { this->do_set_out_shndx(shndx); }

  // Set the address and file offset of this data.  This is called
  // during Layout::finalize.
  void
  set_address(uint64_t addr, off_t off);

  // Write the data to the output file.  This is called after
  // Layout::finalize is complete.
  void
  write(Output_file* file)
  { this->do_write(file); }

  // This is called by Layout::finalize to note that all sizes must
  // now be fixed.
  static void
  layout_complete()
  { Output_data::sizes_are_fixed = true; }

 protected:
  // Functions that child classes may or in some cases must implement.

  // Write the data to the output file.
  virtual void
  do_write(Output_file*) = 0;

  // Return the required alignment.
  virtual uint64_t
  do_addralign() const = 0;

  // Return whether this is an Output_section.
  virtual bool
  do_is_section() const
  { return false; }

  // Return whether this is an Output_section of the specified type.
  // This only needs to be implement by Output_section.
  virtual bool
  do_is_section_type(elfcpp::Elf_Word) const
  { return false; }

  // Return whether this is an Output_section with the specific flag
  // set.  This only needs to be implemented by Output_section.
  virtual bool
  do_is_section_flag_set(elfcpp::Elf_Xword) const
  { return false; }

  // Return the output section index, if there is an output section.
  virtual unsigned int
  do_out_shndx() const
  { gold_unreachable(); }

  // Set the output section index, if this is an output section.
  virtual void
  do_set_out_shndx(unsigned int)
  { gold_unreachable(); }

  // Set the address and file offset of the data.  This only needs to
  // be implemented if the child needs to know.  The child class can
  // set its size in this call.
  virtual void
  do_set_address(uint64_t, off_t)
  { }

  // Functions that child classes may call.

  // Set the size of the data.
  void
  set_data_size(off_t data_size)
  {
    gold_assert(!Output_data::sizes_are_fixed);
    this->data_size_ = data_size;
  }

  // Return default alignment for a size--32 or 64.
  static uint64_t
  default_alignment(int size);

 private:
  Output_data(const Output_data&);
  Output_data& operator=(const Output_data&);

  // This is used for verification, to make sure that we don't try to
  // change any sizes after we set the section addresses.
  static bool sizes_are_fixed;

  // Memory address in file (not always meaningful).
  uint64_t address_;
  // Size of data in file.
  off_t data_size_;
  // Offset within file.
  off_t offset_;
};

// Output the section headers.

class Output_section_headers : public Output_data
{
 public:
  Output_section_headers(int size,
			 bool big_endian,
			 const Layout*,
			 const Layout::Segment_list*,
			 const Layout::Section_list*,
			 const Stringpool*);

  // Write the data to the file.
  void
  do_write(Output_file*);

  // Return the required alignment.
  uint64_t
  do_addralign() const
  { return Output_data::default_alignment(this->size_); }

 private:
  // Write the data to the file with the right size and endianness.
  template<int size, bool big_endian>
  void
  do_sized_write(Output_file*);

  int size_;
  bool big_endian_;
  const Layout* layout_;
  const Layout::Segment_list* segment_list_;
  const Layout::Section_list* unattached_section_list_;
  const Stringpool* secnamepool_;
};

// Output the segment headers.

class Output_segment_headers : public Output_data
{
 public:
  Output_segment_headers(int size, bool big_endian,
			 const Layout::Segment_list& segment_list);

  // Write the data to the file.
  void
  do_write(Output_file*);

  // Return the required alignment.
  uint64_t
  do_addralign() const
  { return Output_data::default_alignment(this->size_); }

 private:
  // Write the data to the file with the right size and endianness.
  template<int size, bool big_endian>
  void
  do_sized_write(Output_file*);

  int size_;
  bool big_endian_;
  const Layout::Segment_list& segment_list_;
};

// Output the ELF file header.

class Output_file_header : public Output_data
{
 public:
  Output_file_header(int size,
		     bool big_endian,
		     const General_options&,
		     const Target*,
		     const Symbol_table*,
		     const Output_segment_headers*);

  // Add information about the section headers.  We lay out the ELF
  // file header before we create the section headers.
  void set_section_info(const Output_section_headers*,
			const Output_section* shstrtab);

  // Write the data to the file.
  void
  do_write(Output_file*);

  // Return the required alignment.
  uint64_t
  do_addralign() const
  { return Output_data::default_alignment(this->size_); }

  // Set the address and offset--we only implement this for error
  // checking.
  void
  do_set_address(uint64_t, off_t off) const
  { gold_assert(off == 0); }

 private:
  // Write the data to the file with the right size and endianness.
  template<int size, bool big_endian>
  void
  do_sized_write(Output_file*);

  int size_;
  bool big_endian_;
  const General_options& options_;
  const Target* target_;
  const Symbol_table* symtab_;
  const Output_segment_headers* segment_header_;
  const Output_section_headers* section_header_;
  const Output_section* shstrtab_;
};

// Output sections are mainly comprised of input sections.  However,
// there are cases where we have data to write out which is not in an
// input section.  Output_section_data is used in such cases.  This is
// an abstract base class.

class Output_section_data : public Output_data
{
 public:
  Output_section_data(off_t data_size, uint64_t addralign)
    : Output_data(data_size), output_section_(NULL), addralign_(addralign)
  { }

  Output_section_data(uint64_t addralign)
    : Output_data(0), output_section_(NULL), addralign_(addralign)
  { }

  // Return the output section.
  const Output_section*
  output_section() const
  { return this->output_section_; }

  // Record the output section.
  void
  set_output_section(Output_section* os);

  // Add an input section, for SHF_MERGE sections.  This returns true
  // if the section was handled.
  bool
  add_input_section(Relobj* object, unsigned int shndx)
  { return this->do_add_input_section(object, shndx); }

  // Given an input OBJECT, an input section index SHNDX within that
  // object, and an OFFSET relative to the start of that input
  // section, return whether or not the output address is known.
  // OUTPUT_SECTION_ADDRESS is the address of the output section which
  // this is a part of.  If this function returns true, it sets
  // *POUTPUT to the output address.
  virtual bool
  output_address(const Relobj* object, unsigned int shndx, off_t offset,
		 uint64_t output_section_address, uint64_t *poutput) const
  {
    return this->do_output_address(object, shndx, offset,
				   output_section_address, poutput);
  }

 protected:
  // The child class must implement do_write.

  // The child class may implement specific adjustments to the output
  // section.
  virtual void
  do_adjust_output_section(Output_section*)
  { }

  // May be implemented by child class.  Return true if the section
  // was handled.
  virtual bool
  do_add_input_section(Relobj*, unsigned int)
  { gold_unreachable(); }

  // The child class may implement output_address.
  virtual bool
  do_output_address(const Relobj*, unsigned int, off_t, uint64_t,
		    uint64_t*) const
  { return false; }

  // Return the required alignment.
  uint64_t
  do_addralign() const
  { return this->addralign_; }

  // Return the section index of the output section.
  unsigned int
  do_out_shndx() const;

  // Set the alignment.
  void
  set_addralign(uint64_t addralign)
  { this->addralign_ = addralign; }

 private:
  // The output section for this section.
  const Output_section* output_section_;
  // The required alignment.
  uint64_t addralign_;
};

// A simple case of Output_data in which we have constant data to
// output.

class Output_data_const : public Output_section_data
{
 public:
  Output_data_const(const std::string& data, uint64_t addralign)
    : Output_section_data(data.size(), addralign), data_(data)
  { }

  Output_data_const(const char* p, off_t len, uint64_t addralign)
    : Output_section_data(len, addralign), data_(p, len)
  { }

  Output_data_const(const unsigned char* p, off_t len, uint64_t addralign)
    : Output_section_data(len, addralign),
      data_(reinterpret_cast<const char*>(p), len)
  { }

  // Add more data.
  void
  add_data(const std::string& add)
  {
    this->data_.append(add);
    this->set_data_size(this->data_.size());
  }

  // Write the data to the output file.
  void
  do_write(Output_file*);

 private:
  std::string data_;
};

// Another version of Output_data with constant data, in which the
// buffer is allocated by the caller.

class Output_data_const_buffer : public Output_section_data
{
 public:
  Output_data_const_buffer(const unsigned char* p, off_t len,
			   uint64_t addralign)
    : Output_section_data(len, addralign), p_(p)
  { }

  // Write the data the output file.
  void
  do_write(Output_file*);

 private:
  const unsigned char* p_;
};

// A place holder for data written out via some other mechanism.

class Output_data_space : public Output_section_data
{
 public:
  Output_data_space(off_t data_size, uint64_t addralign)
    : Output_section_data(data_size, addralign)
  { }

  explicit Output_data_space(uint64_t addralign)
    : Output_section_data(addralign)
  { }

  // Set the size.
  void
  set_space_size(off_t space_size)
  { this->set_data_size(space_size); }

  // Set the alignment.
  void
  set_space_alignment(uint64_t align)
  { this->set_addralign(align); }

  // Write out the data--this must be handled elsewhere.
  void
  do_write(Output_file*)
  { }
};

// A string table which goes into an output section.

class Output_data_strtab : public Output_section_data
{
 public:
  Output_data_strtab(Stringpool* strtab)
    : Output_section_data(1), strtab_(strtab)
  { }

  // This is called to set the address and file offset.  Here we make
  // sure that the Stringpool is finalized.
  void
  do_set_address(uint64_t, off_t);

  // Write out the data.
  void
  do_write(Output_file*);

 private:
  Stringpool* strtab_;
};

// This POD class is used to represent a single reloc in the output
// file.  This could be a private class within Output_data_reloc, but
// the templatization is complex enough that I broke it out into a
// separate class.  The class is templatized on either elfcpp::SHT_REL
// or elfcpp::SHT_RELA, and also on whether this is a dynamic
// relocation or an ordinary relocation.

// A relocation can be against a global symbol, a local symbol, an
// output section, or the undefined symbol at index 0.  We represent
// the latter by using a NULL global symbol.

template<int sh_type, bool dynamic, int size, bool big_endian>
class Output_reloc;

template<bool dynamic, int size, bool big_endian>
class Output_reloc<elfcpp::SHT_REL, dynamic, size, big_endian>
{
 public:
  typedef typename elfcpp::Elf_types<size>::Elf_Addr Address;

  // An uninitialized entry.  We need this because we want to put
  // instances of this class into an STL container.
  Output_reloc()
    : local_sym_index_(INVALID_CODE)
  { }

  // A reloc against a global symbol.

  Output_reloc(Symbol* gsym, unsigned int type, Output_data* od,
	       Address address)
    : address_(address), local_sym_index_(GSYM_CODE), type_(type),
      shndx_(INVALID_CODE)
  {
    this->u1_.gsym = gsym;
    this->u2_.od = od;
  }

  Output_reloc(Symbol* gsym, unsigned int type, Relobj* relobj,
	       unsigned int shndx, Address address)
    : address_(address), local_sym_index_(GSYM_CODE), type_(type),
      shndx_(shndx)
  {
    gold_assert(shndx != INVALID_CODE);
    this->u1_.gsym = gsym;
    this->u2_.relobj = relobj;
  }

  // A reloc against a local symbol.

  Output_reloc(Sized_relobj<size, big_endian>* relobj,
	       unsigned int local_sym_index,
	       unsigned int type,
	       Output_data* od,
	       Address address)
    : address_(address), local_sym_index_(local_sym_index), type_(type),
      shndx_(INVALID_CODE)
  {
    gold_assert(local_sym_index != GSYM_CODE
		&& local_sym_index != INVALID_CODE);
    this->u1_.relobj = relobj;
    this->u2_.od = od;
  }

  Output_reloc(Sized_relobj<size, big_endian>* relobj,
	       unsigned int local_sym_index,
	       unsigned int type,
	       unsigned int shndx,
	       Address address)
    : address_(address), local_sym_index_(local_sym_index), type_(type),
      shndx_(shndx)
  {
    gold_assert(local_sym_index != GSYM_CODE
		&& local_sym_index != INVALID_CODE);
    gold_assert(shndx != INVALID_CODE);
    this->u1_.relobj = relobj;
    this->u2_.relobj = relobj;
  }

  // A reloc against the STT_SECTION symbol of an output section.

  Output_reloc(Output_section* os, unsigned int type, Output_data* od,
	       Address address)
    : address_(address), local_sym_index_(SECTION_CODE), type_(type),
      shndx_(INVALID_CODE)
  {
    this->u1_.os = os;
    this->u2_.od = od;
  }

  Output_reloc(Output_section* os, unsigned int type, Relobj* relobj,
	       unsigned int shndx, Address address)
    : address_(address), local_sym_index_(SECTION_CODE), type_(type),
      shndx_(shndx)
  {
    gold_assert(shndx != INVALID_CODE);
    this->u1_.os = os;
    this->u2_.relobj = relobj;
  }

  // Write the reloc entry to an output view.
  void
  write(unsigned char* pov) const;

  // Write the offset and info fields to Write_rel.
  template<typename Write_rel>
  void write_rel(Write_rel*) const;

 private:
  // Return the symbol index.  We can't do a double template
  // specialization, so we do a secondary template here.
  unsigned int
  get_symbol_index() const;

  // Codes for local_sym_index_.
  enum
  {
    // Global symbol.
    GSYM_CODE = -1U,
    // Output section.
    SECTION_CODE = -2U,
    // Invalid uninitialized entry.
    INVALID_CODE = -3U
  };

  union
  {
    // For a local symbol, the object.  We will never generate a
    // relocation against a local symbol in a dynamic object; that
    // doesn't make sense.  And our callers will always be
    // templatized, so we use Sized_relobj here.
    Sized_relobj<size, big_endian>* relobj;
    // For a global symbol, the symbol.  If this is NULL, it indicates
    // a relocation against the undefined 0 symbol.
    Symbol* gsym;
    // For a relocation against an output section, the output section.
    Output_section* os;
  } u1_;
  union
  {
    // If shndx_ is not INVALID CODE, the object which holds the input
    // section being used to specify the reloc address.
    Relobj* relobj;
    // If shndx_ is INVALID_CODE, the output data being used to
    // specify the reloc address.  This may be NULL if the reloc
    // address is absolute.
    Output_data* od;
  } u2_;
  // The address offset within the input section or the Output_data.
  Address address_;
  // For a local symbol, the local symbol index.  This is GSYM_CODE
  // for a global symbol, or INVALID_CODE for an uninitialized value.
  unsigned int local_sym_index_;
  // The reloc type--a processor specific code.
  unsigned int type_;
  // If the reloc address is an input section in an object, the
  // section index.  This is INVALID_CODE if the reloc address is
  // specified in some other way.
  unsigned int shndx_;
};

// The SHT_RELA version of Output_reloc<>.  This is just derived from
// the SHT_REL version of Output_reloc, but it adds an addend.

template<bool dynamic, int size, bool big_endian>
class Output_reloc<elfcpp::SHT_RELA, dynamic, size, big_endian>
{
 public:
  typedef typename elfcpp::Elf_types<size>::Elf_Addr Address;
  typedef typename elfcpp::Elf_types<size>::Elf_Addr Addend;

  // An uninitialized entry.
  Output_reloc()
    : rel_()
  { }

  // A reloc against a global symbol.

  Output_reloc(Symbol* gsym, unsigned int type, Output_data* od,
	       Address address, Addend addend)
    : rel_(gsym, type, od, address), addend_(addend)
  { }

  Output_reloc(Symbol* gsym, unsigned int type, Relobj* relobj,
	       unsigned int shndx, Address address, Addend addend)
    : rel_(gsym, type, relobj, shndx, address), addend_(addend)
  { }

  // A reloc against a local symbol.

  Output_reloc(Sized_relobj<size, big_endian>* relobj,
	       unsigned int local_sym_index,
	       unsigned int type, Output_data* od, Address address,
	       Addend addend)
    : rel_(relobj, local_sym_index, type, od, address), addend_(addend)
  { }

  Output_reloc(Sized_relobj<size, big_endian>* relobj,
	       unsigned int local_sym_index,
	       unsigned int type,
	       unsigned int shndx,
	       Address address,
	       Addend addend)
    : rel_(relobj, local_sym_index, type, shndx, address),
      addend_(addend)
  { }

  // A reloc against the STT_SECTION symbol of an output section.

  Output_reloc(Output_section* os, unsigned int type, Output_data* od,
	       Address address, Addend addend)
    : rel_(os, type, od, address), addend_(addend)
  { }

  Output_reloc(Output_section* os, unsigned int type, Relobj* relobj,
	       unsigned int shndx, Address address, Addend addend)
    : rel_(os, type, relobj, shndx, address), addend_(addend)
  { }

  // Write the reloc entry to an output view.
  void
  write(unsigned char* pov) const;

 private:
  // The basic reloc.
  Output_reloc<elfcpp::SHT_REL, dynamic, size, big_endian> rel_;
  // The addend.
  Addend addend_;
};

// Output_data_reloc is used to manage a section containing relocs.
// SH_TYPE is either elfcpp::SHT_REL or elfcpp::SHT_RELA.  DYNAMIC
// indicates whether this is a dynamic relocation or a normal
// relocation.  Output_data_reloc_base is a base class.
// Output_data_reloc is the real class, which we specialize based on
// the reloc type.

template<int sh_type, bool dynamic, int size, bool big_endian>
class Output_data_reloc_base : public Output_section_data
{
 public:
  typedef Output_reloc<sh_type, dynamic, size, big_endian> Output_reloc_type;
  typedef typename Output_reloc_type::Address Address;
  static const int reloc_size =
    Reloc_types<sh_type, size, big_endian>::reloc_size;

  // Construct the section.
  Output_data_reloc_base()
    : Output_section_data(Output_data::default_alignment(size))
  { }

  // Write out the data.
  void
  do_write(Output_file*);

 protected:
  // Set the entry size and the link.
  void
  do_adjust_output_section(Output_section *os);

  // Add a relocation entry.
  void
  add(const Output_reloc_type& reloc)
  {
    this->relocs_.push_back(reloc);
    this->set_data_size(this->relocs_.size() * reloc_size);
  }

 private:
  typedef std::vector<Output_reloc_type> Relocs;

  Relocs relocs_;
};

// The class which callers actually create.

template<int sh_type, bool dynamic, int size, bool big_endian>
class Output_data_reloc;

// The SHT_REL version of Output_data_reloc.

template<bool dynamic, int size, bool big_endian>
class Output_data_reloc<elfcpp::SHT_REL, dynamic, size, big_endian>
  : public Output_data_reloc_base<elfcpp::SHT_REL, dynamic, size, big_endian>
{
 private: 
  typedef Output_data_reloc_base<elfcpp::SHT_REL, dynamic, size,
				 big_endian> Base;

 public:
  typedef typename Base::Output_reloc_type Output_reloc_type;
  typedef typename Output_reloc_type::Address Address;

  Output_data_reloc()
    : Output_data_reloc_base<elfcpp::SHT_REL, dynamic, size, big_endian>()
  { }

  // Add a reloc against a global symbol.

  void
  add_global(Symbol* gsym, unsigned int type, Output_data* od, Address address)
  { this->add(Output_reloc_type(gsym, type, od, address)); }

  void
  add_global(Symbol* gsym, unsigned int type, Relobj* relobj,
	     unsigned int shndx, Address address)
  { this->add(Output_reloc_type(gsym, type, relobj, shndx, address)); }

  // Add a reloc against a local symbol.

  void
  add_local(Sized_relobj<size, big_endian>* relobj,
	    unsigned int local_sym_index, unsigned int type,
	    Output_data* od, Address address)
  { this->add(Output_reloc_type(relobj, local_sym_index, type, od, address)); }

  void
  add_local(Sized_relobj<size, big_endian>* relobj,
	    unsigned int local_sym_index, unsigned int type,
	    unsigned int shndx, Address address)
  { this->add(Output_reloc_type(relobj, local_sym_index, type, shndx,
				address)); }


  // A reloc against the STT_SECTION symbol of an output section.

  void
  add_output_section(Output_section* os, unsigned int type,
		     Output_data* od, Address address)
  { this->add(Output_reloc_type(os, type, od, address)); }

  void
  add_output_section(Output_section* os, unsigned int type,
		     Relobj* relobj, unsigned int shndx, Address address)
  { this->add(Output_reloc_type(os, type, relobj, shndx, address)); }
};

// The SHT_RELA version of Output_data_reloc.

template<bool dynamic, int size, bool big_endian>
class Output_data_reloc<elfcpp::SHT_RELA, dynamic, size, big_endian>
  : public Output_data_reloc_base<elfcpp::SHT_RELA, dynamic, size, big_endian>
{
 private: 
  typedef Output_data_reloc_base<elfcpp::SHT_RELA, dynamic, size,
				 big_endian> Base;

 public:
  typedef typename Base::Output_reloc_type Output_reloc_type;
  typedef typename Output_reloc_type::Address Address;
  typedef typename Output_reloc_type::Addend Addend;

  Output_data_reloc()
    : Output_data_reloc_base<elfcpp::SHT_RELA, dynamic, size, big_endian>()
  { }

  // Add a reloc against a global symbol.

  void
  add_global(Symbol* gsym, unsigned int type, Output_data* od,
	     Address address, Addend addend)
  { this->add(Output_reloc_type(gsym, type, od, address, addend)); }

  void
  add_global(Symbol* gsym, unsigned int type, Relobj* relobj,
	     unsigned int shndx, Address address, Addend addend)
  { this->add(Output_reloc_type(gsym, type, relobj, shndx, address, addend)); }

  // Add a reloc against a local symbol.

  void
  add_local(Sized_relobj<size, big_endian>* relobj,
	    unsigned int local_sym_index, unsigned int type,
	    Output_data* od, Address address, Addend addend)
  {
    this->add(Output_reloc_type(relobj, local_sym_index, type, od, address,
				addend));
  }

  void
  add_local(Sized_relobj<size, big_endian>* relobj,
	    unsigned int local_sym_index, unsigned int type,
	    unsigned int shndx, Address address, Addend addend)
  {
    this->add(Output_reloc_type(relobj, local_sym_index, type, shndx, address,
				addend));
  }

  // A reloc against the STT_SECTION symbol of an output section.

  void
  add_output_section(Output_section* os, unsigned int type, Output_data* od,
		     Address address, Addend addend)
  { this->add(Output_reloc_type(os, type, od, address, addend)); }

  void
  add_output_section(Output_section* os, unsigned int type, Relobj* relobj,
		     unsigned int shndx, Address address, Addend addend)
  { this->add(Output_reloc_type(os, type, relobj, shndx, address, addend)); }
};

// Output_data_got is used to manage a GOT.  Each entry in the GOT is
// for one symbol--either a global symbol or a local symbol in an
// object.  The target specific code adds entries to the GOT as
// needed.

template<int size, bool big_endian>
class Output_data_got : public Output_section_data
{
 public:
  typedef typename elfcpp::Elf_types<size>::Elf_Addr Valtype;

  Output_data_got(const General_options* options)
    : Output_section_data(Output_data::default_alignment(size)),
      options_(options), entries_()
  { }

  // Add an entry for a global symbol to the GOT.  Return true if this
  // is a new GOT entry, false if the symbol was already in the GOT.
  bool
  add_global(Symbol* gsym);

  // Add an entry for a local symbol to the GOT.  This returns the
  // offset of the new entry from the start of the GOT.
  unsigned int
  add_local(Object* object, unsigned int sym_index)
  {
    this->entries_.push_back(Got_entry(object, sym_index));
    this->set_got_size();
    return this->last_got_offset();
  }

  // Add a constant to the GOT.  This returns the offset of the new
  // entry from the start of the GOT.
  unsigned int
  add_constant(Valtype constant)
  {
    this->entries_.push_back(Got_entry(constant));
    this->set_got_size();
    return this->last_got_offset();
  }

  // Write out the GOT table.
  void
  do_write(Output_file*);

 private:
  // This POD class holds a single GOT entry.
  class Got_entry
  {
   public:
    // Create a zero entry.
    Got_entry()
      : local_sym_index_(CONSTANT_CODE)
    { this->u_.constant = 0; }

    // Create a global symbol entry.
    explicit Got_entry(Symbol* gsym)
      : local_sym_index_(GSYM_CODE)
    { this->u_.gsym = gsym; }

    // Create a local symbol entry.
    Got_entry(Object* object, unsigned int local_sym_index)
      : local_sym_index_(local_sym_index)
    {
      gold_assert(local_sym_index != GSYM_CODE
		  && local_sym_index != CONSTANT_CODE);
      this->u_.object = object;
    }

    // Create a constant entry.  The constant is a host value--it will
    // be swapped, if necessary, when it is written out.
    explicit Got_entry(Valtype constant)
      : local_sym_index_(CONSTANT_CODE)
    { this->u_.constant = constant; }

    // Write the GOT entry to an output view.
    void
    write(const General_options*, unsigned char* pov) const;

   private:
    enum
    {
      GSYM_CODE = -1U,
      CONSTANT_CODE = -2U
    };

    union
    {
      // For a local symbol, the object.
      Object* object;
      // For a global symbol, the symbol.
      Symbol* gsym;
      // For a constant, the constant.
      Valtype constant;
    } u_;
    // For a local symbol, the local symbol index.  This is GSYM_CODE
    // for a global symbol, or CONSTANT_CODE for a constant.
    unsigned int local_sym_index_;
  };

  typedef std::vector<Got_entry> Got_entries;

  // Return the offset into the GOT of GOT entry I.
  unsigned int
  got_offset(unsigned int i) const
  { return i * (size / 8); }

  // Return the offset into the GOT of the last entry added.
  unsigned int
  last_got_offset() const
  { return this->got_offset(this->entries_.size() - 1); }

  // Set the size of the section.
  void
  set_got_size()
  { this->set_data_size(this->got_offset(this->entries_.size())); }

  // Options.
  const General_options* options_;
  // The list of GOT entries.
  Got_entries entries_;
};

// Output_data_dynamic is used to hold the data in SHT_DYNAMIC
// section.

class Output_data_dynamic : public Output_section_data
{
 public:
  Output_data_dynamic(const Target* target, Stringpool* pool)
    : Output_section_data(Output_data::default_alignment(target->get_size())),
      target_(target), entries_(), pool_(pool)
  { }

  // Add a new dynamic entry with a fixed numeric value.
  void
  add_constant(elfcpp::DT tag, unsigned int val)
  { this->add_entry(Dynamic_entry(tag, val)); }

  // Add a new dynamic entry with the address of output data.
  void
  add_section_address(elfcpp::DT tag, const Output_data* od)
  { this->add_entry(Dynamic_entry(tag, od, false)); }

  // Add a new dynamic entry with the size of output data.
  void
  add_section_size(elfcpp::DT tag, const Output_data* od)
  { this->add_entry(Dynamic_entry(tag, od, true)); }

  // Add a new dynamic entry with the address of a symbol.
  void
  add_symbol(elfcpp::DT tag, const Symbol* sym)
  { this->add_entry(Dynamic_entry(tag, sym)); }

  // Add a new dynamic entry with a string.
  void
  add_string(elfcpp::DT tag, const char* str)
  { this->add_entry(Dynamic_entry(tag, this->pool_->add(str, NULL))); }

  // Set the final data size.
  void
  do_set_address(uint64_t, off_t);

  // Write out the dynamic entries.
  void
  do_write(Output_file*);

 protected:
  // Adjust the output section to set the entry size.
  void
  do_adjust_output_section(Output_section*);

 private:
  // This POD class holds a single dynamic entry.
  class Dynamic_entry
  {
   public:
    // Create an entry with a fixed numeric value.
    Dynamic_entry(elfcpp::DT tag, unsigned int val)
      : tag_(tag), classification_(DYNAMIC_NUMBER)
    { this->u_.val = val; }

    // Create an entry with the size or address of a section.
    Dynamic_entry(elfcpp::DT tag, const Output_data* od, bool section_size)
      : tag_(tag),
	classification_(section_size
			? DYNAMIC_SECTION_SIZE
			: DYNAMIC_SECTION_ADDRESS)
    { this->u_.od = od; }

    // Create an entry with the address of a symbol.
    Dynamic_entry(elfcpp::DT tag, const Symbol* sym)
      : tag_(tag), classification_(DYNAMIC_SYMBOL)
    { this->u_.sym = sym; }

    // Create an entry with a string.
    Dynamic_entry(elfcpp::DT tag, const char* str)
      : tag_(tag), classification_(DYNAMIC_STRING)
    { this->u_.str = str; }

    // Write the dynamic entry to an output view.
    template<int size, bool big_endian>
    void
    write(unsigned char* pov, const Stringpool* ACCEPT_SIZE_ENDIAN) const;

   private:
    enum Classification
    {
      // Number.
      DYNAMIC_NUMBER,
      // Section address.
      DYNAMIC_SECTION_ADDRESS,
      // Section size.
      DYNAMIC_SECTION_SIZE,
      // Symbol adress.
      DYNAMIC_SYMBOL,
      // String.
      DYNAMIC_STRING
    };

    union
    {
      // For DYNAMIC_NUMBER.
      unsigned int val;
      // For DYNAMIC_SECTION_ADDRESS and DYNAMIC_SECTION_SIZE.
      const Output_data* od;
      // For DYNAMIC_SYMBOL.
      const Symbol* sym;
      // For DYNAMIC_STRING.
      const char* str;
    } u_;
    // The dynamic tag.
    elfcpp::DT tag_;
    // The type of entry.
    Classification classification_;
  };

  // Add an entry to the list.
  void
  add_entry(const Dynamic_entry& entry)
  { this->entries_.push_back(entry); }

  // Sized version of write function.
  template<int size, bool big_endian>
  void
  sized_write(Output_file* of);

  // The type of the list of entries.
  typedef std::vector<Dynamic_entry> Dynamic_entries;

  // The target.
  const Target* target_;
  // The entries.
  Dynamic_entries entries_;
  // The pool used for strings.
  Stringpool* pool_;
};

// An output section.  We don't expect to have too many output
// sections, so we don't bother to do a template on the size.

class Output_section : public Output_data
{
 public:
  // Create an output section, giving the name, type, and flags.
  Output_section(const char* name, elfcpp::Elf_Word, elfcpp::Elf_Xword);
  virtual ~Output_section();

  // Add a new input section SHNDX, named NAME, with header SHDR, from
  // object OBJECT.  Return the offset within the output section.
  template<int size, bool big_endian>
  off_t
  add_input_section(Relobj* object, unsigned int shndx, const char *name,
		    const elfcpp::Shdr<size, big_endian>& shdr);

  // Add generated data POSD to this output section.
  void
  add_output_section_data(Output_section_data* posd);

  // Return the section name.
  const char*
  name() const
  { return this->name_; }

  // Return the section type.
  elfcpp::Elf_Word
  type() const
  { return this->type_; }

  // Return the section flags.
  elfcpp::Elf_Xword
  flags() const
  { return this->flags_; }

  // Return the section index in the output file.
  unsigned int
  do_out_shndx() const
  { return this->out_shndx_; }

  // Set the output section index.
  void
  do_set_out_shndx(unsigned int shndx)
  { this->out_shndx_ = shndx; }

  // Return the entsize field.
  uint64_t
  entsize() const
  { return this->entsize_; }

  // Set the entsize field.
  void
  set_entsize(uint64_t v);

  // Set the link field to the output section index of a section.
  void
  set_link_section(const Output_data* od)
  {
    gold_assert(this->link_ == 0
		&& !this->should_link_to_symtab_
		&& !this->should_link_to_dynsym_);
    this->link_section_ = od;
  }

  // Set the link field to a constant.
  void
  set_link(unsigned int v)
  {
    gold_assert(this->link_section_ == NULL
		&& !this->should_link_to_symtab_
		&& !this->should_link_to_dynsym_);
    this->link_ = v;
  }

  // Record that this section should link to the normal symbol table.
  void
  set_should_link_to_symtab()
  {
    gold_assert(this->link_section_ == NULL
		&& this->link_ == 0
		&& !this->should_link_to_dynsym_);
    this->should_link_to_symtab_ = true;
  }

  // Record that this section should link to the dynamic symbol table.
  void
  set_should_link_to_dynsym()
  {
    gold_assert(this->link_section_ == NULL
		&& this->link_ == 0
		&& !this->should_link_to_symtab_);
    this->should_link_to_dynsym_ = true;
  }

  // Return the info field.
  unsigned int
  info() const
  {
    gold_assert(this->info_section_ == NULL);
    return this->info_;
  }

  // Set the info field to the output section index of a section.
  void
  set_info_section(const Output_data* od)
  {
    gold_assert(this->info_ == 0);
    this->info_section_ = od;
  }

  // Set the info field to a constant.
  void
  set_info(unsigned int v)
  {
    gold_assert(this->info_section_ == NULL);
    this->info_ = v;
  }

  // Set the addralign field.
  void
  set_addralign(uint64_t v)
  { this->addralign_ = v; }

  // Indicate that we need a symtab index.
  void
  set_needs_symtab_index()
  { this->needs_symtab_index_ = true; }

  // Return whether we need a symtab index.
  bool
  needs_symtab_index() const
  { return this->needs_symtab_index_; }

  // Get the symtab index.
  unsigned int
  symtab_index() const
  {
    gold_assert(this->symtab_index_ != 0);
    return this->symtab_index_;
  }

  // Set the symtab index.
  void
  set_symtab_index(unsigned int index)
  {
    gold_assert(index != 0);
    this->symtab_index_ = index;
  }

  // Indicate that we need a dynsym index.
  void
  set_needs_dynsym_index()
  { this->needs_dynsym_index_ = true; }

  // Return whether we need a dynsym index.
  bool
  needs_dynsym_index() const
  { return this->needs_dynsym_index_; }

  // Get the dynsym index.
  unsigned int
  dynsym_index() const
  {
    gold_assert(this->dynsym_index_ != 0);
    return this->dynsym_index_;
  }

  // Set the dynsym index.
  void
  set_dynsym_index(unsigned int index)
  {
    gold_assert(index != 0);
    this->dynsym_index_ = index;
  }

  // Return the output virtual address of OFFSET relative to the start
  // of input section SHNDX in object OBJECT.
  uint64_t
  output_address(const Relobj* object, unsigned int shndx,
		 off_t offset) const;

  // Set the address of the Output_section.  For a typical
  // Output_section, there is nothing to do, but if there are any
  // Output_section_data objects we need to set the final addresses
  // here.
  void
  do_set_address(uint64_t, off_t);

  // Write the data to the file.  For a typical Output_section, this
  // does nothing: the data is written out by calling Object::Relocate
  // on each input object.  But if there are any Output_section_data
  // objects we do need to write them out here.
  void
  do_write(Output_file*);

  // Return the address alignment--function required by parent class.
  uint64_t
  do_addralign() const
  { return this->addralign_; }

  // Return whether this is an Output_section.
  bool
  do_is_section() const
  { return true; }

  // Return whether this is a section of the specified type.
  bool
  do_is_section_type(elfcpp::Elf_Word type) const
  { return this->type_ == type; }

  // Return whether the specified section flag is set.
  bool
  do_is_section_flag_set(elfcpp::Elf_Xword flag) const
  { return (this->flags_ & flag) != 0; }

  // Write the section header into *OPHDR.
  template<int size, bool big_endian>
  void
  write_header(const Layout*, const Stringpool*,
	       elfcpp::Shdr_write<size, big_endian>*) const;

 private:
  // In some cases we need to keep a list of the input sections
  // associated with this output section.  We only need the list if we
  // might have to change the offsets of the input section within the
  // output section after we add the input section.  The ordinary
  // input sections will be written out when we process the object
  // file, and as such we don't need to track them here.  We do need
  // to track Output_section_data objects here.  We store instances of
  // this structure in a std::vector, so it must be a POD.  There can
  // be many instances of this structure, so we use a union to save
  // some space.
  class Input_section
  {
   public:
    Input_section()
      : shndx_(0), p2align_(0)
    {
      this->u1_.data_size = 0;
      this->u2_.object = NULL;
    }

    // For an ordinary input section.
    Input_section(Relobj* object, unsigned int shndx, off_t data_size,
		  uint64_t addralign)
      : shndx_(shndx),
	p2align_(ffsll(static_cast<long long>(addralign)))
    {
      gold_assert(shndx != OUTPUT_SECTION_CODE
		  && shndx != MERGE_DATA_SECTION_CODE
		  && shndx != MERGE_STRING_SECTION_CODE);
      this->u1_.data_size = data_size;
      this->u2_.object = object;
    }

    // For a non-merge output section.
    Input_section(Output_section_data* posd)
      : shndx_(OUTPUT_SECTION_CODE),
	p2align_(ffsll(static_cast<long long>(posd->addralign())))
    {
      this->u1_.data_size = 0;
      this->u2_.posd = posd;
    }

    // For a merge section.
    Input_section(Output_section_data* posd, bool is_string, uint64_t entsize)
      : shndx_(is_string
	       ? MERGE_STRING_SECTION_CODE
	       : MERGE_DATA_SECTION_CODE),
	p2align_(ffsll(static_cast<long long>(posd->addralign())))
    {
      this->u1_.entsize = entsize;
      this->u2_.posd = posd;
    }

    // The required alignment.
    uint64_t
    addralign() const
    {
      return (this->p2align_ == 0
	      ? 0
	      : static_cast<uint64_t>(1) << (this->p2align_ - 1));
    }

    // Return the required size.
    off_t
    data_size() const;

    // Return whether this is a merge section which matches the
    // parameters.
    bool
    is_merge_section(bool is_string, uint64_t entsize) const
    {
      return (this->shndx_ == (is_string
			       ? MERGE_STRING_SECTION_CODE
			       : MERGE_DATA_SECTION_CODE)
	      && this->u1_.entsize == entsize);
    }

    // Set the output section.
    void
    set_output_section(Output_section* os)
    {
      gold_assert(!this->is_input_section());
      this->u2_.posd->set_output_section(os);
    }

    // Set the address and file offset.  This is called during
    // Layout::finalize.  SECOFF is the file offset of the enclosing
    // section.
    void
    set_address(uint64_t addr, off_t off, off_t secoff);

    // Add an input section, for SHF_MERGE sections.
    bool
    add_input_section(Relobj* object, unsigned int shndx)
    {
      gold_assert(this->shndx_ == MERGE_DATA_SECTION_CODE
		  || this->shndx_ == MERGE_STRING_SECTION_CODE);
      return this->u2_.posd->add_input_section(object, shndx);
    }

    // Given an input OBJECT, an input section index SHNDX within that
    // object, and an OFFSET relative to the start of that input
    // section, return whether or not the output address is known.
    // OUTPUT_SECTION_ADDRESS is the address of the output section
    // which this is a part of.  If this function returns true, it
    // sets *POUTPUT to the output address.
    bool
    output_address(const Relobj* object, unsigned int shndx, off_t offset,
		   uint64_t output_section_address, uint64_t *poutput) const;

    // Write out the data.  This does nothing for an input section.
    void
    write(Output_file*);

   private:
    // Code values which appear in shndx_.  If the value is not one of
    // these codes, it is the input section index in the object file.
    enum
    {
      // An Output_section_data.
      OUTPUT_SECTION_CODE = -1U,
      // An Output_section_data for an SHF_MERGE section with
      // SHF_STRINGS not set.
      MERGE_DATA_SECTION_CODE = -2U,
      // An Output_section_data for an SHF_MERGE section with
      // SHF_STRINGS set.
      MERGE_STRING_SECTION_CODE = -3U
    };

    // Whether this is an input section.
    bool
    is_input_section() const
    {
      return (this->shndx_ != OUTPUT_SECTION_CODE
	      && this->shndx_ != MERGE_DATA_SECTION_CODE
	      && this->shndx_ != MERGE_STRING_SECTION_CODE);
    }

    // For an ordinary input section, this is the section index in the
    // input file.  For an Output_section_data, this is
    // OUTPUT_SECTION_CODE or MERGE_DATA_SECTION_CODE or
    // MERGE_STRING_SECTION_CODE.
    unsigned int shndx_;
    // The required alignment, stored as a power of 2.
    unsigned int p2align_;
    union
    {
      // For an ordinary input section, the section size.
      off_t data_size;
      // For OUTPUT_SECTION_CODE, this is not used.  For
      // MERGE_DATA_SECTION_CODE or MERGE_STRING_SECTION_CODE, the
      // entity size.
      uint64_t entsize;
    } u1_;
    union
    {
      // For an ordinary input section, the object which holds the
      // input section.
      Relobj* object;
      // For OUTPUT_SECTION_CODE or MERGE_DATA_SECTION_CODE or
      // MERGE_STRING_SECTION_CODE, the data.
      Output_section_data* posd;
    } u2_;
  };

  typedef std::vector<Input_section> Input_section_list;

  // Add a new output section by Input_section.
  void
  add_output_section_data(Input_section*);

  // Add an SHF_MERGE input section.  Returns true if the section was
  // handled.
  bool
  add_merge_input_section(Relobj* object, unsigned int shndx, uint64_t flags,
			  uint64_t entsize, uint64_t addralign);

  // Add an output SHF_MERGE section POSD to this output section.
  // IS_STRING indicates whether it is a SHF_STRINGS section, and
  // ENTSIZE is the entity size.  This returns the entry added to
  // input_sections_.
  void
  add_output_merge_section(Output_section_data* posd, bool is_string,
			   uint64_t entsize);

  // Most of these fields are only valid after layout.

  // The name of the section.  This will point into a Stringpool.
  const char* name_;
  // The section address is in the parent class.
  // The section alignment.
  uint64_t addralign_;
  // The section entry size.
  uint64_t entsize_;
  // The file offset is in the parent class.
  // Set the section link field to the index of this section.
  const Output_data* link_section_;
  // If link_section_ is NULL, this is the link field.
  unsigned int link_;
  // Set the section info field to the index of this section.
  const Output_data* info_section_;
  // If info_section_ is NULL, this is the section info field.
  unsigned int info_;
  // The section type.
  elfcpp::Elf_Word type_;
  // The section flags.
  elfcpp::Elf_Xword flags_;
  // The section index.
  unsigned int out_shndx_;
  // If there is a STT_SECTION for this output section in the normal
  // symbol table, this is the symbol index.  This starts out as zero.
  // It is initialized in Layout::finalize() to be the index, or -1U
  // if there isn't one.
  unsigned int symtab_index_;
  // If there is a STT_SECTION for this output section in the dynamic
  // symbol table, this is the symbol index.  This starts out as zero.
  // It is initialized in Layout::finalize() to be the index, or -1U
  // if there isn't one.
  unsigned int dynsym_index_;
  // The input sections.  This will be empty in cases where we don't
  // need to keep track of them.
  Input_section_list input_sections_;
  // The offset of the first entry in input_sections_.
  off_t first_input_offset_;
  // Whether this output section needs a STT_SECTION symbol in the
  // normal symbol table.  This will be true if there is a relocation
  // which needs it.
  bool needs_symtab_index_ : 1;
  // Whether this output section needs a STT_SECTION symbol in the
  // dynamic symbol table.  This will be true if there is a dynamic
  // relocation which needs it.
  bool needs_dynsym_index_ : 1;
  // Whether the link field of this output section should point to the
  // normal symbol table.
  bool should_link_to_symtab_ : 1;
  // Whether the link field of this output section should point to the
  // dynamic symbol table.
  bool should_link_to_dynsym_ : 1;
};

// An output segment.  PT_LOAD segments are built from collections of
// output sections.  Other segments typically point within PT_LOAD
// segments, and are built directly as needed.

class Output_segment
{
 public:
  // Create an output segment, specifying the type and flags.
  Output_segment(elfcpp::Elf_Word, elfcpp::Elf_Word);

  // Return the virtual address.
  uint64_t
  vaddr() const
  { return this->vaddr_; }

  // Return the physical address.
  uint64_t
  paddr() const
  { return this->paddr_; }

  // Return the segment type.
  elfcpp::Elf_Word
  type() const
  { return this->type_; }

  // Return the segment flags.
  elfcpp::Elf_Word
  flags() const
  { return this->flags_; }

  // Return the memory size.
  uint64_t
  memsz() const
  { return this->memsz_; }

  // Return the file size.
  off_t
  filesz() const
  { return this->filesz_; }

  // Return the maximum alignment of the Output_data.
  uint64_t
  addralign();

  // Add an Output_section to this segment.
  void
  add_output_section(Output_section* os, elfcpp::Elf_Word seg_flags)
  { this->add_output_section(os, seg_flags, false); }

  // Add an Output_section to the start of this segment.
  void
  add_initial_output_section(Output_section* os, elfcpp::Elf_Word seg_flags)
  { this->add_output_section(os, seg_flags, true); }

  // Add an Output_data (which is not an Output_section) to the start
  // of this segment.
  void
  add_initial_output_data(Output_data*);

  // Set the address of the segment to ADDR and the offset to *POFF
  // (aligned if necessary), and set the addresses and offsets of all
  // contained output sections accordingly.  Set the section indexes
  // of all contained output sections starting with *PSHNDX.  Return
  // the address of the immediately following segment.  Update *POFF
  // and *PSHNDX.  This should only be called for a PT_LOAD segment.
  uint64_t
  set_section_addresses(uint64_t addr, off_t* poff, unsigned int* pshndx);

  // Set the offset of this segment based on the section.  This should
  // only be called for a non-PT_LOAD segment.
  void
  set_offset();

  // Return the number of output sections.
  unsigned int
  output_section_count() const;

  // Write the segment header into *OPHDR.
  template<int size, bool big_endian>
  void
  write_header(elfcpp::Phdr_write<size, big_endian>*);

  // Write the section headers of associated sections into V.
  template<int size, bool big_endian>
  unsigned char*
  write_section_headers(const Layout*, const Stringpool*, unsigned char* v,
			unsigned int* pshndx ACCEPT_SIZE_ENDIAN) const;

 private:
  Output_segment(const Output_segment&);
  Output_segment& operator=(const Output_segment&);

  typedef std::list<Output_data*> Output_data_list;

  // Add an Output_section to this segment, specifying front or back.
  void
  add_output_section(Output_section*, elfcpp::Elf_Word seg_flags,
		     bool front);

  // Find the maximum alignment in an Output_data_list.
  static uint64_t
  maximum_alignment(const Output_data_list*);

  // Set the section addresses in an Output_data_list.
  uint64_t
  set_section_list_addresses(Output_data_list*, uint64_t addr, off_t* poff,
			     unsigned int* pshndx);

  // Return the number of Output_sections in an Output_data_list.
  unsigned int
  output_section_count_list(const Output_data_list*) const;

  // Write the section headers in the list into V.
  template<int size, bool big_endian>
  unsigned char*
  write_section_headers_list(const Layout*, const Stringpool*,
			     const Output_data_list*, unsigned char* v,
			     unsigned int* pshdx ACCEPT_SIZE_ENDIAN) const;

  // The list of output data with contents attached to this segment.
  Output_data_list output_data_;
  // The list of output data without contents attached to this segment.
  Output_data_list output_bss_;
  // The segment virtual address.
  uint64_t vaddr_;
  // The segment physical address.
  uint64_t paddr_;
  // The size of the segment in memory.
  uint64_t memsz_;
  // The segment alignment.
  uint64_t align_;
  // The offset of the segment data within the file.
  off_t offset_;
  // The size of the segment data in the file.
  off_t filesz_;
  // The segment type;
  elfcpp::Elf_Word type_;
  // The segment flags.
  elfcpp::Elf_Word flags_;
  // Whether we have set align_.
  bool is_align_known_;
};

// This class represents the output file.

class Output_file
{
 public:
  Output_file(const General_options& options);

  // Open the output file.  FILE_SIZE is the final size of the file.
  void
  open(off_t file_size);

  // Close the output file and make sure there are no error.
  void
  close();

  // We currently always use mmap which makes the view handling quite
  // simple.  In the future we may support other approaches.

  // Write data to the output file.
  void
  write(off_t offset, const void* data, off_t len)
  { memcpy(this->base_ + offset, data, len); }

  // Get a buffer to use to write to the file, given the offset into
  // the file and the size.
  unsigned char*
  get_output_view(off_t start, off_t size)
  {
    gold_assert(start >= 0 && size >= 0 && start + size <= this->file_size_);
    return this->base_ + start;
  }

  // VIEW must have been returned by get_output_view.  Write the
  // buffer to the file, passing in the offset and the size.
  void
  write_output_view(off_t, off_t, unsigned char*)
  { }

 private:
  // General options.
  const General_options& options_;
  // File name.
  const char* name_;
  // File descriptor.
  int o_;
  // File size.
  off_t file_size_;
  // Base of file mapped into memory.
  unsigned char* base_;
};

} // End namespace gold.

#endif // !defined(GOLD_OUTPUT_H)

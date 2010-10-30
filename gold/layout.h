// layout.h -- lay out output file sections for gold  -*- C++ -*-

#ifndef GOLD_LAYOUT_H
#define GOLD_LAYOUT_H

#include <list>
#include <string>
#include <utility>
#include <vector>

#include "workqueue.h"
#include "object.h"
#include "dynobj.h"
#include "stringpool.h"

namespace gold
{

class General_options;
class Input_objects;
class Symbol_table;
class Output_section_data;
class Output_section;
class Output_section_headers;
class Output_segment;
class Output_data;
class Output_data_dynamic;
class Target;

// This task function handles mapping the input sections to output
// sections and laying them out in memory.

class Layout_task_runner : public Task_function_runner
{
 public:
  // OPTIONS is the command line options, INPUT_OBJECTS is the list of
  // input objects, SYMTAB is the symbol table, LAYOUT is the layout
  // object.
  Layout_task_runner(const General_options& options,
		     const Input_objects* input_objects,
		     Symbol_table* symtab,
		     Layout* layout)
    : options_(options), input_objects_(input_objects), symtab_(symtab),
      layout_(layout)
  { }

  // Run the operation.
  void
  run(Workqueue*);

 private:
  Layout_task_runner(const Layout_task_runner&);
  Layout_task_runner& operator=(const Layout_task_runner&);

  const General_options& options_;
  const Input_objects* input_objects_;
  Symbol_table* symtab_;
  Layout* layout_;
};

// This class handles the details of laying out input sections.

class Layout
{
 public:
  Layout(const General_options& options);

  // Given an input section SHNDX, named NAME, with data in SHDR, from
  // the object file OBJECT, return the output section where this
  // input section should go.  Set *OFFSET to the offset within the
  // output section.
  template<int size, bool big_endian>
  Output_section*
  layout(Relobj *object, unsigned int shndx, const char* name,
	 const elfcpp::Shdr<size, big_endian>& shdr, off_t* offset);

  // Add an Output_section_data to the layout.  This is used for
  // special sections like the GOT section.
  void
  add_output_section_data(const char* name, elfcpp::Elf_Word type,
			  elfcpp::Elf_Xword flags,
			  Output_section_data*);

  // Create dynamic sections if necessary.
  void
  create_initial_dynamic_sections(const Input_objects*, Symbol_table*);

  // Return the Stringpool used for symbol names.
  const Stringpool*
  sympool() const
  { return &this->sympool_; }

  // Return the Stringpool used for dynamic symbol names and dynamic
  // tags.
  const Stringpool*
  dynpool() const
  { return &this->dynpool_; }

  // Return whether a section is a .gnu.linkonce section, given the
  // section name.
  static inline bool
  is_linkonce(const char* name)
  { return strncmp(name, ".gnu.linkonce", sizeof(".gnu.linkonce") - 1) == 0; }

  // Record the signature of a comdat section, and return whether to
  // include it in the link.  The GROUP parameter is true for a
  // section group signature, false for a signature derived from a
  // .gnu.linkonce section.
  bool
  add_comdat(const char*, bool group);

  // Finalize the layout after all the input sections have been added.
  off_t
  finalize(const Input_objects*, Symbol_table*);

  // Return the TLS segment.  This will return NULL if there isn't
  // one.
  Output_segment*
  tls_segment() const
  { return this->tls_segment_; }

  // Return the normal symbol table.
  Output_section*
  symtab_section() const
  {
    gold_assert(this->symtab_section_ != NULL);
    return this->symtab_section_;
  }

  // Return the dynamic symbol table.
  Output_section*
  dynsym_section() const
  {
    gold_assert(this->dynsym_section_ != NULL);
    return this->dynsym_section_;
  }

  // Return the dynamic tags.
  Output_data_dynamic*
  dynamic_data() const
  { return this->dynamic_data_; }

  // Write out data not associated with an input file or the symbol
  // table.
  void
  write_data(const Symbol_table*, const Target*, Output_file*) const;

  // Return an output section named NAME, or NULL if there is none.
  Output_section*
  find_output_section(const char* name) const;

  // Return an output segment of type TYPE, with segment flags SET set
  // and segment flags CLEAR clear.  Return NULL if there is none.
  Output_segment*
  find_output_segment(elfcpp::PT type, elfcpp::Elf_Word set,
		      elfcpp::Elf_Word clear) const;

  // The list of segments.

  typedef std::vector<Output_segment*> Segment_list;

  // The list of sections not attached to a segment.

  typedef std::vector<Output_section*> Section_list;

  // The list of information to write out which is not attached to
  // either a section or a segment.
  typedef std::vector<Output_data*> Data_list;

 private:
  Layout(const Layout&);
  Layout& operator=(const Layout&);

  // Mapping from .gnu.linkonce section names to output section names.
  struct Linkonce_mapping
  {
    const char* from;
    int fromlen;
    const char* to;
    int tolen;
  };
  static const Linkonce_mapping linkonce_mapping[];
  static const int linkonce_mapping_count;

  // Find the first read-only PT_LOAD segment, creating one if
  // necessary.
  Output_segment*
  find_first_load_seg();

  // Create the output sections for the symbol table.
  void
  create_symtab_sections(int size, const Input_objects*, Symbol_table*,
			 off_t*);

  // Create the .shstrtab section.
  Output_section*
  create_shstrtab();

  // Create the section header table.
  Output_section_headers*
  create_shdrs(int size, bool big_endian, off_t*);

  // Create the dynamic symbol table.
  void
  create_dynamic_symtab(const Target*, Symbol_table*, Output_section** pdynstr,
			unsigned int* plocal_dynamic_count,
			std::vector<Symbol*>* pdynamic_symbols,
			Versions* versions);

  // Finish the .dynamic section and PT_DYNAMIC segment.
  void
  finish_dynamic_section(const Input_objects*, const Symbol_table*);

  // Create the .interp section and PT_INTERP segment.
  void
  create_interp(const Target* target);

  // Create the version sections.
  void
  create_version_sections(const Target*, const Versions*,
			  unsigned int local_symcount,
			  const std::vector<Symbol*>& dynamic_symbols,
			  const Output_section* dynstr);

  template<int size, bool big_endian>
  void
  sized_create_version_sections(const Versions* versions,
				unsigned int local_symcount,
				const std::vector<Symbol*>& dynamic_symbols,
				const Output_section* dynstr
                                ACCEPT_SIZE_ENDIAN);

  // Return whether to include this section in the link.
  template<int size, bool big_endian>
  bool
  include_section(Object* object, const char* name,
		  const elfcpp::Shdr<size, big_endian>&);

  // Return the output section name to use given an input section
  // name.  Set *PLEN to the length of the name.  *PLEN must be
  // initialized to the length of NAME.
  static const char*
  output_section_name(const char* name, size_t* plen);

  // Return the output section name to use for a linkonce section
  // name.  PLEN is as for output_section_name.
  static const char*
  linkonce_output_name(const char* name, size_t* plen);

  // Return the output section for NAME, TYPE and FLAGS.
  Output_section*
  get_output_section(const char* name, Stringpool::Key name_key,
		     elfcpp::Elf_Word type, elfcpp::Elf_Xword flags);

  // Create a new Output_section.
  Output_section*
  make_output_section(const char* name, elfcpp::Elf_Word type,
		      elfcpp::Elf_Xword flags);

  // Set the final file offsets of all the segments.
  off_t
  set_segment_offsets(const Target*, Output_segment*, unsigned int* pshndx);

  // Set the final file offsets and section indexes of all the
  // sections not associated with a segment.
  off_t
  set_section_offsets(off_t, unsigned int *pshndx);

  // Return whether SEG1 comes before SEG2 in the output file.
  static bool
  segment_precedes(const Output_segment* seg1, const Output_segment* seg2);

  // Map from section flags to segment flags.
  static elfcpp::Elf_Word
  section_flags_to_segment(elfcpp::Elf_Xword flags);

  // A mapping used for group signatures.
  typedef Unordered_map<std::string, bool> Signatures;

  // Mapping from input section name/type/flags to output section.  We
  // use canonicalized strings here.

  typedef std::pair<Stringpool::Key,
		    std::pair<elfcpp::Elf_Word, elfcpp::Elf_Xword> > Key;

  struct Hash_key
  {
    size_t
    operator()(const Key& k) const;
  };

  typedef Unordered_map<Key, Output_section*, Hash_key> Section_name_map;

  // A comparison class for segments.

  struct Compare_segments
  {
    bool
    operator()(const Output_segment* seg1, const Output_segment* seg2)
    { return Layout::segment_precedes(seg1, seg2); }
  };

  // A reference to the options on the command line.
  const General_options& options_;
  // The output section names.
  Stringpool namepool_;
  // The output symbol names.
  Stringpool sympool_;
  // The dynamic strings, if needed.
  Stringpool dynpool_;
  // The list of group sections and linkonce sections which we have seen.
  Signatures signatures_;
  // The mapping from input section name/type/flags to output sections.
  Section_name_map section_name_map_;
  // The list of output segments.
  Segment_list segment_list_;
  // The list of output sections.
  Section_list section_list_;
  // The list of output sections which are not attached to any output
  // segment.
  Section_list unattached_section_list_;
  // The list of unattached Output_data objects which require special
  // handling because they are not Output_sections.
  Data_list special_output_list_;
  // A pointer to the PT_TLS segment if there is one.
  Output_segment* tls_segment_;
  // The SHT_SYMTAB output section.
  Output_section* symtab_section_;
  // The SHT_DYNSYM output section if there is one.
  Output_section* dynsym_section_;
  // The SHT_DYNAMIC output section if there is one.
  Output_section* dynamic_section_;
  // The dynamic data which goes into dynamic_section_.
  Output_data_dynamic* dynamic_data_;
};

// This task handles writing out data which is not part of a section
// or segment.

class Write_data_task : public Task
{
 public:
  Write_data_task(const Layout* layout, const Symbol_table* symtab,
		  const Target* target, Output_file* of,
		  Task_token* final_blocker)
    : layout_(layout), symtab_(symtab), target_(target), of_(of),
      final_blocker_(final_blocker)
  { }

  // The standard Task methods.

  Is_runnable_type
  is_runnable(Workqueue*);

  Task_locker*
  locks(Workqueue*);

  void
  run(Workqueue*);

 private:
  const Layout* layout_;
  const Symbol_table* symtab_;
  const Target* target_;
  Output_file* of_;
  Task_token* final_blocker_;
};

// This task handles writing out the global symbols.

class Write_symbols_task : public Task
{
 public:
  Write_symbols_task(const Symbol_table* symtab, const Target* target,
		     const Stringpool* sympool, const Stringpool* dynpool,
		     Output_file* of, Task_token* final_blocker)
    : symtab_(symtab), target_(target), sympool_(sympool), dynpool_(dynpool),
      of_(of), final_blocker_(final_blocker)
  { }

  // The standard Task methods.

  Is_runnable_type
  is_runnable(Workqueue*);

  Task_locker*
  locks(Workqueue*);

  void
  run(Workqueue*);

 private:
  const Symbol_table* symtab_;
  const Target* target_;
  const Stringpool* sympool_;
  const Stringpool* dynpool_;
  Output_file* of_;
  Task_token* final_blocker_;
};

// This task function handles closing the file.

class Close_task_runner : public Task_function_runner
{
 public:
  Close_task_runner(Output_file* of)
    : of_(of)
  { }

  // Run the operation.
  void
  run(Workqueue*);

 private:
  Output_file* of_;
};

// A small helper function to align an address.

inline uint64_t
align_address(uint64_t address, uint64_t addralign)
{
  if (addralign != 0)
    address = (address + addralign - 1) &~ (addralign - 1);
  return address;
}

} // End namespace gold.

#endif // !defined(GOLD_LAYOUT_H)

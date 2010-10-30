// dynobj.h -- dynamic object support for gold   -*- C++ -*-

#ifndef GOLD_DYNOBJ_H
#define GOLD_DYNOBJ_H

#include <vector>

#include "stringpool.h"
#include "object.h"

namespace gold
{

class General_options;

// A dynamic object (ET_DYN).  This is an abstract base class itself.
// The implementations is the template class Sized_dynobj.

class Dynobj : public Object
{
 public:
  Dynobj(const std::string& name, Input_file* input_file, off_t offset = 0)
    : Object(name, input_file, true, offset), soname_()
  { }

  // Return the name to use in a DT_NEEDED entry for this object.
  const char*
  soname() const;

  // Compute the ELF hash code for a string.
  static uint32_t
  elf_hash(const char*);

  // Create a standard ELF hash table, setting *PPHASH and *PHASHLEN.
  // DYNSYMS is the global dynamic symbols.  LOCAL_DYNSYM_COUNT is the
  // number of local dynamic symbols, which is the index of the first
  // dynamic gobal symbol.
  static void
  create_elf_hash_table(const Target*, const std::vector<Symbol*>& dynsyms,
			unsigned int local_dynsym_count,
			unsigned char** pphash,
			unsigned int* phashlen);

  // Create a GNU hash table, setting *PPHASH and *PHASHLEN.  DYNSYMS
  // is the global dynamic symbols.  LOCAL_DYNSYM_COUNT is the number
  // of local dynamic symbols, which is the index of the first dynamic
  // gobal symbol.
  static void
  create_gnu_hash_table(const Target*, const std::vector<Symbol*>& dynsyms,
			unsigned int local_dynsym_count,
			unsigned char** pphash, unsigned int* phashlen);

 protected:
  // Set the DT_SONAME string.
  void
  set_soname_string(const char* s)
  { this->soname_.assign(s); }

 private:
  // Compute the GNU hash code for a string.
  static uint32_t
  gnu_hash(const char*);

  // Compute the number of hash buckets to use.
  static unsigned int
  compute_bucket_count(const std::vector<uint32_t>& hashcodes,
		       bool for_gnu_hash_table);

  // Sized version of create_elf_hash_table.
  template<bool big_endian>
  static void
  sized_create_elf_hash_table(const std::vector<uint32_t>& bucket,
			      const std::vector<uint32_t>& chain,
			      unsigned char* phash,
			      unsigned int hashlen);

  // Sized version of create_gnu_hash_table.
  template<int size, bool big_endian>
  static void
  sized_create_gnu_hash_table(const std::vector<Symbol*>& hashed_dynsyms,
			      const std::vector<uint32_t>& dynsym_hashvals,
			      unsigned int unhashed_dynsym_count,
			      unsigned char** pphash,
			      unsigned int* phashlen);

  // The DT_SONAME name, if any.
  std::string soname_;
};

// A dynamic object, size and endian specific version.

template<int size, bool big_endian>
class Sized_dynobj : public Dynobj
{
 public:
  Sized_dynobj(const std::string& name, Input_file* input_file, off_t offset,
	       const typename elfcpp::Ehdr<size, big_endian>&);

  // Set up the object file based on the ELF header.
  void
  setup(const typename elfcpp::Ehdr<size, big_endian>&);

  // Read the symbols.
  void
  do_read_symbols(Read_symbols_data*);

  // Lay out the input sections.
  void
  do_layout(const General_options&, Symbol_table*, Layout*,
	    Read_symbols_data*);

  // Add the symbols to the symbol table.
  void
  do_add_symbols(Symbol_table*, Read_symbols_data*);

  // Get the name of a section.
  std::string
  do_section_name(unsigned int shndx)
  { return this->elf_file_.section_name(shndx); }

  // Return a view of the contents of a section.  Set *PLEN to the
  // size.
  Object::Location
  do_section_contents(unsigned int shndx)
  { return this->elf_file_.section_contents(shndx); }

  // Return section flags.
  uint64_t
  do_section_flags(unsigned int shndx)
  { return this->elf_file_.section_flags(shndx); }

 private:
  // For convenience.
  typedef Sized_dynobj<size, big_endian> This;
  static const int shdr_size = elfcpp::Elf_sizes<size>::shdr_size;
  static const int sym_size = elfcpp::Elf_sizes<size>::sym_size;
  static const int dyn_size = elfcpp::Elf_sizes<size>::dyn_size;
  typedef elfcpp::Shdr<size, big_endian> Shdr;
  typedef elfcpp::Dyn<size, big_endian> Dyn;

  // Find the dynamic symbol table and the version sections, given the
  // section headers.
  void
  find_dynsym_sections(const unsigned char* pshdrs,
		       unsigned int* pdynshm_shndx,
		       unsigned int* pversym_shndx,
		       unsigned int* pverdef_shndx,
		       unsigned int* pverneed_shndx,
		       unsigned int* pdynamic_shndx);

  // Read the dynamic symbol section SHNDX.
  void
  read_dynsym_section(const unsigned char* pshdrs, unsigned int shndx,
		      elfcpp::SHT type, unsigned int link,
		      File_view** view, off_t* view_size,
		      unsigned int* view_info);

  // Set the SONAME from the SHT_DYNAMIC section at DYNAMIC_SHNDX.
  // The STRTAB parameters may have the relevant string table.
  void
  set_soname(const unsigned char* pshdrs, unsigned int dynamic_shndx,
	     unsigned int strtab_shndx, const unsigned char* strtabu,
	     off_t strtab_size);

  // Mapping from version number to version name.
  typedef std::vector<const char*> Version_map;

  // Create the version map.
  void
  make_version_map(Read_symbols_data* sd, Version_map*) const;

  // Add version definitions to the version map.
  void
  make_verdef_map(Read_symbols_data* sd, Version_map*) const;

  // Add version references to the version map.
  void
  make_verneed_map(Read_symbols_data* sd, Version_map*) const;

  // Add an entry to the version map.
  void
  set_version_map(Version_map*, unsigned int ndx, const char* name) const;

  // General access to the ELF file.
  elfcpp::Elf_file<size, big_endian, Object> elf_file_;
};

// A base class for Verdef and Verneed_version which just handles the
// version index which will be stored in the SHT_GNU_versym section.

class Version_base
{
 public:
  Version_base()
    : index_(-1U)
  { }

  virtual
  ~Version_base()
  { }

  // Return the version index.
  unsigned int
  index() const
  {
    gold_assert(this->index_ != -1U);
    return this->index_;
  }

  // Set the version index.
  void
  set_index(unsigned int index)
  {
    gold_assert(this->index_ == -1U);
    this->index_ = index;
  }

  // Clear the weak flag in a version definition.
  virtual void
  clear_weak() = 0;

 private:
  Version_base(const Version_base&);
  Version_base& operator=(const Version_base&);

  // The index of the version definition or reference.
  unsigned int index_;
};

// This class handles a version being defined in the file we are
// generating.

class Verdef : public Version_base
{
 public:
  Verdef(const char* name, bool is_base, bool is_weak, bool is_symbol_created)
    : name_(name), deps_(), is_base_(is_base), is_weak_(is_weak),
      is_symbol_created_(is_symbol_created)
  { }

  // Return the version name.
  const char*
  name() const
  { return this->name_; }

  // Return the number of dependencies.
  unsigned int
  count_dependencies() const
  { return this->deps_.size(); }

  // Add a dependency to this version.  The NAME should be
  // canonicalized in the dynamic Stringpool.
  void
  add_dependency(const char* name)
  { this->deps_.push_back(name); }

  // Return whether this definition is weak.
  bool
  is_weak() const
  { return this->is_weak_; }

  // Clear the weak flag.
  void
  clear_weak()
  { this->is_weak_ = false; }

  // Return whether a version symbol has been created for this
  // definition.
  bool
  is_symbol_created() const
  { return this->is_symbol_created_; }

  // Write contents to buffer.
  template<int size, bool big_endian>
  unsigned char*
  write(const Stringpool*, bool is_last, unsigned char*
        ACCEPT_SIZE_ENDIAN) const;

 private:
  Verdef(const Verdef&);
  Verdef& operator=(const Verdef&);

  // The type of the list of version dependencies.  Each dependency
  // should be canonicalized in the dynamic Stringpool.
  typedef std::vector<const char*> Deps;

  // The name of this version.  This should be canonicalized in the
  // dynamic Stringpool.
  const char* name_;
  // A list of other versions which this version depends upon.
  Deps deps_;
  // Whether this is the base version.
  bool is_base_;
  // Whether this version is weak.
  bool is_weak_;
  // Whether a version symbol has been created.
  bool is_symbol_created_;
};

// A referened version.  This will be associated with a filename by
// Verneed.

class Verneed_version : public Version_base
{
 public:
  Verneed_version(const char* version)
    : version_(version)
  { }

  // Return the version name.
  const char*
  version() const
  { return this->version_; }

  // Clear the weak flag.  This is invalid for a reference.
  void
  clear_weak()
  { gold_unreachable(); }

 private:
  Verneed_version(const Verneed_version&);
  Verneed_version& operator=(const Verneed_version&);

  const char* version_;
};

// Version references in a single dynamic object.

class Verneed
{
 public:
  Verneed(const char* filename)
    : filename_(filename), need_versions_()
  { }

  ~Verneed();

  // Return the file name.
  const char*
  filename() const
  { return this->filename_; }

  // Return the number of versions.
  unsigned int
  count_versions() const
  { return this->need_versions_.size(); }

  // Add a version name.  The name should be canonicalized in the
  // dynamic Stringpool.  If the name is already present, this does
  // nothing.
  Verneed_version*
  add_name(const char* name);

  // Set the version indexes, starting at INDEX.  Return the updated
  // INDEX.
  unsigned int
  finalize(unsigned int index);

  // Write contents to buffer.
  template<int size, bool big_endian>
  unsigned char*
  write(const Stringpool*, bool is_last, unsigned char*
        ACCEPT_SIZE_ENDIAN) const;

 private:
  Verneed(const Verneed&);
  Verneed& operator=(const Verneed&);

  // The type of the list of version names.  Each name should be
  // canonicalized in the dynamic Stringpool.
  typedef std::vector<Verneed_version*> Need_versions;

  // The filename of the dynamic object.  This should be
  // canonicalized in the dynamic Stringpool.
  const char* filename_;
  // The list of version names.
  Need_versions need_versions_;
};

// This class handles version definitions and references which go into
// the output file.

class Versions
{
 public:
  Versions()
    : defs_(), needs_(), version_table_(), is_finalized_(false)
  { }

  ~Versions();

  // SYM is going into the dynamic symbol table and has a version.
  // Record the appropriate version information.
  void
  record_version(const General_options*, Stringpool*, const Symbol* sym);

  // Set the version indexes.  DYNSYM_INDEX is the index we should use
  // for the next dynamic symbol.  We add new dynamic symbols to SYMS
  // and return an updated DYNSYM_INDEX.
  unsigned int
  finalize(const Target*, Symbol_table* symtab, unsigned int dynsym_index,
	   std::vector<Symbol*>* syms);

  // Return whether there are any version definitions.
  bool
  any_defs() const
  { return !this->defs_.empty(); }

  // Return whether there are any version references.
  bool
  any_needs() const
  { return !this->needs_.empty(); }

  // Build an allocated buffer holding the contents of the symbol
  // version section (.gnu.version).
  template<int size, bool big_endian>
  void
  symbol_section_contents(const Stringpool*, unsigned int local_symcount,
			  const std::vector<Symbol*>& syms,
			  unsigned char**, unsigned int*
                          ACCEPT_SIZE_ENDIAN) const;

  // Build an allocated buffer holding the contents of the version
  // definition section (.gnu.version_d).
  template<int size, bool big_endian>
  void
  def_section_contents(const Stringpool*, unsigned char**,
		       unsigned int* psize, unsigned int* pentries
                       ACCEPT_SIZE_ENDIAN) const;

  // Build an allocated buffer holding the contents of the version
  // reference section (.gnu.version_r).
  template<int size, bool big_endian>
  void
  need_section_contents(const Stringpool*, unsigned char**,
			unsigned int* psize, unsigned int* pentries
                        ACCEPT_SIZE_ENDIAN) const;

 private:
  // The type of the list of version definitions.
  typedef std::vector<Verdef*> Defs;

  // The type of the list of version references.
  typedef std::vector<Verneed*> Needs;

  // Handle a symbol SYM defined with version VERSION.
  void
  add_def(const General_options*, const Symbol* sym, const char* version,
	  Stringpool::Key);

  // Add a reference to version NAME in file FILENAME.
  void
  add_need(Stringpool*, const char* filename, const char* name,
	   Stringpool::Key);

  // Return the version index to use for SYM.
  unsigned int
  version_index(const Stringpool*, const Symbol* sym) const;

  // We keep a hash table mapping canonicalized name/version pairs to
  // a version base.
  typedef std::pair<Stringpool::Key, Stringpool::Key> Key;

  struct Version_table_hash
  {
    size_t
    operator()(const Key& k) const
    { return k.first + k.second; }
  };

  struct Version_table_eq
  {
    bool
    operator()(const Key& k1, const Key& k2) const
    { return k1.first == k2.first && k1.second == k2.second; }
  };

  typedef Unordered_map<Key, Version_base*, Version_table_hash,
			Version_table_eq> Version_table;

  // The version definitions.
  Defs defs_;
  // The version references.
  Needs needs_;
  // The mapping from a canonicalized version/filename pair to a
  // version index.  The filename may be NULL.
  Version_table version_table_;
  // Whether the version indexes have been set.
  bool is_finalized_;
};

} // End namespace gold.

#endif // !defined(GOLD_DYNOBJ_H)

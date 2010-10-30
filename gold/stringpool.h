// stringpool.h -- a string pool for gold    -*- C++ -*-

#include <string>
#include <list>

// Stringpool
//   Manage a pool of unique strings.

#ifndef GOLD_STRINGPOOL_H
#define GOLD_STRINGPOOL_H

namespace gold
{

class Output_file;

template<typename Stringpool_char>
class Stringpool_template
{
 public:
  // The type of a key into the stringpool.  A key value will always
  // be the same during any run of the linker.  The string pointers
  // may change when using address space randomization.  We use key
  // values in order to get repeatable runs when the value is inserted
  // into an unordered hash table.  Zero is never a valid key.
  typedef size_t Key;

  // Create a Stringpool.  ZERO_NULL is true if we should reserve
  // offset 0 to hold the empty string.
  Stringpool_template(bool zero_null = true);

  ~Stringpool_template();

  // Add a string to the pool.  This returns a canonical permanent
  // pointer to the string.  If PKEY is not NULL, this sets *PKEY to
  // the key for the string.
  const Stringpool_char*
  add(const Stringpool_char*, Key* pkey);

  const Stringpool_char*
  add(const std::basic_string<Stringpool_char>& s, Key* pkey)
  { return this->add(s.c_str(), pkey); }

  // Add the prefix of a string to the pool.
  const Stringpool_char*
  add(const Stringpool_char*, size_t, Key* pkey);

  // If a string is present, return the canonical string.  Otherwise,
  // return NULL.  If PKEY is not NULL, set *PKEY to the key.
  const Stringpool_char*
  find(const Stringpool_char*, Key* pkey) const;

  // Turn the stringpool into an ELF strtab: determine the offsets of
  // all the strings.
  void
  set_string_offsets();

  // Get the offset of a string in an ELF strtab.  This returns the
  // offset in bytes, not characters.
  off_t
  get_offset(const Stringpool_char*) const;

  off_t
  get_offset(const std::basic_string<Stringpool_char>& s) const
  { return this->get_offset(s.c_str()); }

  // Get the size of the ELF strtab.  This returns the number of
  // bytes, not characters.
  off_t
  get_strtab_size() const
  {
    gold_assert(this->strtab_size_ != 0);
    return this->strtab_size_;
  }

  // Write the strtab into the output file at the specified offset.
  void
  write(Output_file*, off_t offset);

 private:
  Stringpool_template(const Stringpool_template&);
  Stringpool_template& operator=(const Stringpool_template&);

  // Return the length of a string.
  static size_t
  string_length(const Stringpool_char*);

  // We store the actual data in a list of these buffers.
  struct Stringdata
  {
    // Length of data in buffer.
    size_t len;
    // Allocated size of buffer.
    size_t alc;
    // Buffer index.
    unsigned int index;
    // Buffer.
    char data[1];
  };

  // Copy a string into the buffers, returning a canonical string.
  const Stringpool_char*
  add_string(const Stringpool_char*, Key*);

  struct Stringpool_hash
  {
    size_t
    operator()(const Stringpool_char*) const;
  };

  struct Stringpool_eq
  {
    bool
    operator()(const Stringpool_char* p1, const Stringpool_char* p2) const;
  };

  // Return whether s1 is a suffix of s2.
  static bool
  is_suffix(const Stringpool_char* s1, const Stringpool_char* s2);

  // The hash table is a map from string names to a pair of Key and
  // ELF strtab offsets.  We only use the offsets if we turn this into
  // an ELF strtab section.

  typedef std::pair<Key, off_t> Val;

#ifdef HAVE_TR1_UNORDERED_SET
  typedef Unordered_map<const Stringpool_char*, Val, Stringpool_hash,
			Stringpool_eq,
			std::allocator<std::pair<const Stringpool_char* const,
						 Val> >,
			true> String_set_type;
#else
  typedef Unordered_map<const Stringpool_char*, Val, Stringpool_hash,
			Stringpool_eq> String_set_type;
#endif

  // Comparison routine used when sorting into an ELF strtab.

  struct Stringpool_sort_comparison
  {
    bool
    operator()(typename String_set_type::iterator,
	       typename String_set_type::iterator) const;
  };

  // List of Stringdata structures.
  typedef std::list<Stringdata*> Stringdata_list;

  // Mapping from const char* to namepool entry.
  String_set_type string_set_;
  // List of buffers.
  Stringdata_list strings_;
  // Size of ELF strtab.
  off_t strtab_size_;
  // Next Stringdata index.
  unsigned int next_index_;
  // Whether to reserve offset 0 to hold the null string.
  bool zero_null_;
};

// The most common type of Stringpool.
typedef Stringpool_template<char> Stringpool;

} // End namespace gold.

#endif // !defined(GOLD_STRINGPOOL_H)

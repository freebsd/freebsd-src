// merge.h -- handle section merging for gold  -*- C++ -*-

#ifndef GOLD_MERGE_H
#define GOLD_MERGE_H

#include <climits>

#include "stringpool.h"
#include "output.h"

namespace gold
{

// A general class for SHF_MERGE data, to hold functions shared by
// fixed-size constant data and string data.

class Output_merge_base : public Output_section_data
{
 public:
  Output_merge_base(uint64_t entsize)
    : Output_section_data(1), merge_map_(), entsize_(entsize)
  { }

  // Return the output address for an input address.
  bool
  do_output_address(const Relobj* object, unsigned int shndx, off_t offset,
		    uint64_t output_section_address, uint64_t* poutput) const;

 protected:
  // Return the entry size.
  uint64_t
  entsize() const
  { return this->entsize_; }

  // Add a mapping from an OFFSET in input section SHNDX in object
  // OBJECT to an OUTPUT_OFFSET in the output section.
  void
  add_mapping(Relobj* object, unsigned int shndx, off_t offset,
	      off_t output_offset);

 private:
  // We build a mapping from OBJECT/SHNDX/OFFSET to an offset in the
  // output section.
  struct Merge_key
  {
    const Relobj* object;
    unsigned int shndx;
    off_t offset;
  };

  struct Merge_key_less
  {
    bool
    operator()(const Merge_key&, const Merge_key&) const;
  };

  typedef std::map<Merge_key, off_t, Merge_key_less> Merge_map;

  // A mapping from input object/section/offset to offset in output
  // section.
  Merge_map merge_map_;

  // The entry size.  For fixed-size constants, this is the size of
  // the constants.  For strings, this is the size of a character.
  uint64_t entsize_;
};

// Handle SHF_MERGE sections with fixed-size constant data.

class Output_merge_data : public Output_merge_base
{
 public:
  Output_merge_data(uint64_t entsize)
    : Output_merge_base(entsize), p_(NULL), len_(0), alc_(0),
      hashtable_(128, Merge_data_hash(this), Merge_data_eq(this))
  { }

  // Add an input section.
  bool
  do_add_input_section(Relobj* object, unsigned int shndx);

  // Set the final data size.
  void
  do_set_address(uint64_t, off_t);

  // Write the data to the file.
  void
  do_write(Output_file*);

 private:
  // We build a hash table of the fixed-size constants.  Each constant
  // is stored as a pointer into the section data we are accumulating.

  // A key in the hash table.  This is an offset in the section
  // contents we are building.
  typedef off_t Merge_data_key;

  // Compute the hash code.  To do this we need a pointer back to the
  // object holding the data.
  class Merge_data_hash
  {
   public:
    Merge_data_hash(const Output_merge_data* pomd)
      : pomd_(pomd)
    { }

    size_t
    operator()(Merge_data_key) const;

   private:
    const Output_merge_data* pomd_;
  };

  friend class Merge_data_hash;

  // Compare two entries in the hash table for equality.  To do this
  // we need a pointer back to the object holding the data.  Note that
  // we now have a pointer to the object stored in two places in the
  // hash table.  Fixing this would require specializing the hash
  // table, which would be hard to do portably.
  class Merge_data_eq
  {
   public:
    Merge_data_eq(const Output_merge_data* pomd)
      : pomd_(pomd)
    { }

    bool
    operator()(Merge_data_key k1, Merge_data_key k2) const;

   private:
    const Output_merge_data* pomd_;
  };

  friend class Merge_data_eq;

  // The type of the hash table.
  typedef Unordered_set<Merge_data_key, Merge_data_hash, Merge_data_eq>
    Merge_data_hashtable;

  // Given a hash table key, which is just an offset into the section
  // data, return a pointer to the corresponding constant.
  const unsigned char*
  constant(Merge_data_key k) const
  {
    gold_assert(k >= 0 && k < this->len_);
    return this->p_ + k;
  }

  // Add a constant to the output.
  void
  add_constant(const unsigned char*);

  // The accumulated data.
  unsigned char* p_;
  // The length of the accumulated data.
  off_t len_;
  // The size of the allocated buffer.
  size_t alc_;
  // The hash table.
  Merge_data_hashtable hashtable_;
};

// Handle SHF_MERGE sections with string data.  This is a template
// based on the type of the characters in the string.

template<typename Char_type>
class Output_merge_string : public Output_merge_base
{
 public:
  Output_merge_string()
    : Output_merge_base(sizeof(Char_type)), stringpool_(false), hashtable_()
  { }

  // Add an input section.
  bool
  do_add_input_section(Relobj* object, unsigned int shndx);

  // Set the final data size.
  void
  do_set_address(uint64_t, off_t);

  // Write the data to the file.
  void
  do_write(Output_file*);

 private:
  // As we see input sections, we build a mapping from object, section
  // index and offset to strings.
  struct Merge_string_key
  {
    Relobj* object;
    unsigned int shndx;
    off_t offset;

    Merge_string_key(Relobj *objecta, unsigned int shndxa, off_t offseta)
      : object(objecta), shndx(shndxa), offset(offseta)
    { }
  };

  struct Merge_string_key_hash
  {
    size_t
    operator()(const Merge_string_key&) const;
  };

  struct Merge_string_key_eq
  {
    bool
    operator()(const Merge_string_key&, const Merge_string_key&) const;
  };

  typedef Unordered_map<Merge_string_key, const Char_type*,
			Merge_string_key_hash, Merge_string_key_eq>
    Merge_string_hashtable;

  // As we see the strings, we add them to a Stringpool.
  Stringpool_template<Char_type> stringpool_;
  // Map from a location in an input object to an entry in the
  // Stringpool.
  Merge_string_hashtable hashtable_;
};

} // End namespace gold.

#endif // !defined(GOLD_MERGE_H)

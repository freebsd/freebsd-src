// merge.cc -- handle section merging for gold

#include "gold.h"

#include <cstdlib>

#include "merge.h"

namespace gold
{

// Sort the entries in a merge mapping.  The key is an input object, a
// section index in that object, and an offset in that section.

bool
Output_merge_base::Merge_key_less::operator()(const Merge_key& mk1,
					      const Merge_key& mk2) const
{
  // The order of different objects and different sections doesn't
  // matter.  We want to get consistent results across links so we
  // don't use pointer comparison.
  if (mk1.object != mk2.object)
    return mk1.object->name() < mk2.object->name();
  if (mk1.shndx != mk2.shndx)
    return mk1.shndx < mk2.shndx;
  return mk1.offset < mk2.offset;
}

// Add a mapping from an OFFSET in input section SHNDX in object
// OBJECT to an OUTPUT_OFFSET in a merged output section.  This
// manages the mapping used to resolve relocations against merged
// sections.

void
Output_merge_base::add_mapping(Relobj* object, unsigned int shndx,
			       off_t offset, off_t output_offset)
{
  Merge_key mk;
  mk.object = object;
  mk.shndx = shndx;
  mk.offset = offset;
  std::pair<Merge_map::iterator, bool> ins =
    this->merge_map_.insert(std::make_pair(mk, output_offset));
  gold_assert(ins.second);
}

// Return the output address for an input address.  The input address
// is at offset OFFSET in section SHNDX in OBJECT.
// OUTPUT_SECTION_ADDRESS is the address of the output section.  If we
// know the address, set *POUTPUT and return true.  Otherwise return
// false.

bool
Output_merge_base::do_output_address(const Relobj* object, unsigned int shndx,
				     off_t offset,
				     uint64_t output_section_address,
				     uint64_t* poutput) const
{
  gold_assert(output_section_address == this->address());

  Merge_key mk;
  mk.object = object;
  mk.shndx = shndx;
  mk.offset = offset;
  Merge_map::const_iterator p = this->merge_map_.lower_bound(mk);

  // If MK is not in the map, lower_bound returns the next iterator
  // larger than it.
  if (p->first.object != object
      || p->first.shndx != shndx
      || p->first.offset != offset)
    {
      if (p == this->merge_map_.begin())
	return false;
      --p;
    }

  if (p->first.object != object || p->first.shndx != shndx)
    return false;

  // Any input section is fully mapped: we don't need to know the size
  // of the range starting at P->FIRST.OFFSET.
  *poutput = output_section_address + p->second + (offset - p->first.offset);
  return true;
}

// Compute the hash code for a fixed-size constant.

size_t
Output_merge_data::Merge_data_hash::operator()(Merge_data_key k) const
{
  const unsigned char* p = this->pomd_->constant(k);
  uint64_t entsize = this->pomd_->entsize();

  // Fowler/Noll/Vo (FNV) hash (type FNV-1a).
  if (sizeof(size_t) == 8)
    {
      size_t result = static_cast<size_t>(14695981039346656037ULL);
      for (uint64_t i = 0; i < entsize; ++i)
	{
	  result &= (size_t) *p++;
	  result *= 1099511628211ULL;
	}
      return result;
    }
  else
    {
      size_t result = 2166136261UL;
      for (uint64_t i = 0; i < entsize; ++i)
	{
	  result ^= (size_t) *p++;
	  result *= 16777619UL;
	}
      return result;
    }
}

// Return whether one hash table key equals another.

bool
Output_merge_data::Merge_data_eq::operator()(Merge_data_key k1,
					     Merge_data_key k2) const
{
  const unsigned char* p1 = this->pomd_->constant(k1);
  const unsigned char* p2 = this->pomd_->constant(k2);
  return memcmp(p1, p2, this->pomd_->entsize()) == 0;
}

// Add a constant to the end of the section contents.

void
Output_merge_data::add_constant(const unsigned char* p)
{
  uint64_t entsize = this->entsize();
  if (this->len_ + entsize > this->alc_)
    {
      if (this->alc_ == 0)
	this->alc_ = 128 * entsize;
      else
	this->alc_ *= 2;
      this->p_ = static_cast<unsigned char*>(realloc(this->p_, this->alc_));
      if (this->p_ == NULL)
	gold_fatal("out of memory", true);
    }

  memcpy(this->p_ + this->len_, p, entsize);
  this->len_ += entsize;
}

// Add the input section SHNDX in OBJECT to a merged output section
// which holds fixed length constants.  Return whether we were able to
// handle the section; if not, it will be linked as usual without
// constant merging.

bool
Output_merge_data::do_add_input_section(Relobj* object, unsigned int shndx)
{
  off_t len;
  const unsigned char* p = object->section_contents(shndx, &len);

  uint64_t entsize = this->entsize();

  if (len % entsize != 0)
    return false;

  for (off_t i = 0; i < len; i += entsize, p += entsize)
    {
      // Add the constant to the section contents.  If we find that it
      // is already in the hash table, we will remove it again.
      Merge_data_key k = this->len_;
      this->add_constant(p);

      std::pair<Merge_data_hashtable::iterator, bool> ins =
	this->hashtable_.insert(k);

      if (!ins.second)
	{
	  // Key was already present.  Remove the copy we just added.
	  this->len_ -= entsize;
	  k = *ins.first;
	}

      // Record the offset of this constant in the output section.
      this->add_mapping(object, shndx, i, k);
    }

  return true;
}

// Set the final data size in a merged output section with fixed size
// constants.

void
Output_merge_data::do_set_address(uint64_t, off_t)
{
  // Release the memory we don't need.
  this->p_ = static_cast<unsigned char*>(realloc(this->p_, this->len_));
  gold_assert(this->p_ != NULL);
  this->set_data_size(this->len_);
}

// Write the data of a merged output section with fixed size constants
// to the file.

void
Output_merge_data::do_write(Output_file* of)
{
  of->write(this->offset(), this->p_, this->len_);
}

// Compute a hash code for a Merge_string_key, which is an object, a
// section index, and an offset.

template<typename Char_type>
size_t
Output_merge_string<Char_type>::Merge_string_key_hash::operator()(
    const Merge_string_key& key) const
{
  // This is a very simple minded hash code.  Fix it if it we get too
  // many collisions.
  const std::string& oname(key.object->name());
  return oname[0] + oname.length() + key.shndx + key.offset;
}

// Compare two Merge_string_keys for equality.

template<typename Char_type>
bool
Output_merge_string<Char_type>::Merge_string_key_eq::operator()(
    const Merge_string_key& k1, const Merge_string_key& k2) const
{
  return (k1.object == k2.object
	  && k1.shndx == k2.shndx
	  && k1.offset == k2.offset);
}

// Add an input section to a merged string section.

template<typename Char_type>
bool
Output_merge_string<Char_type>::do_add_input_section(Relobj* object,
						     unsigned int shndx)
{
  off_t len;
  const unsigned char* pdata = object->section_contents(shndx, &len);

  const Char_type* p = reinterpret_cast<const Char_type*>(pdata);

  if (len % sizeof(Char_type) != 0)
    {
      fprintf(stderr,
	      _("%s: %s: mergeable string section length not multiple of "
		"character size\n"),
	      program_name, object->name().c_str());
      gold_exit(false);
    }
  len /= sizeof(Char_type);

  off_t i = 0;
  while (i < len)
    {
      off_t plen = 0;
      for (const Char_type* pl = p; *pl != 0; ++pl)
	{
	  ++plen;
	  if (i + plen >= len)
	    {
	      fprintf(stderr,
		      _("%s: %s: entry in mergeable string section "
			"not null terminated\n"),
		      program_name, object->name().c_str());
	      gold_exit(false);
	    }
	}

      const Char_type* str = this->stringpool_.add(p, NULL);

      Merge_string_key k(object, shndx, i);
      typename Merge_string_hashtable::value_type v(k, str);
      bool b = this->hashtable_.insert(v).second;
      gold_assert(b);

      p += plen + 1;
      i += plen + 1;
    }

  return true;
}

// Set the final data size of a merged string section.  This is where
// we finalize the mappings from the input sections to the output
// section.

template<typename Char_type>
void
Output_merge_string<Char_type>::do_set_address(uint64_t, off_t)
{
  this->stringpool_.set_string_offsets();

  for (typename Merge_string_hashtable::const_iterator p =
	 this->hashtable_.begin();
       p != this->hashtable_.end();
       ++p)
    this->add_mapping(p->first.object, p->first.shndx, p->first.offset,
		      this->stringpool_.get_offset(p->second));

  this->set_data_size(this->stringpool_.get_strtab_size());

  // Save some memory.
  this->hashtable_.clear();
}

// Write out a merged string section.

template<typename Char_type>
void
Output_merge_string<Char_type>::do_write(Output_file* of)
{
  this->stringpool_.write(of, this->offset());
}

// Instantiate the templates we need.

template
class Output_merge_string<char>;

template
class Output_merge_string<uint16_t>;

template
class Output_merge_string<uint32_t>;

} // End namespace gold.

// stringpool.cc -- a string pool for gold

#include "gold.h"

#include <cstring>
#include <algorithm>
#include <vector>

#include "output.h"
#include "stringpool.h"

namespace gold
{

template<typename Stringpool_char>
Stringpool_template<Stringpool_char>::Stringpool_template(bool zero_null)
  : string_set_(), strings_(), strtab_size_(0), next_index_(1),
    zero_null_(zero_null)
{
}

template<typename Stringpool_char>
Stringpool_template<Stringpool_char>::~Stringpool_template()
{
  for (typename std::list<Stringdata*>::iterator p = this->strings_.begin();
       p != this->strings_.end();
       ++p)
    delete[] reinterpret_cast<char*>(*p);
}

// Return the length of a string of arbitrary character type.

template<typename Stringpool_char>
size_t
Stringpool_template<Stringpool_char>::string_length(const Stringpool_char* p)
{
  size_t len = 0;
  for (; *p != 0; ++p)
    ++len;
  return len;
}

// Specialize string_length for char.  Maybe we could just use
// std::char_traits<>::length?

template<>
inline size_t
Stringpool_template<char>::string_length(const char* p)
{
  return strlen(p);
}

// Equality comparison function.

template<typename Stringpool_char>
bool
Stringpool_template<Stringpool_char>::Stringpool_eq::operator()(
    const Stringpool_char* s1,
    const Stringpool_char* s2) const
{
  while (*s1 != 0)
    if (*s1++ != *s2++)
      return false;
  return *s2 == 0;
}

// Specialize equality comparison for char.

template<>
bool
Stringpool_template<char>::Stringpool_eq::operator()(const char* s1,
						     const char* s2) const
{
  return strcmp(s1, s2) == 0;
}

// Hash function.

template<typename Stringpool_char>
size_t
Stringpool_template<Stringpool_char>::Stringpool_hash::operator()(
    const Stringpool_char* s) const
{
  // Fowler/Noll/Vo (FNV) hash (type FNV-1a).
  if (sizeof(size_t) == 8)
    {
      size_t result = static_cast<size_t>(14695981039346656037ULL);
      while (*s != 0)
	{
	  const char* p = reinterpret_cast<const char*>(s);
	  for (size_t i = 0; i < sizeof(Stringpool_char); ++i)
	    {
	      result &= (size_t) *p++;
	      result *= 1099511628211ULL;
	    }
	  ++s;
	}
      return result;
    }
  else
    {
      size_t result = 2166136261UL;
      while (*s != 0)
	{
	  const char* p = reinterpret_cast<const char*>(s);
	  for (size_t i = 0; i < sizeof(Stringpool_char); ++i)
	    {
	      result ^= (size_t) *p++;
	      result *= 16777619UL;
	    }
	  ++s;
	}
      return result;
    }
}

// Add a string to the list of canonical strings.  Return a pointer to
// the canonical string.  If PKEY is not NULL, set *PKEY to the key.

template<typename Stringpool_char>
const Stringpool_char*
Stringpool_template<Stringpool_char>::add_string(const Stringpool_char* s,
						 Key* pkey)
{
  // We are in trouble if we've already computed the string offsets.
  gold_assert(this->strtab_size_ == 0);

  // The size we allocate for a new Stringdata.
  const size_t buffer_size = 1000;
  // The amount we multiply the Stringdata index when calculating the
  // key.
  const size_t key_mult = 1024;
  gold_assert(key_mult >= buffer_size);

  size_t len = (string_length(s) + 1) * sizeof(Stringpool_char);

  size_t alc;
  bool front = true;
  if (len > buffer_size)
    {
      alc = sizeof(Stringdata) + len;
      front = false;
    }
  else if (this->strings_.empty())
    alc = sizeof(Stringdata) + buffer_size;
  else
    {
      Stringdata *psd = this->strings_.front();
      if (len > psd->alc - psd->len)
	alc = sizeof(Stringdata) + buffer_size;
      else
	{
	  char* ret = psd->data + psd->len;
	  memcpy(ret, s, len);

	  if (pkey != NULL)
	    *pkey = psd->index * key_mult + psd->len;

	  psd->len += len;

	  return reinterpret_cast<const Stringpool_char*>(ret);
	}
    }

  Stringdata *psd = reinterpret_cast<Stringdata*>(new char[alc]);
  psd->alc = alc - sizeof(Stringdata);
  memcpy(psd->data, s, len);
  psd->len = len;
  psd->index = this->next_index_;
  ++this->next_index_;

  if (pkey != NULL)
    *pkey = psd->index * key_mult;

  if (front)
    this->strings_.push_front(psd);
  else
    this->strings_.push_back(psd);

  return reinterpret_cast<const Stringpool_char*>(psd->data);
}

// Add a string to a string pool.

template<typename Stringpool_char>
const Stringpool_char*
Stringpool_template<Stringpool_char>::add(const Stringpool_char* s, Key* pkey)
{
  // FIXME: This will look up the entry twice in the hash table.  The
  // problem is that we can't insert S before we canonicalize it.  I
  // don't think there is a way to handle this correctly with
  // unordered_map, so this should be replaced with custom code to do
  // what we need, which is to return the empty slot.

  typename String_set_type::const_iterator p = this->string_set_.find(s);
  if (p != this->string_set_.end())
    {
      if (pkey != NULL)
	*pkey = p->second.first;
      return p->first;
    }

  Key k;
  const Stringpool_char* ret = this->add_string(s, &k);

  const off_t ozero = 0;
  std::pair<const Stringpool_char*, Val> element(ret,
						 std::make_pair(k, ozero));
  std::pair<typename String_set_type::iterator, bool> ins =
    this->string_set_.insert(element);
  gold_assert(ins.second);

  if (pkey != NULL)
    *pkey = k;

  return ret;
}

// Add a prefix of a string to a string pool.

template<typename Stringpool_char>
const Stringpool_char*
Stringpool_template<Stringpool_char>::add(const Stringpool_char* s, size_t len,
				 Key* pkey)
{
  // FIXME: This implementation should be rewritten when we rewrite
  // the hash table to avoid copying.
  std::basic_string<Stringpool_char> st(s, len);
  return this->add(st, pkey);
}

template<typename Stringpool_char>
const Stringpool_char*
Stringpool_template<Stringpool_char>::find(const Stringpool_char* s,
					   Key* pkey) const
{
  typename String_set_type::const_iterator p = this->string_set_.find(s);
  if (p == this->string_set_.end())
    return NULL;

  if (pkey != NULL)
    *pkey = p->second.first;

  return p->first;
}

// Comparison routine used when sorting into an ELF strtab.  We want
// to sort this so that when one string is a suffix of another, we
// always see the shorter string immediately after the longer string.
// For example, we want to see these strings in this order:
//   abcd
//   cd
//   d
// When strings are not suffixes, we don't care what order they are
// in, but we need to ensure that suffixes wind up next to each other.
// So we do a reversed lexicographic sort on the reversed string.

template<typename Stringpool_char>
bool
Stringpool_template<Stringpool_char>::Stringpool_sort_comparison::operator()(
  typename String_set_type::iterator it1,
  typename String_set_type::iterator it2) const
{
  const Stringpool_char* s1 = it1->first;
  const Stringpool_char* s2 = it2->first;
  size_t len1 = string_length(s1);
  size_t len2 = string_length(s2);
  size_t minlen = len1 < len2 ? len1 : len2;
  const Stringpool_char* p1 = s1 + len1 - 1;
  const Stringpool_char* p2 = s2 + len2 - 1;
  for (size_t i = minlen; i > 0; --i, --p1, --p2)
    {
      if (*p1 != *p2)
	return *p1 > *p2;
    }
  return len1 > len2;
}

// Return whether s1 is a suffix of s2.

template<typename Stringpool_char>
bool
Stringpool_template<Stringpool_char>::is_suffix(const Stringpool_char* s1,
						const Stringpool_char* s2)
{
  size_t len1 = string_length(s1);
  size_t len2 = string_length(s2);
  if (len1 > len2)
    return false;
  return memcmp(s1, s2 + len2 - len1, len1 * sizeof(Stringpool_char)) == 0;
}

// Turn the stringpool into an ELF strtab: determine the offsets of
// each string in the table.

template<typename Stringpool_char>
void
Stringpool_template<Stringpool_char>::set_string_offsets()
{
  if (this->strtab_size_ != 0)
    {
      // We've already computed the offsets.
      return;
    }

  size_t count = this->string_set_.size();

  std::vector<typename String_set_type::iterator> v;
  v.reserve(count);

  for (typename String_set_type::iterator p = this->string_set_.begin();
       p != this->string_set_.end();
       ++p)
    v.push_back(p);

  std::sort(v.begin(), v.end(), Stringpool_sort_comparison());

  const size_t charsize = sizeof(Stringpool_char);

  // Offset 0 may be reserved for the empty string.
  off_t offset = this->zero_null_ ? charsize : 0;
  for (size_t i = 0; i < count; ++i)
    {
      if (this->zero_null_ && v[i]->first[0] == 0)
	v[i]->second.second = 0;
      else if (i > 0 && is_suffix(v[i]->first, v[i - 1]->first))
	v[i]->second.second = (v[i - 1]->second.second
			       + ((string_length(v[i - 1]->first)
				   - string_length(v[i]->first))
				  * charsize));
      else
	{
	  v[i]->second.second = offset;
	  offset += (string_length(v[i]->first) + 1) * charsize;
	}
    }

  this->strtab_size_ = offset;
}

// Get the offset of a string in the ELF strtab.  The string must
// exist.

template<typename Stringpool_char>
off_t
Stringpool_template<Stringpool_char>::get_offset(const Stringpool_char* s)
  const
{
  gold_assert(this->strtab_size_ != 0);
  typename String_set_type::const_iterator p = this->string_set_.find(s);
  if (p != this->string_set_.end())
    return p->second.second;
  gold_unreachable();
}

// Write the ELF strtab into the output file at the specified offset.

template<typename Stringpool_char>
void
Stringpool_template<Stringpool_char>::write(Output_file* of, off_t offset)
{
  gold_assert(this->strtab_size_ != 0);
  unsigned char* viewu = of->get_output_view(offset, this->strtab_size_);
  char* view = reinterpret_cast<char*>(viewu);
  if (this->zero_null_)
    view[0] = '\0';
  for (typename String_set_type::const_iterator p = this->string_set_.begin();
       p != this->string_set_.end();
       ++p)
    memcpy(view + p->second.second, p->first,
	   (string_length(p->first) + 1) * sizeof(Stringpool_char));
  of->write_output_view(offset, this->strtab_size_, viewu);
}

// Instantiate the templates we need.

template
class Stringpool_template<char>;

template
class Stringpool_template<uint16_t>;

template
class Stringpool_template<uint32_t>;

} // End namespace gold.

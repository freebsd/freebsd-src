// strtab.h -- manage an ELF string table for gold   -*- C++ -*-

#ifndef GOLD_STRTAB_H
#define GOLD_STRTAB_H

#include <cstring>
#include <string>

namespace gold
{

// This class holds an ELF string table.  We keep a reference count
// for each string, which we use to determine which strings are
// actually required at the end.  When all operations are done, the
// string table is finalized, which sets the offsets to use for each
// string.

class Strtab
{
 public:
  Strtab();

  ~Strtab();

  Strtab_ref* add(const char*);

  Strtab_ref* add(const std::string& s)
  { return this->add(s.c_str()); }

 private:
  Strtab(const Strtab&);
  Strtab& operator=(const Strtab&);

  struct strtab_hash
  {
    std::size_t
    operator()(const char*p);
  };

  struct strtab_eq
  {
    bool
    operator()(const char* p1, const char* p2)
    { return strcmp(p1, p2) == 0; }
  };

  Unordered_map<const char*, Strtab_ref*, strtab_hash, strtab_eq,
		std::allocator<std::pair<const char* const, Strtab_ref*> >,
		true> strings_;
};

// Users of Strtab work with pointers to Strtab_ref structures.  These
// are allocated via new and should be deleted if the string is no
// longer needed.

class Strtab_ref
{
 public:
  ~Strtab_ref();

  const char*
  str() const;

 private:
  Strtab_ref(const Strtab_ref&);
  Strtab_ref& operator=(const Strtab_ref&);

  int refs_;
};

} // End namespace gold.

#endif // !defined(GOLD_STRTAB_H)

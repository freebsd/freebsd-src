// defstd.h -- define standard symbols for gold   -*- C++ -*-

#ifndef GOLD_DEFSTD_H
#define GOLD_DEFSTD_H

#include "symtab.h"

namespace gold
{

extern void
define_standard_symbols(Symbol_table*, const Layout*, Target*);

} // End namespace gold.

#endif // !defined(GOLD_DEFSTD_H)

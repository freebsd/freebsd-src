// script.h -- handle linker scripts for gold   -*- C++ -*-

// We implement a subset of the original GNU ld linker script language
// for compatibility.  The goal is not to implement the entire
// language.  It is merely to implement enough to handle common uses.
// In particular we need to handle /usr/lib/libc.so on a typical
// GNU/Linux system, and we want to handle linker scripts used by the
// Linux kernel build.

#ifndef GOLD_SCRIPT_H
#define GOLD_SCRIPT_H

namespace gold
{

class General_options;
class Symbol_table;
class Layout;
class Input_objects;
class Input_group;
class Input_file;
class Task_token;

// FILE was found as an argument on the command line, but was not
// recognized as an ELF file.  Try to read it as a script.  We've
// already read BYTES of data into P.  Return true if the file was
// handled.  This has to handle /usr/lib/libc.so on a GNU/Linux
// system.

bool
read_input_script(Workqueue*, const General_options&, Symbol_table*, Layout*,
		  const Dirsearch&, Input_objects*, Input_group*,
		  const Input_argument*, Input_file*, const unsigned char* p,
		  off_t bytes, Task_token* this_blocker,
		  Task_token* next_blocker);

} // End namespace gold.

#endif // !defined(GOLD_SCRIPT_H)

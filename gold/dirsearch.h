// dirsearch.h -- directory searching for gold  -*- C++ -*-

#ifndef GOLD_DIRSEARCH_H
#define GOLD_DIRSEARCH_H

#include <string>
#include <list>

#include "workqueue.h"

namespace gold
{

class General_options;

// A simple interface to manage directories to be searched for
// libraries.

class Dirsearch
{
 public:
  Dirsearch();

  // Add a directory to the search path.
  void
  add(Workqueue*, const char*);

  // Add a list of directories to the search path.
  void
  add(Workqueue*, const General_options::Dir_list&);

  // Search for a file, giving one or two names to search for (the
  // second one may be empty).  Return a full path name for the file,
  // or the empty string if it could not be found.  This may only be
  // called if the token is not blocked.
  std::string
  find(const std::string&, const std::string& n2 = std::string()) const;

  // Return a reference to the blocker token which controls access.
  const Task_token&
  token() const
  { return this->token_; }

 private:
  // We can not copy this class.
  Dirsearch(const Dirsearch&);
  Dirsearch& operator=(const Dirsearch&);

  // Directories to search.
  std::list<const char*> directories_;
  // Blocker token to control access from tasks.
  Task_token token_;
};

} // End namespace gold.

#endif // !defined(GOLD_DIRSEARCH_H)

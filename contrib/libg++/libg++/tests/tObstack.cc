// This may look like C code, but it is really -*- C++ -*-

/*
 a little test of Obstacks
 Thu Feb 18 11:16:28 1988  Doug Lea  (dl at rocky.oswego.edu)
*/

#include <assert.h>

#define tassert(ex) {if ((ex)) cerr << #ex << "\n"; \
                       else _assert(#ex, __FILE__,__LINE__); }

#include <stream.h>
#include <Obstack.h>
#include <stddef.h>
#include <ctype.h>

int
main()
{
  char*   s[10000];
  int     n = 0;
  int     got_one = 0;
  Obstack os;
  char    c;

  s[n++] = (char *)os.copy("\nunique words:");
  assert(os.OK());
  assert(os.contains(s[0]));

  cout << "enter anything at all, end with an EOF(^D)\n";

  while (cin.good() && n < 10000)
  {
    if (cin.get(c) && isalnum(c))
    {
      got_one = 1;
      os.grow(c);
    }
    else if (got_one)
    {
      char* current = (char *)os.finish(0);
      for (int i = 0; i < n; ++i) // stupid, but this is only a test.
      {
        if (strcmp(s[i], current) == 0)
        {
          os.free(current);
          current = 0;
          break;
        }
      }
      if (current != 0)
        s[n++] = current;
      got_one = 0;
    }
  }
  assert(os.OK());

  cout << s[0] << "\n";

  for (int i = n - 1; i > 0; -- i)
  {
    assert(os.contains(s[i]));
    cout << s[i] << "\n";
    os.free(s[i]);
  }

  assert(os.OK());
  assert(os.contains(s[0]));

  cout << "\n\nObstack vars:\n";
  cout << "alignment_mask = " << os.alignment_mask() << "\n";
  cout << "chunk_size = " << os.chunk_size() << "\n";
  cout << "size = " << os.size() << "\n";
  cout << "room + chunk overhead = " << os.room() + 2*sizeof (char*) << "\n";

  cout << "\nend of test\n";

  return 0;
}

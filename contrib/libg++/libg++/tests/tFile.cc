// This may look like C code, but it is really -*- C++ -*-

/*
 * a few tests for streams
 *
 */

#include <stream.h>
#ifndef _OLD_STREAMS
#include <strstream.h>
#include "unistd.h"
#endif
#include <SFile.h>
#include <PlotFile.h>

#include <std.h>
#include <assert.h>

class record
{
public:
  char c; int i; double d;
};

ostream& operator<<(ostream& s, record& r)
{
  return(s << "(i = " << r.i << " c = " << r.c << " d = " << r.d << ")");
}

void t1()
{
  char ch;

  assert(cout.good());
  assert(cout.writable());
  assert(cout.is_open());
  cout << "Hello, world via cout\n";
  assert(cerr.good());
  assert(cerr.writable());
  assert(cerr.is_open());
  cerr << "Hello, world via cerr\n";

  assert(cin.good());
  assert(cin.readable());
  assert(cin.is_open());

  cout << "enter a char:";  cin >> ch;
  cout.put('c');  cout.put(' ');  cout.put('=');  cout.put(' ');
  cout.put('"');  cout.put(ch);    cout << '"';  cout << char('\n');
  assert(cin.good());
  assert(cout.good());
}

void t2()
{
  int i;
  short h;
  long l;
  float f;
  double d;
  char s[100];

  cout << "enter three integers (short, int, long):";  
  cin >> h; cin >> i;   
  // cin.scan("%ld", &l);
  cin >> l;
  cout << "first  = " << h << " via dec = " << dec(h, 8) << "\n";
  cout << "second = " << i << form(" via form = %d = 0%o", i, i);
  cout.form(" via cout.form = %d = 0x%x\n", i, i);
  cout << "third  = " << l  << " via hex = " << hex(l) << "\n";
  assert(cin.good());
  assert(cout.good());

  cout << "enter a float then a double:";  cin >> f; cin >> d;
  cout << "first  = " << f << "\n";
  cout << "second = " << d << "\n";
  assert(cin.good());
  assert(cout.good());

  cout << "enter 5 characters separated with spaces:";  cin >> s;
  cout << "first  = " << s << "\n";
  cin.get(s, 100);
  cout << "rest   = " << s << "\n";

  assert(cin.good());
  assert(cout.good());

}

void t3()
{
  char ch;
  cout << "\nMaking streams sout and sin...";
#ifdef _OLD_STREAMS
  ostream sout("streamfile", io_writeonly, a_create);
#else
  ofstream sout("streamfile");
#endif
  assert(sout.good());
  assert(sout.is_open());
  assert(sout.writable());
  assert(!sout.readable());
  sout << "This file has one line testing output streams.\n";
  sout.close();
  assert(!sout.is_open());
#ifdef _OLD_STREAMS
  istream sin("streamfile", io_readonly, a_useonly);
#else
  ifstream sin("streamfile");
#endif
  assert(sin.good());
  assert(sin.is_open());
  assert(!sin.writable());
  assert(sin.readable());
  cout << "contents of file:\n";
  while(sin >> ch) cout << ch;
  sin.close();
  assert(!sin.is_open());
}


void t4()
{  
  char s[100];
  char ch;
  int i;

  cout << "\nMaking File tf ... "; 
#ifdef _OLD_STREAMS
  File tf("tempfile", io_readwrite, a_create);
#else
  fstream tf("tempfile", ios::in|ios::out|ios::trunc);
#endif
  assert(tf.good());
  assert(tf.is_open());
  assert(tf.writable());
  assert(tf.readable());
  strcpy(s, "This is the first and only line of this file.\n");
#ifdef _OLD_STREAMS
  tf.put(s);
  tf.seek(0);
#else
  tf << s;
  tf.rdbuf()->seekoff(0, ios::beg);
#endif
  tf.get(s, 100);
  assert(tf.good());
  cout << "first line of file:\n" << s << "\n";
  cout << "next char = ";
  tf.get(ch);
  cout << (int)ch;
  cout.put('\n');
  assert(ch == 10);
  strcpy(s, "Now there is a second line.\n");
  cout << "reopening tempfile, appending: " << s;
#ifdef _OLD_STREAMS
  tf.open(tf.name(), io_appendonly, a_use);
#else
  tf.close();
  tf.open("tempfile", ios::app);
#endif
  assert(tf.good());
  assert(tf.is_open());
  assert(tf.writable());
  assert(!tf.readable());
#ifdef _OLD_STREAMS
  tf.put(s);
  assert(tf.good());
  tf.open(tf.name(), io_readonly, a_use);
#else
  tf << s;
  assert(tf.good());
  tf.close();
  tf.open("tempfile", ios::in);
#endif
  tf.raw();
  assert(tf.good());
  assert(tf.is_open());
  assert(!tf.writable());
  assert(tf.readable());
  cout << "First 10 chars via raw system read after reopen for input:\n";
  read(tf.filedesc(), s, 10);
  assert(tf.good());
  for (i = 0; i < 10; ++ i)
    cout.put(s[i]);
  lseek(tf.filedesc(), 5, 0);
  cout << "\nContents after raw lseek to pos 5:\n";
  while ( (tf.get(ch)) && (cout.put(ch)) );
#ifdef _OLD_STREAMS
  tf.remove();
#else
  tf.close();
  unlink("tempfile");
#endif
  assert(!tf.is_open());
}

void t5()
{
  record r;
  int i;
  cout << "\nMaking SFile rf...";
#ifdef _OLD_STREAMS
  SFile rf("recfile", sizeof(record), io_readwrite, a_create);
#else
  SFile rf("recfile", sizeof(record), ios::in|ios::out|ios::trunc);
#endif
  assert(rf.good());
  assert(rf.is_open());
  assert(rf.writable());
  assert(rf.readable());
  for (i = 0; i < 10; ++i)
  {
    r.c = i + 'a';
    r.i = i;
    r.d = (double)(i) / 1000.0;
    rf.put(&r);
  }
  assert(rf.good());
  cout << "odd elements of file in reverse order:\n";
  for (i = 9; i >= 0; i -= 2)
  {
    rf[i].get(&r);
    assert(r.c == i + 'a');
    assert(r.i == i);
    cout << r << "\n";
  }
  assert(rf.good());
#ifdef _OLD_STREAMS
  rf.remove();
#else
  rf.close();
  unlink("recfile");
#endif
  assert(!rf.is_open());
}

void t6()
{
  cout << "\nMaking PlotFile pf ...";
  static const char plot_name[] = "plot.out";
  PlotFile pf(plot_name);
  assert(pf.good());
  assert(pf.is_open());
  assert(pf.writable());
  assert(!pf.readable());
  pf.move(10,10);
  pf.label("Test");
  pf.circle(300,300,200);
  pf.line(100, 100, 500, 500);
  assert(pf.good());
#ifdef _OLD_STREAMS
  cout << "(You may delete or attempt to plot " << pf.name() << ")\n";
#else
  cout << "(You may delete or attempt to plot " << plot_name << ")\n";
#endif
}

void t7()
{
  char ch;
  char mybuf[1000];
#ifdef _OLD_STREAMS
  cout << "creating string-based ostream...\n";
  ostream strout(1000, mybuf);
#else
  cout << "creating ostrstream...\n";
  ostrstream strout(mybuf, 1000);
#endif
  assert(strout.good());
  assert(strout.writable());
  strout << "This is a string-based stream.\n";
  strout << "With two lines.\n";
  strout.put(char(0));
  assert(strout.good());
  cout << "with contents:\n";
  cout << mybuf;
#ifdef _OLD_STREAMS
  cout << "using it to create string-based istream...\n";
  istream strin(strlen(mybuf), mybuf);
#else
  cout << "using it to create istrstream...\n";
  istrstream strin(mybuf, strlen(mybuf));
#endif
  assert(strin.good());
  assert(strin.readable());
  cout << "with contents:\n";
  while (strin.get(ch)) cout.put(ch);
}

void t8()
{
#ifdef _OLD_STREAMS
  cout << "\nThe following file open should generate error message:";
  cout.flush();
  File ef("shouldnotexist", io_readonly, a_useonly);
#else
  ifstream ef("shouldnotexist");
#endif
  assert(!ef.good());
  assert(!ef.is_open());
}

void t9()
{
  char ch;
  static char ffile_name[] = "ffile";
  {
      cout << "\nMaking filebuf streams fout and fin...";
      filebuf foutbuf;
#ifdef _OLD_STREAMS
      foutbuf.open(ffile_name, output);
#else
      foutbuf.open(ffile_name, ios::out);
#endif
      ostream fout(&foutbuf);
      assert(fout.good());
      assert(fout.is_open());
      assert(fout.writable());
      assert(!fout.readable());
      fout << "This file has one line testing output streams.\n";
#ifdef _OLD_STREAMS
      fout.close();
      assert(!fout.is_open());
#endif
  }
  filebuf finbuf;
#ifdef _OLD_STREAMS
  finbuf.open(ffile_name, input);
#else
  finbuf.open(ffile_name, ios::in);
#endif
  istream fin(&finbuf);
  assert(fin.good());
  assert(fin.is_open());
  assert(!fin.writable());
  assert(fin.readable());
  cout << "contents of file:\n";
  while(fin >> ch) cout << ch;
#ifndef _OLD_STREAMS
  cout << '\n';
#endif
  fin.close();
  assert(!fin.is_open());
}

main()
{
  t1();
  t2();
  t3();
  t4();
  t5();
  t6();
  t7();
  t9();
  t8(); 

  cout << "\nFinal names & states:\n";
#ifdef _OLD_STREAMS
  cout << "cin:      " << cin.name()  << "\t" << cin.rdstate() << "\n";
  cout << "cout:     " << cout.name() << "\t" << cout.rdstate() << "\n";
  cout << "cerr:     " << cerr.name() << "\t" << cerr.rdstate() << "\n";
#else
  cout << "cin:      " << "(stdin)"  << "\t" << cin.rdstate() << "\n";
  cout << "cout:     " << "(stdout)" << "\t" << cout.rdstate() << "\n";
  cout << "cerr:     " << "(stderr)" << "\t" << cerr.rdstate() << "\n";
#endif
  cout << "\nend of test.\n";
  return 0;
}

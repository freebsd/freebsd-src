#!/usr/bin/awk -f
#
# This file is in the public domain.  Written by Garrett A. Wollman,
# 2002-09-17.
#
# $FreeBSD$
#
BEGIN {
  state = 0;
}
/^%{$/ && state == 0 {
  state = 1;
  next;
}
/^%}$/ && state == 1 {
  state = 0;
  next;
}
state == 1 { print; next; }
/^%%$/ && state == 0 {
  state = 2;
  print "#include <stddef.h>";
  print "#include <string.h>";
  print "static const struct map {";
  print "\tconst char *name;";
  print "\tint key;";
  print "} wordlist[] = {";
  next;
}
/^%%$/ && state == 2 {
  state = 3;
  print "\t{ NULL }";
  print "};";
  print "#define\tNWORDS\t(sizeof(wordlist)/sizeof(wordlist[0]))";
  print "static const struct map *";
  print "in_word_set(const char *word, unsigned int len)";
  print "{";
  print "\tconst struct map *mp;";
  print "";
  print "\tfor (mp = wordlist; mp < &wordlist[NWORDS]; mp++) {";
  print "\t\tif (strcmp(word, mp->name) == 0)";
  print "\t\t\treturn (mp);";
  print "\t}";
  print "\treturn (NULL);";
  print "}";
  print "";
  next;
}
state == 2 && NF == 2 {
  name = substr($1, 1, length($1) - 1);
  printf "\t{ \"%s\", %s },\n", name, $2;
  next;
}
state == 3 { print; next; }
{
				# eat anything not matched.
}

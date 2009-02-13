"This tests arrays!
"
define p(a[],x,y) {
  auto i;
  for (i=x; i<y; i++) a[i];
}

define m(a[],x,y) {
  auto i;
  for (i=x; i<y; i++) a[i] = i;
}

define m1(*a[],x,y) {
  auto i;
  print "m1\n"
  for (i=x; i<y; i++) a[i] = i;
}

for (i=0; i<10; i++) a[i] = i;
j = p(a[],0,10);

j = m(b[],0,10);
j = p(b[],0,10);

print "---\n";
j = m1(b[],0,10);
j = p(b[],0,10);

quit


define t (x,y,d,s,t) {
   auto u, v, w, i, b, c;

   if (s >= t) {
     "Bad Scales. Try again.
";   return;
   }

   for (i = x; i < y; i += d) {
     scale = s;
     u = f(i);
     scale = t;
     v = f(i);
     scale = s;
     w = v / 1;
     b += 1;
     if (u != w) {
       c += 1;
"
Failed:  
"
       "  index = "; i;
       "  val1 = "; u;
       "  val2 = "; v;
"
"
     }
   }

"
Total tests:    "; b;
"
Total failures: "; c;
"
Percent failed: "; scale = 2; c*100/b;

}

/*
   b = begining scale value, 
   l = limit scale value,
   i = increment scale value.

   if b is set to a non-zero value before this file is executed,
   b, l and i are not reset.
*/

if (b == 0) { b = 10; l = 61; i = 10; }

"
Checking e(x)"
define f(x) {
  return (e(x))
}
for (s=10; s<l; s=s+i) {
"
scale = "; s
j = t(0,200,1,s,s+4)
}

"
Checking l(x)"
define f(x) {
  return (l(x))
}
for (s=10; s<l; s=s+i) {
"
scale = "; s
j = t(1,10000,25,s,s+4)
}

"
Checking s(x)"
define f(x) {
  return (s(x))
}
for (s=10; s<l; s=s+i) {
"
scale = "; s
j = t(0,8*a(1),.01,s,s+4)
}

"
Checking a(x)"
define f(x) {
  return (a(x))
}
for (s=10; s<l; s=s+i) {
"
scale = "; s
j = t(-1000,1000,10,s,s+4)
}

"
Checking j(n,x)"
define f(x) {
  return (j(n,x))
}
for (s=10; s<l; s=s+i) {
"
n=0, scale = "; s
n=0
j = t(0,30,.1,s,s+4)
"
n=1, scale = "; s
n=1
j = t(0,30,.1,s,s+4)
}


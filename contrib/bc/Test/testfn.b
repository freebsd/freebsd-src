/* This function "t" tests the function "f" to see if computing at
   two different scales has much effect on the accuracy. 
   test from f(x) to f(y) incrementing the index by d.  f(i) is
   computed at two scales, scale s and then scale t, where t>s.
   the result from scale t is divided by 1 at scale s and the
   results are compared.  If they are different, the function is
   said to have failed.  It will then print out the value of i
   (called index) and the two original values val1 (scale s) and
   val2 (scale t) */

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

program primes(output);
const n = 50; n1 = 7; (*n1 = sqrt(n)*)
var i,k,x,inc,lim,square,l: integer;
    prim: boolean;
    p,v: array[1..n1] of integer;
begin
   write(2:6, 3:6); l := 2;
   x := 1; inc := 4; lim := 1; square := 9;
   for i := 3 to n do
   begin (*find next prime*)
      repeat x := x + inc; inc := 6-inc;
         if square <= x then
            begin lim := lim+1;
               v[lim] := square; square := sqr(p[lim+1])
            end ;
         k := 2; prim := true;
         while prim and (k<lim) do
         begin k := k+1;
            if v[k] < x then v[k] := v[k] + 2*p[k];
            prim := x <> v[k]
         end
      until prim;
      if i <= n1 then p[i] := x;
      write(x:6); l := l+1;
      if l = 10 then
         begin writeln; l := 0
         end
   end ;
   writeln;
end .

define f (x) {

  if (x<=1) return(1)
  return (f(x-1)*x)
}

"Here we go"
for (a=1; a<100; a++) b+=f(a)/a
"
"
"b=";b
quit



/* An example that finds all primes between 2 and limit. */

define primes (limit) {
    auto num, p, root, i

    prime[1] = 2;
    prime[2] = 3;
    num = 2;
    scale = 0;

    for ( p=5; p <= limit; p += 2)  {
	root = sqrt(p);
	isprime = 1;
	for ( i = 1;  i < num && prime[i] <= root; i++ ) {
	    if ( p % prime[i] == 0 ) {
		isprime = 0;
		break;
            }
	}
	if (isprime) {
	    num += 1;
	    prime [num] = p;
	}
     }
}


print "\ntyping 'twins (10)' will print all twin primes less than 10.\n"

define twins (limit) {
   auto i;

   i = primes(limit+2);

   for (i=1; prime[i] > 0; i++) {
      if ((prime[i]+2) == prime[i+1]) \
	print "twins are ", prime[i], " and ", prime[i+1], "\n"
   }
}

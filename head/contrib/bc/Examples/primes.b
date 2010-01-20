
/* An example that finds all primes between 2 and limit. */

define primes (limit) {
    auto num, p, root, i

    prime[1] = 2;
    prime[2] = 3;
    num = 2;
    if (limit >= 2) print "prime 1 = 2\n"
    if (limit >= 3) print "prime 2 = 3\n";
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
	    print "prime ", num, " = ", p, "\n"
	}
     }
}


print "\ntyping 'primes (10)' will print all primes less than 10.\n"

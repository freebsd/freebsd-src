BEGIN{
  a[1] = "barz";
  a[2] = "blattt";
  a[3] = "Zebra";
  a[4] = 1234;

  testit1(a)

  delete a

  a[1] = "barz";
  a[2] = "blattt";
  a[3] = "Zebra";
  a[4] = 1234;

  n = asort(a, b);

  print "N = ", n;

  for(i=1; i <= n; i++)
    print i, a[i], b[i];
}

function testit1(a,	count, j)
{
	print "start testit"
	count = asort(a)
	for (j = 1; j <= count; j++)
		print j, a[j]
	print "end testit"
}

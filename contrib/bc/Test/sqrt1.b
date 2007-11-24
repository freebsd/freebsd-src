for (j=0; j<10; j++) {
  a = .9;
  b = .9+j;
  scale = 2;
  for (i=0; i<90; i++) {
    scale += 1;
    a /= 10;
    b += a;
    x = sqrt(b);
  }
  x;
}
quit

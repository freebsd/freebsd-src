BEGIN { b = 1; a[b] = 2; a[b++] += 1; print b,a[1] }

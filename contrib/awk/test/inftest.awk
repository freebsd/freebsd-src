BEGIN {
  x = 100
  do { y = x ; x *= 1000; print x,y } while ( y != x )
  print "loop terminated"
}

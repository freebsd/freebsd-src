/*
   This is a program to determine the distribution of digits in the
   fraction part of PI.   It will look at the first scale digits.

   The results are left in the global variable digits.
   digits[0] is the number of 0's in PI.

   This program requires the math library.
*/

define pi () {
  auto ix, pi, save_scale, work;

  save_scale = scale;
  scale += 5;
  print "\n\nCalculating PI to ",scale," digits.  Please wait . . .";
  pi = 4*a(1);
  scale -= 5;
  work = pi;

  print "\nCounting digits. . .";
  for (ix = 0; ix < 10; ix++) digits[ix] = 0;

  /* Extract the One's digit from pi. */
  scale = 0;
  one_digit = work / 1;

  for (ix = save_scale; ix > 0; ix--) {

    /* Remove the One's digit and multiply by 10. */
    scale = ix;
    work = (work - one_digit) / 1 * 10;

    /* Extract the One's digit. */
    scale = 0;
    one_digit = work / 1;

    digits[one_digit] += 1;
  }

  /* Restore the scale. */
  scale = save_scale;

  /* Report. */
  print "\n\n"
  print "PI to ", scale, " digits is:\n", pi/1, "\n\n"
  print "The frequency of the digits are:\n"
  for (ix = 0; ix < 10; ix++) {
    print "    ", ix, " - ", digits[ix], " times\n"
  }

  print "\n\n"
}

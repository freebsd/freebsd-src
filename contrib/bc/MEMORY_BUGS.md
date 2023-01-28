# Memory Bugs

This is a list of all of the memory bugs that were found in *released* versions
of `bc`, `dc`, or `bcl`. (Non-released commits with memory bugs do not count.)

I made this list for two reasons: first, so users can know what versions of
`bc`, `dc`, and `bcl` have vulnerabilities, and two, I once had a perfect record
and then found a couple, but forgot and claimed I still had a perfect record
right after, which was embarrassing.

This list is sorted by the first version a bug exists in, not the last it
existed in.

* In versions `1.1.0` until `6.2.0` (inclusive) of `bc` and `dc`, there is a
  out of bounds read and write in history when pressing ctrl+r (or any other
  unused letter) then inserting two characters.

  The first version without this bug is `6.2.1`.

* In versions `3.0.0` until `6.0.1` (inclusive) of `bc` and `dc`, there is a
  double-free on `SIGINT` when using command-line expressions with `-e` and
  `-f`. This was caused by not properly ending a jump series.

  The first version without this bug is `6.0.2`.

* In versions `5.0.0` until `6.0.4` (inclusive) of `bc`, there is an
  out-of-bounds access if a non-local (non-`auto`) variable is set to a string
  with `asciify()`, then the function is redefined with a use of the same
  non-local variable.

  This happened because strings were stored per-function, and the non-local
  variable now had a reference to the string in the old function, which could be
  at a higher index than exists in the new function. Strings are stored globally
  now, and they are *not* freed once not used.

  The first version without this bug is `6.1.0`.

* In versions `5.0.0` until `6.0.4` (inclusive) of `bc`, there is another
  out-of-bounds access if an array is passed to the `asciify()` built-in
  function as the only argument. This happened because arrays are allowed as
  function arguments, which allowed them to be used as arguments to `asciify()`,
  but they should not have been allowed. However, since they were, the
  `asciify()` code tried to access an argument that was not there.

  The first version without this bug is `6.1.0`.

* In version `6.0.0` of `bcl`, there are several uses of initialized data that
  have the same root cause: I forgot to call `memset()` on the per-thread global
  data. This is because the data used to be *actually* global, which meant that
  it was initialized to zero by the system. This happened because I thought I
  had properly hooked Valgrind into my `bcl` tests, but I had not.

  The first version without this bug is `6.0.1`.

#if defined (hpux) || defined (__alpha__)
/* HPUX lacks random().  DEC Alpha's random() returns a double.  */
static inline unsigned long
urandom ()
{
  return mrand48 ();
}
#else
long random ();

static inline unsigned long
urandom ()
{
  /* random() returns 31 bits, we want 32.  */
  return random() ^ (random() << 1);
}
#endif

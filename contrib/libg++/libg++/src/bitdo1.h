#ifndef ONES
#define ONES  ((_BS_word)(~0L))
#endif
  register int nwords;
  register _BS_word mask;
  if (offset == 0)
    ;
  else if (offset + length >= _BS_BITS_PER_WORD)
    {
      mask = ONES _BS_RIGHT offset;
      DOIT(*ptr++, mask);
      length -= _BS_BITS_PER_WORD - offset;
    }
  else
    {
      mask = (ONES _BS_RIGHT (_BS_BITS_PER_WORD - length))
	_BS_LEFT (_BS_BITS_PER_WORD - length - offset);
      DOIT(*ptr, mask);
      goto done;
    }
  nwords = _BS_INDEX(length);
  while (--nwords >= 0)
   {
     DOIT(*ptr++, ONES);
   }
  length = _BS_POS (length);
  if (length)
    {
      mask = ONES _BS_LEFT (_BS_BITS_PER_WORD - length);
      DOIT(*ptr, mask);
    }
 done: ;

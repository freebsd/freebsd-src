#ifndef ONES
#define ONES  ((_BS_word)(~0L))
#endif

#ifndef DOIT_SOLID
#ifdef DOIT
#define DOIT_SOLID(dst, src) DOIT(dst, src, (_BS_word)(~0))
#else
#define DOIT_SOLID(dst, src) (dst) = (COMBINE(dst, src))
#endif
#endif

#ifndef DOIT
#define DOIT(dst, src, mask) \
  (dst) = ((COMBINE(dst, src)) & (mask)) | ((dst) & ~(mask))
#endif

  _BS_word word0, mask;
  int shift0, shift1;

  if (length == 0)
    goto done;

  shift0 = srcbit - dstbit;

  /* First handle the case that only one destination word is touched. */
  if (length + dstbit <= _BS_BITS_PER_WORD)
    {
      _BS_word mask
	= (ONES _BS_LEFT (_BS_BITS_PER_WORD - length)) _BS_RIGHT dstbit;
      _BS_word word0 = *psrc++;
      if (shift0 <= 0)  /* dstbit >= srcbit */
        {
	  word0 = word0 _BS_RIGHT (-shift0);
	}
      else
	{
	  word0 = word0 _BS_LEFT shift0;
	  if (length + srcbit > _BS_BITS_PER_WORD)
	    word0 = word0 | (*psrc _BS_RIGHT (_BS_BITS_PER_WORD - shift0));
	}
      DOIT(*pdst, word0, mask);
      goto done;
    }

  /* Next optimize the case that the source and destination are aligned. */
  if (shift0 == 0)
    {
      _BS_word mask;
      if (psrc > pdst)
        {
	  if (srcbit)
	    {
	      mask = ONES _BS_RIGHT srcbit;
	      DOIT(*pdst, *psrc, mask);
	      pdst++; psrc++;
	      length -= _BS_BITS_PER_WORD - srcbit;
	    }
	  for (; length >= _BS_BITS_PER_WORD; length -= _BS_BITS_PER_WORD)
	    {
	      DOIT_SOLID(*pdst, *psrc);
	      pdst++;  psrc++;
	    }
	  if (length)
	    {
	      mask = ONES _BS_LEFT (_BS_BITS_PER_WORD - length);
	      DOIT(*pdst, *psrc, mask);
	    }
        }
      else if (psrc < pdst)
        {
	  _BS_size_t span = srcbit + length;
	  pdst += span / (_BS_size_t)_BS_BITS_PER_WORD;
	  psrc += span / (_BS_size_t)_BS_BITS_PER_WORD;
	  span %= (_BS_size_t)_BS_BITS_PER_WORD;
	  if (span)
	    {
	      mask = ONES _BS_LEFT (_BS_BITS_PER_WORD - span);
	      DOIT(*pdst, *psrc, mask);
	      length -= span;
	    }
	  pdst--;  psrc--;
	  for (; length >= _BS_BITS_PER_WORD; length -= _BS_BITS_PER_WORD)
	    {
	      DOIT_SOLID(*pdst, *psrc);
	      pdst--;  psrc--;
	    }
	  if (srcbit)
	    {
	      mask = ONES _BS_RIGHT srcbit;
	      DOIT(*pdst, *psrc, mask);
	    }
	}
      /* else if (psrc == pdst) --nothing to do--; */
      goto done;
    }

  /* Now we assume shift!=0, and more than on destination word is changed. */
  if (psrc >= pdst) /* Do the updates in forward direction. */
    {
      _BS_word word0 = *psrc++;
      _BS_word mask = ONES _BS_RIGHT dstbit;
      if (shift0 > 0)
        {
	  _BS_word word1 = *psrc++;
	  shift1 = _BS_BITS_PER_WORD - shift0;
	  DOIT(*pdst, (word0 _BS_LEFT shift0) | (word1 _BS_RIGHT shift1), mask);
	  word0 = word1;
        }
      else /* dstbit > srcbit */
        {
	  shift1 = -shift0;
	  shift0 += _BS_BITS_PER_WORD;
	  DOIT(*pdst, word0 _BS_RIGHT shift1, mask);
      }
      pdst++;
      length -= _BS_BITS_PER_WORD - dstbit;

      for ( ; length >= _BS_BITS_PER_WORD; length -= _BS_BITS_PER_WORD)
        {
	  register _BS_word word1 = *psrc++;
	  DOIT_SOLID(*pdst,
		     (word0 _BS_LEFT shift0) | (word1 _BS_RIGHT shift1));
	  pdst++;
	  word0 = word1;
        }
      if (length > 0)
        {
	  _BS_size_t mask = ONES _BS_LEFT (_BS_BITS_PER_WORD - length);
	  word0 = word0 _BS_LEFT shift0;
	  if (length > shift1)
	    word0 = word0 | (*psrc _BS_RIGHT shift1) ;
	  DOIT (*pdst, word0, mask);
        }
    }
  else /* Do the updates in backward direction. */
    {
      _BS_word word0;

      /* Make (psrc, srcbit) and (pdst, dstbit) point to *last* bit. */
      psrc += (srcbit + length  - 1) / _BS_BITS_PER_WORD;
      srcbit = (srcbit + length - 1) % _BS_BITS_PER_WORD;
      pdst += (dstbit + length - 1) / _BS_BITS_PER_WORD;
      dstbit = (dstbit + length - 1) % _BS_BITS_PER_WORD;

      shift0 = srcbit - dstbit;

      word0 = *psrc--;
      mask = ONES _BS_LEFT (_BS_BITS_PER_WORD - 1 - dstbit);
      if (shift0 < 0)
        {
	  _BS_word word1 = *psrc--;
	  shift1 = -shift0;
	  shift0 += _BS_BITS_PER_WORD;
	  DOIT (*pdst, (word0 _BS_RIGHT shift1) | (word1 _BS_LEFT shift0),
		mask);
	  word0 = word1;
        }
      else
        {
	  shift1 = _BS_BITS_PER_WORD - shift0;
	  DOIT(*pdst, word0 _BS_LEFT shift0, mask);
      }
      pdst--;
      length -= dstbit + 1;

      for ( ; length >= _BS_BITS_PER_WORD; length -= _BS_BITS_PER_WORD)
        {
	  register _BS_word word1 = *psrc--;
	  DOIT_SOLID(*pdst,
		     (word0 _BS_RIGHT shift1) | (word1 _BS_LEFT shift0));
	  pdst--;
	  word0 = word1;
        }
      if (length > 0)
        {
	  _BS_size_t mask = ONES _BS_RIGHT (_BS_BITS_PER_WORD - length);
	  word0 = word0 _BS_RIGHT shift1;
	  if (length > shift0)
	    word0 = word0 | (*psrc _BS_LEFT shift0) ;
	  DOIT (*pdst, word0, mask);
        }
    }
 done: ;

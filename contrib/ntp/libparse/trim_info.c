/*
 * /src/NTP/ntp-4/libparse/trim_info.c,v 4.2 1998/12/20 23:45:31 kardel RELEASE_19990228_A
 *
 * $Created: Sun Aug  2 20:20:34 1998 $
 *
 * Copyright (C) 1998 by Frank Kardel
 */
#include "ntp_types.h"
#include "trimble.h"

cmd_info_t *
trimble_convert(
		unsigned int cmd,
		cmd_info_t   *tbl
		)
{
  int i;

  for (i = 0; tbl[i].cmd != 0xFF; i++)
    {
      if (tbl[i].cmd == cmd)
	return &tbl[i];
    }
  return 0;
}

/*
 * trim_info.c,v
 * Revision 4.2  1998/12/20 23:45:31  kardel
 * fix types and warnings
 *
 * Revision 4.1  1998/08/09 22:27:48  kardel
 * Trimble TSIP support
 *
 */

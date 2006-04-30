
/*
 * THIS CODE IS SPECIFICALLY EXEMPTED FROM THE NCURSES PACKAGE COPYRIGHT.
 * You may freely copy it for use as a template for your own field types.
 * If you develop a field type that might be of general use, please send
 * it back to the ncurses maintainers for inclusion in the next version.
 */
/***************************************************************************
*                                                                          *
*  Author : Per Foreby, perf@efd.lth.se                                    *
*                                                                          *
***************************************************************************/

#include "form.priv.h"

MODULE_ID("$Id: fty_ipv4.c,v 1.4 2000/12/09 23:46:12 tom Exp $")

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  static bool Check_IPV4_Field(
|                                      FIELD * field,
|                                      const void * argp)
|   
|   Description   :  Validate buffer content to be a valid IP number (Ver. 4)
|
|   Return Values :  TRUE  - field is valid
|                    FALSE - field is invalid
+--------------------------------------------------------------------------*/
static bool Check_IPV4_Field(FIELD * field, const void * argp GCC_UNUSED)
{
  char *bp = field_buffer(field,0);
  int num = 0, len;
  unsigned int d1, d2, d3, d4;

  if(isdigit((unsigned char)*bp)) /* Must start with digit */
    {
      num = sscanf(bp, "%u.%u.%u.%u%n", &d1, &d2, &d3, &d4, &len);
      if (num == 4)
        {
          bp += len;            /* Make bp point to what sscanf() left */
          while (*bp && isspace((unsigned char)*bp))
            bp++;               /* Allow trailing whitespace */
        }
    }
  return ((num != 4 || *bp || d1 > 255 || d2 > 255
                           || d3 > 255 || d4 > 255) ? FALSE : TRUE);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  static bool Check_IPV4_Character(
|                                      int c, 
|                                      const void *argp )
|   
|   Description   :  Check a character for unsigned type or period.
|
|   Return Values :  TRUE  - character is valid
|                    FALSE - character is invalid
+--------------------------------------------------------------------------*/
static bool Check_IPV4_Character(int c, const void * argp GCC_UNUSED)
{
  return ((isdigit(c) || (c=='.')) ? TRUE : FALSE);
}

static FIELDTYPE typeIPV4 = {
  _RESIDENT,
  1,                           /* this is mutable, so we can't be const */
  (FIELDTYPE *)0,
  (FIELDTYPE *)0,
  NULL,
  NULL,
  NULL,
  Check_IPV4_Field,
  Check_IPV4_Character,
  NULL,
  NULL
};

NCURSES_EXPORT_VAR(FIELDTYPE*) TYPE_IPV4 = &typeIPV4;

/* fty_ipv4.c ends here */

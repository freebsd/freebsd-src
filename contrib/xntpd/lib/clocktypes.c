/* clocktypes.c,v 3.1 1993/07/06 01:08:09 jbj Exp
 * Data for pretty printing clock types
 */
#include <stdio.h>

#include "ntp_fp.h"
#include "ntp.h"
#include "lib_strbuf.h"
#include "ntp_refclock.h"

struct clktype clktypes[] = {
	{ REFCLK_NONE,		"unspecified type (0)",            "UNKNOWN" },
	{ REFCLK_LOCALCLOCK,	"local clock synchronization (1)", "LOCAL" },
	{ REFCLK_WWV_HEATH,	"Heathkit WWV clock (2)",          "WWV_HEATH" },
	{ REFCLK_WWV_PST,	"Precision Standard Time WWV clock (3)", "WWV_PST" },
	{ REFCLK_WWVB_SPECTRACOM, "Spectracom WWVB clock (4)",     "WWVB_SPEC" },
	{ REFCLK_GOES_TRUETIME,	"True Time GPS/GOES clock (5)",    "GPS_GOES_TRUE" },
	{ REFCLK_IRIG_AUDIO,	"IRIG audio decoder (6)",          "IRIG_AUDIO" },
	{ REFCLK_CHU,		"Direct synced to CHU (7)",        "CHU" },
	{ REFCLK_PARSE,		"Generic reference clock driver (8)", "GENERIC" },
	{ REFCLK_GPS_MX4200,	"Magnavox MX4200 GPS clock (9)",   "GPS_MX4200" },
	{ REFCLK_GPS_AS2201,	"Austron 2201A GPS clock (10)",    "GPS_AS2201" },
	{ REFCLK_OMEGA_TRUETIME, "TrueTime OMEGA clock (11)",      "OMEGA_TRUE" },
	{ REFCLK_IRIG_TPRO,	"Odetics/KSI TPRO IRIG decoder (12)", "IRIG_TPRO" },
	{ REFCLK_ATOM_LEITCH,	"Leitch CSD 5300 controller (13)", "ATOM_LEITCH" },
	{ REFCLK_MSF_EES,	"MSF EES M201, UK (14)",           "MSF_EES" },
	{ REFCLK_GPSTM_TRUETIME, "TrueTime GPS/TM-TMD clock (15)", "GPS_TRUE" },
	{ -1,			"", "" }
};

const char *
clockname(num)
  int num;
{
  register struct clktype *clk;
  
  for (clk = clktypes; clk->code != -1; clk++)
    {
      if (num == clk->code)
	{
	  return clk->abbrev;
	}
    }

  return NULL;
}

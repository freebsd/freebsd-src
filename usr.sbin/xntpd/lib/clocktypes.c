/*
 * Data for pretty printing clock types
 */
#include <stdio.h>

#include "ntp_fp.h"
#include "ntp.h"
#include "lib_strbuf.h"
#include "ntp_refclock.h"

struct clktype clktypes[] = {
	{ REFCLK_NONE,		"unspecified type (0)",
	    "UNKNOWN" },
	{ REFCLK_LOCALCLOCK,	"Undisciplined local clock (1)",
	    "LOCAL" },
	{ REFCLK_GPS_TRAK,	"TRAK 8810 GPS Receiver (2)",
	    "GPS_TRAK" },
	{ REFCLK_WWV_PST,	"PSTI/Traconex WWV/WWVH Receiver (3)",
	    "WWV_PST" },
	{ REFCLK_WWVB_SPECTRACOM, "Spectracom WWVB Receiver (4)",
	    "WWVB_SPEC" },
	{ REFCLK_GOES_TRUETIME,	"TrueTime GPS/GOES Receivers (5)",
	    "GPS_GOES_TRUE" },
	{ REFCLK_IRIG_AUDIO,	"IRIG Audio Decoder (6)",
	    "IRIG_AUDIO" },
	{ REFCLK_CHU,		"Scratchbuilt CHU Receiver (7)",
            "CHU" },
	{ REFCLK_PARSE,		"Generic reference clock driver (8)",
	    "GENERIC" },
	{ REFCLK_GPS_MX4200,	"Magnavox MX4200 GPS Receiver (9)",
	    "GPS_MX4200" },
	{ REFCLK_GPS_AS2201,	"Austron 2201A GPS Receiver (10)",
	    "GPS_AS2201" },
	{ REFCLK_OMEGA_TRUETIME, "TrueTime OM-DC OMEGA Receiver (11)",
	    "OMEGA_TRUE" },
	{ REFCLK_IRIG_TPRO,	"KSI/Odetics TPRO/S IRIG Interface (12)",
	    "IRIG_TPRO" },
	{ REFCLK_ATOM_LEITCH,	"Leitch CSD 5300 Master Clock Controller (13)",
	    "ATOM_LEITCH" },
	{ REFCLK_MSF_EES,	"EES M201 MSF Receiver (14)",
            "MSF_EES" },
	{ REFCLK_GPSTM_TRUETIME, "TrueTime GPS/TM-TMD Receiver (15)",
	    "GPS_TRUE" },
	{ REFCLK_IRIG_BANCOMM,	"Bancomm GPS/IRIG Receiver (16)",
	    "GPS_BANC" },
	{ REFCLK_GPS_DATUM,	"Datum Precision Time System (17)",
	    "GPS_DATUM" },
	{ REFCLK_NIST_ACTS,	"NIST Automated Computer Time Service (18)",
	    "NIST_ACTS" },
	{ REFCLK_WWV_HEATH,	"Heath WWV/WWVH Receiver (19)",
	    "WWV_HEATH" },
	{ REFCLK_GPS_NMEA,	"Generic NMEA GPS Receiver (20)",
	    "GPS_NMEA" },
	{ REFCLK_GPS_MOTO,	"Motorola Six Gun GPS Receiver (21)",
	    "GPS_MOTO" },
	{ REFCLK_ATOM_PPS,	"PPS Clock Discipline (22)",
	    "ATOM_PPS" },
	{ -1,			"", "" }
};

const char *
clockname(num)
	int num;
{
	register struct clktype *clk;
  
	for (clk = clktypes; clk->code != -1; clk++) {
		if (num == clk->code)
			return (clk->abbrev);
	}
	return (NULL);
}

/*
 * Data for pretty printing clock types
 */
#include <stdio.h>

#include "ntp_fp.h"
#include "ntp.h"
#include "lib_strbuf.h"
#include "ntp_refclock.h"
#include "ntp_stdlib.h"

struct clktype clktypes[] = {
	{ REFCLK_NONE,		"unspecified type (0)",
	  "UNKNOWN" },
	{ REFCLK_LOCALCLOCK,	"Undisciplined local clock (1)",
	  "LOCAL" },
	{ REFCLK_GPS_TRAK,	"TRAK 8810 GPS Receiver (2)",
	  "GPS_TRAK" },
	{ REFCLK_WWV_PST,	"PSTI/Traconex WWV/WWVH Receiver (3)",
	  "WWV_PST" },
	{ REFCLK_SPECTRACOM,	"Spectracom (generic) Receivers (4)",
	  "SPECTRACOM" },
	{ REFCLK_TRUETIME,	"TrueTime (generic) Receivers (5)",
	  "TRUETIME" },
	{ REFCLK_IRIG_AUDIO,	"IRIG Audio Decoder (6)",
	  "IRIG_AUDIO" },
	{ REFCLK_CHU_AUDIO,	"CHU Audio Demodulator/Decoder (7)",
	  "CHU_AUDIO" },
	{ REFCLK_PARSE,		"Generic reference clock driver (8)",
	  "GENERIC" },
	{ REFCLK_GPS_MX4200,	"Magnavox MX4200 GPS Receiver (9)",
	  "GPS_MX4200" },
	{ REFCLK_GPS_AS2201,	"Austron 2201A GPS Receiver (10)",
	  "GPS_AS2201" },
	{ REFCLK_GPS_ARBITER,	"Arbiter 1088A/B GPS Receiver (11)",
	  "GPS_ARBITER" },
	{ REFCLK_IRIG_TPRO,	"KSI/Odetics TPRO/S IRIG Interface (12)",
	  "IRIG_TPRO" },
	{ REFCLK_ATOM_LEITCH,	"Leitch CSD 5300 Master Clock Controller (13)",
	  "ATOM_LEITCH" },
	{ REFCLK_MSF_EES,	"EES M201 MSF Receiver (14)",
	  "MSF_EES" },
	{ REFCLK_GPSTM_TRUE,	"TrueTime (old GPSTM driver) Receiver (15)",
	  "GPSTM_TRUE" },
	{ REFCLK_IRIG_BANCOMM,	"Bancomm GPS/IRIG Receiver (16)",
	  "GPS_BANC" },
	{ REFCLK_GPS_DATUM,	"Datum Precision Time System (17)",
	  "GPS_DATUM" },
	{ REFCLK_NIST_ACTS,	"NIST Automated Computer Time Service (18)",
	  "ACTS_NIST" },
	{ REFCLK_WWV_HEATH,	"Heath WWV/WWVH Receiver (19)",
	  "WWV_HEATH" },
	{ REFCLK_GPS_NMEA,	"Generic NMEA GPS Receiver (20)",
	  "GPS_NMEA" },
	{ REFCLK_GPS_VME,	"TrueTime GPS-VME Interface (21)",
	  "GPS_VME" },
	{ REFCLK_ATOM_PPS,	"PPS Clock Discipline (22)",
	  "PPS" },
	{ REFCLK_PTB_ACTS,	"PTB Automated Computer Time Service (23)",
	  "ACTS_PTB" },
	{ REFCLK_USNO,		"Naval Observatory dialup (24)",
	  "ACTS_USNO" },
	{ REFCLK_TRUETIME,	"TrueTime (generic) Receivers (25)",
	  "TRUETIME" },
	{ REFCLK_GPS_HP,	"HP 58503A GPS Time & Frequency Receiver (26)",
	  "GPS_HP" },
	{ REFCLK_ARCRON_MSF,	"ARCRON MSF (and DCF77) Receiver (27)",
	  "MSF_ARCRON" },
	{ REFCLK_SHM,		"Clock attached thru shared Memory (28)",
	  "SHM" },
        { REFCLK_PALISADE,      "Trimble Navigation Palisade GPS (29)",
          "GPS_PALISADE" },
	{ REFCLK_ONCORE,	"Motorola UT Oncore GPS (30)",
	  "GPS_ONCORE" },
	{ REFCLK_GPS_JUPITER,	"Rockwell Jupiter GPS (31)",
	  "GPS_JUPITER" },
	{ REFCLK_CHRONOLOG,	"Chrono-log K (32)",
	  "CHRONOLOG" },
	{ REFCLK_DUMBCLOCK,	"Dumb generic hh:mm:ss local clock (33)",
	  "DUMBCLOCK" },
	{ REFCLK_ULINK,         "Ultralink M320 WWVB receiver (34)",
	  "ULINK_M320"},
	{ REFCLK_PCF,		"Conrad parallel port radio clock (35)",
	  "PCF"},
	{ REFCLK_WWV_AUDIO,	"WWV/H Audio Demodulator/Decoder (36)",
	  "WWV_AUDIO"},
	{ REFCLK_FG,	"Forum Graphic GPS Dating Station (37)",
	  "GPS_FG"},
	{ REFCLK_HOPF_SERIAL,	"hopf Elektronic serial line receiver (38)",
	  "HOPF_S"},
	{ REFCLK_HOPF_PCI,	"hopf Elektronic PCI receiver (39)",
	  "HOPF_P"},
	{ -1,			"", "" }
};

const char *
clockname(
	int num
	)
{
	register struct clktype *clk;
  
	for (clk = clktypes; clk->code != -1; clk++) {
		if (num == clk->code)
		    return (clk->abbrev);
	}
	return (NULL);
}

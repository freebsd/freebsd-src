# awk program to scan clockstat files and report errors/statistics
#
# usage: awk -f check.awk clockstats
#
# This program works for Spectracom 8170/Netclock-2 receiver, Austron
# 2200A/2201A receiver and IRIG audio decoder. It is easily adapted to
# other receivers as required. See README.austron file for additional
# information on Austron receiver.
#
BEGIN {
	etf_min = osc_vmin = osc_tmin = 1e9
	etf_max = osc_vmax = osc_tmax = -1e9
}
#
# scan all records in file
#
{
	#
	# select WWVB records
	# see summary for decode
	#
	if (NF >= 4 && $3 == "127.127.4.1") {
		if ($4 == "SIGNAL" || NF > 7)
			printf "%s\n", $0
		else {
			wwvb_count++
			if ($4 ~ /\?/)
				wwvb_x++
			else if ($4 ~ /A/)
				wwvb_a++
			else if ($4 ~ /B/)
				wwvb_b++
			else if ($4 ~ /C/)
				wwvb_c++
			else if ($4 ~ /D/)
				wwvb_d++
		}
	continue
	}
	#
	# select IRIG records
	# see summary for decode
	#
	if (NF >= 4 && $3 == "127.127.6.0") {
		irig_count++
		if ($5 ~ /\?/)
			irig_error++
	continue
	}
	#
	# select LORAN ENSEMBLE records
	# see summary for decode
	#
	else if (NF >= 13 && $6 == "ENSEMBLE") {
		ensemble_count++
		if ($9 <= 0)
			ensemble_badgps++
		else if ($12 <= 0)
			ensemble_badloran++
		else {
			if ($13 > 200e-9 || $13 < -200e-9)
				ensemble_200++
			else if ($13 > 100e-9 || $13 < -100e-9)
				ensemble_100++
			ensemble_mean += $13
			ensemble_rms += $13 * $13
		}
	continue
	}
	#
	# select LORAN TDATA records
	# see summary for decode; note that signal quality log is simply
	# copied to output
	#
	else if (NF >= 7 && $6 == "TDATA") {
                tdata_count++
                for (i = 7; i < NF; i++) {
                        if ($i == "M" && $(i+1) == "OK") {
                                i += 5
                                m += $i
                		tdata_m++
		        }
                        else if ($i == "W" && $(i+1) == "OK") {
                                i += 5
                                w += $i
                        	tdata_w++
			}
                        else if ($i == "X" && $(i+1) == "OK") {
                                i += 5
                                x += $i
                        	tdata_x++
			}
                        else if ($i == "Y" && $(i+1) == "OK") {
                                i += 5
                                y += $i
                        	tdata_y++
			}
                        else if ($i == "Z" && $(i+1) == "OK") {
                                i += 5
                                z += $i
                        	tdata_z++
			}
		}	
	continue
	}
	#
	# select ITF records
	# see summary for decode
	#
	else if (NF >= 13 && $5 == "ITF" && $12 >= 500) {
		itf_count++
		if ($9 > 200e-9 || $9 < -200e-9)
			itf_200++
		else if ($9 > 100e-9 || $9 < -100e-9)
			itf_100++
		itf_mean += $9
		itf_rms += $9 * $9
		itf_var += $10 * $10
	continue
	}
	#
	# select ETF records
	# see summary for decode
	#
	else if (NF >= 13 && $5 == "ETF" && $13 >= 500) {
		etf_count++
		if ($6 > etf_max)
			etf_max = $6
		else if ($6 < etf_min)
			etf_min = $6
		etf_mean += $6
		etf_rms += $6 * $6
		etf_var += $9 * $9
	continue
	}
	#
	# select TRSTAT records
	# see summary for decode
	#
	else if (NF >= 5 && $5 == "TRSTAT") {
		trstat_count++
		j = 0
		for (i = 6; i <= NF; i++)
			if ($i == "T")
				j++
		trstat_sat[j]++
	continue
	}
	#
	# select ID;OPT;VER records
	#
	# config GPS 2201A TTY1 TC1 LORAN IN OUT1 B.00 B.00 28-Apr-93
	#
	# GPS 2201A	receiver model
	# TTY1		rs232 moduel
	# TC1		IRIG module
	# LORAN		LORAN assist module
	# IN		input module
	# OUT1		output module
	# B.00 B.00	firmware revision
	# 28-Apr-9	firmware date3
        #
	else if (NF >= 5 && $5 == "ID;OPT;VER") {
		id_count++
		id_temp = ""
		for (i = 6; i <= NF; i++)
			id_temp = id_temp " " $i
		if (id_string != id_temp)
			printf "config%s\n", id_temp
		id_string = id_temp
	continue	
	}
	#
	# select POS;PPS;PPSOFF records
	#
	# position +39:40:48.425 -075:45:02.392 +74.09 Stored UTC 0 200 0
	#
	# +39:40:48.425	position north latitude
	# -075:45:02.392 position east longitude
	# +74.09	elevation (meters)
	# Stored	position is stored
	# UTC		time is relative to UTC
	# 0 200 0	PPS offsets
	#
	else if (NF >= 5 && $5 == "POS;PPS;PPSOFF") {
		pos_count++
		pos_temp = ""
		for (i = 6; i <= NF; i++)
			pos_temp = pos_temp " " $i
		if (pos_string != pos_temp)
			printf "position%s\n", pos_temp
		pos_string = pos_temp
	continue
	}
	#
	# select OSC;ET;TEMP records
	#
	# loop 1121 Software Control Locked
	#
	# 1121		oscillator type
	# Software Control loop is under software control
	# Locked	loop is locked
	#
	else if (NF >= 5 && $5 == "OSC;ET;TEMP") {
		osc_count++
		osc_temp = $6 " " $7 " " $8 " " $9
		if (osc_status != osc_temp)
			printf "loop %s\n", osc_temp
		osc_status = osc_temp
		if ($10 > osc_vmax)
			osc_vmax = $10
		if ($10 < osc_vmin)
			osc_vmin = $10
		if ($11 > osc_tmax)
			osc_tmax = $11
		if ($11 < osc_tmin)
			osc_tmin = $11
	continue
	}
	#
	# select UTC records
	# these ain't ready yet
	#
	else if (NF >= 5 && $5 == "UTC") {
		utc_count++
		utc_temp = ""
		for (i = 6; i <= NF; i++)
			utc_temp = utc_temp " " $i
		if (utc_string != utc_temp)
#			printf "utc%s\n", utc_temp
                utc_string = utc_temp
	continue
	}
} END {
#
# ensemble summary data
#
# ensemble	record count
# badgps	gps data unavailable
# badloran	loran data unavailable
# rms		ensemble rms error (ns)
# >200		ensemble error >200 ns
# >100		100 ns < ensemble error < 200 ns
#
	if (ensemble_count > 0) {
		ensemble_mean /= ensemble_count
		ensemble_rms = sqrt(ensemble_rms / ensemble_count - ensemble_mean * ensemble_mean) * 1e9 
		printf "ensemble %d, badgps %d, badloran %d, rms %.1f, >200 %d, >100 %d\n", ensemble_count, ensemble_badgps, ensemble_badloran, ensemble_rms, ensemble_200, ensemble_100
	}
#
# wwvb summary data
#
# wwvb		record count
# ?		unsynchronized
# >1		error > 1 ms
# >10		error > 10 ms
# >100		error > 100 ms
# >500		error > 500 ms
#
	if (wwvb_count > 0)
		printf "wwvb %d, ? %d, >1 %d, >10 %d, >100 %d, >500 %d\n", wwvb_count, wwvb_x, wwvb_a, wwvb_b, wwvb_c, wwvb_d
#
# irig summary data
#
# irig		record count
# err		error count
#
	if (irig_count > 0)
		printf "irig %d, err %d\n", irig_count, irig_error
#
# tdata summary data
#
# tdata		record count
# m		M master OK-count, mean level (dB)
# w		W slave OK-count, mean level (dB)
# x		X slave OK-count, mean level (dB)
# y		Y slave OK-count, mean level (dB)
# z		Z slave OK-count, mean level (dB)
#
	if (tdata_count > 0 ) {
		if (tdata_m > 0)
			m /= tdata_count
		if (tdata_x > 0)
			w /= tdata_count
		if (tdata_x > 0)
			x /= tdata_count
		if (tdata_y > 0)
			y /= tdata_count
		if (tdata_z > 0)
			z /= tdata_count
		printf "tdata %d, m %d %.1f, w %d %.1f, x %d %.1f, y %d %.1f, z %d %.1f\n", tdata_count, tdata_m, m, tdata_w, w, tdata_x, x, tdata_y, y, tdata_z, z
	}
#
# itf summary data
#
# itf		record count
# rms		itf rms error (ns)
# >200		itf error > 200 ns
# >100		itf error > 100 ns
# var		Allan variance
#
	if (itf_count > 1) { 
		itf_mean /= itf_count
		itf_rms = sqrt(itf_rms / itf_count - itf_mean * itf_mean) * 1e9
		itf_var = sqrt(itf_var / (2 * (itf_count - 1)))
		printf "itf %d, rms %.1f, >200 %d, >100 %d, var %.2e\n", itf_count, itf_rms, itf_200, itf_100, itf_var
	}
#
# etf summary data
#
# etf		record count
# mean		etf mean (ns)
# rms		etf rms error (ns)
# max		etf maximum (ns)
# min		etf minimum (ns)
# var		Allan variance
#
	if (etf_count > 0) {
                etf_mean /= etf_count
		etf_rms = sqrt(etf_rms / etf_count - etf_mean * etf_mean)
		etf_var = sqrt(etf_var / (2 * (etf_count - 1)))
		printf "etf %d, mean %.1f, rms %.1f, max %d, min %d, var %.2e\n", etf_count, etf_mean, etf_rms, etf_max, etf_min, etf_var
	}
#
# trstat summary data
#
# trstat	record count
# sat		histogram of tracked satellites (0 - 7)
#
	if (trstat_count > 0)
		printf "trstat %d, sat %d %d %d %d %d %d %d %d\n", trstat_count, trstat_sat[0], trstat_sat[1], trstat_sat[2], trstat_sat[2], trstat_sat[3], trstat_sat[4], trstat_sat[5], trstat_sat[6], trstat_sat[7]
#
# osc summary data
#
# osc		record count
# control	control midrange (V) +/- deviation (mV)
# temp		oven temperature midrange +/- deviation (deg C)
#
	if (osc_count > 0)
		printf "osc %d, control %.3f+/-%.3f, temp %.1f+/-%.2f\n", osc_count, (osc_vmax + osc_vmin) / 2, (osc_vmax - osc_vmin) / 2 * 1e3, (osc_tmax + osc_tmin) / 2, (osc_tmax - osc_tmin) / 2
}

# To: bug-gnu-utils@prep.ai.mit.edu
# Cc: arnold@gnu.ai.mit.edu
# Date: Mon, 20 Nov 1995 11:39:29 -0500
# From: "R. Hank Donnelly" <emory!head-cfa.harvard.edu!donnelly>
# 
# Operating system: Linux1.2.13 (Slackware distrib)
# GAWK version: 2.15 (?)
# compiler: GCC (?)
# 
# The following enclosed script does not want to fully process the input data
# file. It correctly executes the operations on the first record, and then dies
# on the second one. My true data file is much longer but this is
# representative and it does fail on a file even as short as this one.
# The failure appears to occur in the declared function add2output. Between the
# steps of incrementing NF by one and setting $NF to the passed variable
# the passed variable appears to vanish (i.e. NF does go from 68 to 69
# and before incrementing it "variable" equals what it should but after
# "variable" has no value at all.)
# 
# The scripts have been developed using nawk on a Sun (where they run fine)
# I have tried gawk there but get a different crash which I have not yet traced
# down. Ideally I would like to keep the script the same so that it would run
# on either gawk or nawk (that way I can step back and forth between laptop and
# workstation.
# 
# Any ideas why the laptop installation is having problems?
# Hank 
# 
# 
# #!/usr/bin/gawk -f

BEGIN {
	# set a few values
	FS = "\t"
	OFS = "\t"
	pi = atan2(0, -1)
# distance from HRMA to focal plane in mm
	fullradius = 10260.54

	# set locations of parameters on input line
	nf_nrg = 1
	nf_order = 3
	nf_item = 4
	nf_suite = 5
	nf_grating = 8
	nf_shutter = 9
	nf_type = 13
	nf_src = 14
	nf_target = 15
	nf_voltage = 16
	nf_flux = 17
	nf_filt1 = 20
	nf_filt1_th = 21
	nf_filt2 = 22
	nf_filt2_th = 23
	nf_bnd = 24
	nf_hrma_polar = 27
	nf_hrma_az = 28
	nf_detector = 30
	nf_acis_read = 32
	nf_acis_proc = 33
	nf_acis_frame = 34
	nf_hxda_aplist = 36
	nf_hxda_y_range = 37
	nf_hxda_z_range = 38
	nf_hxda_y_step = 39
	nf_hxda_z_step = 40
	nf_sim_z = 41
	nf_fam_polar = 43
	nf_fam_az = 44
	nf_fam_dither_type = 45
	nf_mono_init = 51
	nf_mono_range = 52
	nf_mono_step = 53
	nf_defocus = 54
	nf_acis_temp = 55
	nf_tight = 59
	nf_offset_y = 64
	nf_offset_z = 65

	while( getline < "xrcf_mnemonics.dat" > 0 ) {
		mnemonic[$1] = $2
	}

#	"date" | getline date_line
# ADR: use a fixed date so that testing will work
	date_line = "Sun Mar 10 23:00:27 EST 1996"
        split(date_line, in_date, " ")
        out_date = in_date[2] " " in_date[3] ", " in_date[6]
}

function add2output( variable ) {
#print("hi1") >> "debug"
	NF++
#print("hi2") >> "debug"
 	$NF = variable
#print("hi3") >> "debug"
}

function error( ekey, message ) {
	print "Error at input line " NR ", anode " ekey >> "errors.cleanup"
	print "   " message "." >> "errors.cleanup"
}

function hxda_na() {
	$nf_hxda_aplist = $nf_hxda_y_range = $nf_hxda_z_range = "N/A"
	$nf_hxda_y_step = $nf_hxda_z_step = "N/A"
}

function acis_na() {
	$nf_acis_read = $nf_acis_proc = $nf_acis_frame = $nf_acis_temp = "N/A"
}

function hrc_na() {
#        print ("hi") >> "debug"
}

function fpsi_na() {
	acis_na()
	hrc_na()
	$nf_sim_z = $nf_fam_polar = $nf_fam_az = $nf_fam_dither_type = "N/A"
}

function mono_na() {
	$nf_mono_init = $nf_mono_range = $nf_mono_step = "N/A"
}

# this gives the pitch and yaw of the HRMA and FAM
# positive pitch is facing the source "looking down"
# positive yaw is looking left
# 0 az is north 90 is up
# this also adds in the FAM X,Y,Z positions 

function polaz2yawpitch(polar, az) {
	theta = az * pi / 180
	phi = polar * pi / 180 / 60


	if( polar == 0 ) {
		add2output( 0 )
		add2output( 0 )
	} else {
		if(az == 0 || az == 180)
			add2output( 0 )
		else 
			add2output( - polar * sin(theta) )


#			x = cos (phi)
#			y = sin (phi) * cos (theta)
#			add2output( atan2(y,x)*180 / pi * 60 )
		
		if(az == 90 || az ==270 )
			add2output( 0 )
		else 
			add2output( - polar * cos(theta) )

	}
#			x = cos (phi)
#			z= sin (phi) * sin (theta)
#			add2output( atan2(z,x)*180 / pi * 60 )

	if(config !~ /HXDA/) {
# negative values of defocus move us farther from the source thus
# increasing radius
		radius = fullradius - defocus

# FAM_x; FAM_y;  FAM_z
	   	if((offset_y == 0) && (offset_z == 0)){
			add2output( fullradius - radius * cos (phi) )
	
			if (az == 90 || az ==270) 
				add2output( 0 )
			else
				add2output(  radius * sin (phi) * cos (theta) )
			
			if (az == 0 || az == 180)
				add2output( 0 )
			else		
				add2output( - radius * sin (phi) * sin (theta) )
	   	} else {
# ******* THIS SEGMENT OF CODE IS NOT MATHEMATICALLY CORRECT FOR ****
# OFF AXIS ANGLES AND IS SUPPLIED AS A WORKAROUND SINCE IT WILL
# PROBABLY ONLY BE USED ON AXIS.
			add2output( defocus )
			add2output( offset_y )
			add2output( offset_z )
		}

	} else {
		add2output( "N/A" )
		add2output( "N/A" )
		add2output( "N/A" )
	}
}

# set TIGHT/LOOSE to N/A if it is not one of the two allowed values
function tight_na() {
	if( $nf_tight !~ /TIGHT|LOOSE/ ) {
		$nf_tight == "N/A"
	}
}

# this entry is used to give certain entries names
{
	type = $nf_type
	item = $nf_item
	suite = $nf_suite
	order = $nf_order
	detector = $nf_detector
	grating = $nf_grating
	offset_y= $nf_offset_y
	offset_z= $nf_offset_z
	bnd = $nf_bnd
	defocus = $nf_defocus
}

{
	# make configuration parameter
	# as well as setting configuration-dependent N/A values

	if( $nf_bnd ~ "SCAN" ) {
		# BND is scanning beam
		config = "BND"
		hxda_na()
		fpsi_na()
	} else {
		if( grating == "NONE" ) {
			config = "HRMA"
		} else {
			if( grating == "HETG" ) {
				if( order != "Both" ) {
				    $nf_shutter = order substr($nf_shutter, \
					index($nf_shutter, ",") )
				}
			} else {
				order = "N/A"
			}
			config = "HRMA/" grating
		}
	
		if( detector ~ /ACIS|HRC/ ) {
			detsys = detector
			nsub = sub("-", ",", detsys)
			config = config "/" detsys
			hxda_na()
		} else {
			config = config "/HXDA"
			fpsi_na()
			if( detector == "HSI" ) {
				hxda_na()
			}
		}
	}

	add2output( config )

	if( $nf_src ~ /EIPS|Penning/ ) mono_na()

	if( $nf_src == "Penning" ) $nf_voltage = "N/A"

	itm = sprintf("%03d", item)

	if(config in mnemonic) {
		if( type in mnemonic ) {
		    ID = mnemonic[config] "-" mnemonic[type] "-" suite "." itm
		    add2output( ID )
		} else {
			error(type, "measurement type not in list")
		}
	} else {
		error(config, "measurement configuration not in list")
	}

	# add date to output line
	add2output( out_date )

	# Convert HRMA polar and azimuthal angles to yaw and pitch
	polaz2yawpitch($nf_hrma_polar, $nf_hrma_az)

	# set TIGHT/LOOSE to N/A if it is not one of the two allowed values
	tight_na()

	# compute number of HXDA apertures
	if( config ~ /HXDA/ && $nf_hxda_aplist != "N/A") 
		add2output( split( $nf_hxda_aplist, dummy, "," ) )
	else
		add2output( "N/A" )

	# make sure the BND value is properly set
	if($nf_bnd == "FIXED" && detector ~ /ACIS/)
		$nf_bnd =bnd"-SYNC"
	else
		$nf_bnd = bnd"-FREE"
	print
}

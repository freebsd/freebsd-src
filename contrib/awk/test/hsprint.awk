# Test which attempts to repeat examples of formatted output
# from "C a reference manual" by Harbison and Steele.
#
# In the second series of outputs formats of a type "%5%" are skipped
# since my old copy of H&S explicitely requires padding ("...%05% will
# print 0000%..."), whereas Standard says "...the complete conversion
# specification shall be %%".
#
# Michal Jaegermann - michal@phys.ualberta.ca


BEGIN {
    zero = "0";
    alt  = "#";
    spc  = " ";
    plus = "+";
    just = "-";
    value[0] = 45;
    value[1] = 45;
    value[2] = 45;
    value[3] = 12.678;
    value[4] = 12.678;
    value[5] = 12.678;
    value[6] = "zap";
    value[7] = "*";
    value[8] = -3.4567;
    value[9] = -3.4567;
    value[10]= -3.4567;
    value[11]= -3.4567;
    oper[0]  = "5d";
    oper[1]  = "5o";
    oper[2]  = "5x";
    oper[3]  = "7.2f";
    oper[4]  = "10.2e";
    oper[5]  = "10.4g";
    oper[6]  = "5s";
    oper[7]  = "5c";
    oper[8]  = "7.1G";
    oper[9]  = "7.2f";
    oper[10] = "10.2e";
    oper[11] = "10.4g";

    
    for (r = 0; r < 12; r += 6) {
	for (j = 2; j > 0; --j) {
	    for (p = 2; p > 0; --p) {
		for (s = 2; s > 0; --s) {
		    for (a = 2; a > 0; --a) {
			for (z = 2; z > 0; --z) {
			    fmt = "%" substr(just,j,1) substr(plus,p,1) \
			      substr(spc,s,1) substr(alt,a,1) substr(zero,z,1);
			    fstr = sprintf(\
				     "%6s|%s%s|%s%s|%s%s|%s%s|%s%s|%s%s|\n",
					   fmt, 
					   fmt, oper[r],
					   fmt, oper[r+1],
					   fmt, oper[r+2],
					   fmt, oper[r+3],
					   fmt, oper[r+4],
					   fmt, oper[r+5]);
			    printf(fstr, value[r],   value[r+1],
					 value[r+2], value[r+3],
					 value[r+4], value[r+5]);
			}
		    }
		}
	    }
	}
	print "";
    }
}




# addcomma - put commas in numbers
#   input:  a number per line
#   output: the input number followed by
#      the number with commas and two decimal places 

{ printf("%-12s %20s\n", $0, addcomma($0)) }

function addcomma(x,   num) {
 	if (x < 0)
 	    return "-" addcomma(-x)
 	num = sprintf("%.2f", x)   # num is dddddd.dd
 	while (num ~ /[0-9][0-9][0-9][0-9]/)
 	    sub(/[0-9][0-9][0-9][,.]/, ",&", num)
 	return num
}

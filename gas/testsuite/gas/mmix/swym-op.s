# Check different type of operands to SWYM etc.
# No need to check the canonical three constants.
Main	SWYM 132,3567
	TRIP 132,3567
	TRAP 132,YZ
	TRAP X,3567
	TRAP 2345678
	TRIP X,Y,Z
	TRIP X,YZ
	SWYM XYZ
X IS 23
Y IS 12
Z IS 67
YZ IS #5678
XYZ IS 1234567

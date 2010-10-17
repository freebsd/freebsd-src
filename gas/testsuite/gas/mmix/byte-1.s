# Test BYTE sequences, excercising code paths for valid input.
number	IS 42
Main SWYM 43,number,41

label BYTE "string",#a,255
lab2 BYTE number+100,0,"string2",#a

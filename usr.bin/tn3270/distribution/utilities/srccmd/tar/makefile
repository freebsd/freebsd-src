tarread.obj: tarread.c
	cl /c /Zd tarread.c

tarread.exe: tarread.obj
	link tarread,,tarread/map/lin;

tarread.sym: tarread.map
	mapsym /l tarread

# from Pat Rankin, rankin@eql.caltech.edu

BEGIN { dummy(1); legit(); exit }

function dummy(arg)
{
	return arg
}

function legit(         scratch)
{
	split("1 2 3", scratch)
	return ""
}

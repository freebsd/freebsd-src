BEGIN { FS="("; state = 0 }

/^	public/ { ftype = $0; state = 1 }

{ if (state == 1)
	state = 2
  else if (state == 2)
	{ print ftype,$1,"();"; state = 0 }
}

enum {
	DISPATCH_BLOCK,
	DISPATCH_NONBLOCK
};

typedef void dispatch_fn(int type, int plen);

void	dispatch_init(dispatch_fn *dflt);
void	dispatch_set(int type, dispatch_fn *fn);
void	dispatch_run(int mode, int *done);
void	dispatch_protocol_error(int type, int plen);

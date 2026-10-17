/* Bypass locale translation. */
const char *
__lctrans_cur(const char *msg)
{
	return (msg);
}


#define DN_DEBUG_BUFFER_BASE 0x82800000
#define DN_DEBUG_BUFFER_SIZE 8*1024*1024

static char *current_dbg_ptr=DN_DEBUG_BUFFER_BASE;

int dn_deb_printf(const char *fmt, ...) {

	va_list args;
	int i;

	if(current_dbg_ptr<(DN_DEBUG_BUFFER_BASE + DN_DEBUG_BUFFER_SIZE)) {
		va_start(args,fmt);
		i=vsprintf(current_dbg_ptr,fmt,args);
		va_end(args);
		current_dbg_ptr+=i;
	
		return i;
	}
	else 
		return 0;
}

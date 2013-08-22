/*
 * str2host.c
 *
 * conversion routines from the presentation format
 * to the host format
 *
 * a Net::DNS like library for C
 *
 * (c) NLnet Labs, 2004-2006
 *
 * See the file LICENSE for the license
 */
#include <ldns/config.h>

#include <ldns/ldns.h>

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <time.h>

#include <errno.h>
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#include <limits.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

ldns_status
ldns_str2rdf_int16(ldns_rdf **rd, const char *shortstr)
{
	char *end = NULL;
	uint16_t *r;
	r = LDNS_MALLOC(uint16_t);
        if(!r) return LDNS_STATUS_MEM_ERR;

	*r = htons((uint16_t)strtol((char *)shortstr, &end, 10));

	if(*end != 0) {
		LDNS_FREE(r);
		return LDNS_STATUS_INVALID_INT;
	} else {
		*rd = ldns_rdf_new_frm_data(
			LDNS_RDF_TYPE_INT16, sizeof(uint16_t), r);
		LDNS_FREE(r);
		return *rd?LDNS_STATUS_OK:LDNS_STATUS_MEM_ERR;
	}
}

ldns_status
ldns_str2rdf_time(ldns_rdf **rd, const char *time)
{
	/* convert a time YYYYDDMMHHMMSS to wireformat */
	uint16_t *r = NULL;
	struct tm tm;
	uint32_t l;
	char *end;

	/* Try to scan the time... */
	r = (uint16_t*)LDNS_MALLOC(uint32_t);
        if(!r) return LDNS_STATUS_MEM_ERR;

	memset(&tm, 0, sizeof(tm));

	if (strlen(time) == 14 &&
	    sscanf(time, "%4d%2d%2d%2d%2d%2d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6
	   ) {
	   	tm.tm_year -= 1900;
	   	tm.tm_mon--;
	   	/* Check values */
		if (tm.tm_year < 70) {
			goto bad_format;
		}
		if (tm.tm_mon < 0 || tm.tm_mon > 11) {
			goto bad_format;
		}
		if (tm.tm_mday < 1 || tm.tm_mday > 31) {
			goto bad_format;
		}

		if (tm.tm_hour < 0 || tm.tm_hour > 23) {
			goto bad_format;
		}

		if (tm.tm_min < 0 || tm.tm_min > 59) {
			goto bad_format;
		}

		if (tm.tm_sec < 0 || tm.tm_sec > 59) {
			goto bad_format;
		}

		l = htonl(ldns_mktime_from_utc(&tm));
		memcpy(r, &l, sizeof(uint32_t));
		*rd = ldns_rdf_new_frm_data(
			LDNS_RDF_TYPE_TIME, sizeof(uint32_t), r);
		LDNS_FREE(r);
		return *rd?LDNS_STATUS_OK:LDNS_STATUS_MEM_ERR;
	} else {
		/* handle it as 32 bits timestamp */
		l = htonl((uint32_t)strtol((char*)time, &end, 10));
		if(*end != 0) {
			LDNS_FREE(r);
			return LDNS_STATUS_ERR;
		} else {
			memcpy(r, &l, sizeof(uint32_t));
			*rd = ldns_rdf_new_frm_data(
				LDNS_RDF_TYPE_INT32, sizeof(uint32_t), r);
			LDNS_FREE(r);
		        return *rd?LDNS_STATUS_OK:LDNS_STATUS_MEM_ERR;
		}
	}

	bad_format:
	LDNS_FREE(r);
	return LDNS_STATUS_INVALID_TIME;
}

ldns_status
ldns_str2rdf_nsec3_salt(ldns_rdf **rd, const char *salt_str)
{
	uint8_t salt_length;
	int c;
	int salt_length_str;

	uint8_t *salt;
	uint8_t *data;
	if(rd == NULL) {
		return LDNS_STATUS_NULL;
	}

	salt_length_str = (int)strlen(salt_str);
	if (salt_length_str == 1 && salt_str[0] == '-') {
		salt_length_str = 0;
	} else if (salt_length_str % 2 != 0) {
		return LDNS_STATUS_INVALID_HEX;
	}
	if (salt_length_str > 512) {
		return LDNS_STATUS_INVALID_HEX;
	}

	salt = LDNS_XMALLOC(uint8_t, salt_length_str / 2);
        if(!salt) {
                return LDNS_STATUS_MEM_ERR;
        }
	for (c = 0; c < salt_length_str; c += 2) {
		if (isxdigit((int) salt_str[c]) && isxdigit((int) salt_str[c+1])) {
			salt[c/2] = (uint8_t) ldns_hexdigit_to_int(salt_str[c]) * 16 +
					  ldns_hexdigit_to_int(salt_str[c+1]);
		} else {
			LDNS_FREE(salt);
			return LDNS_STATUS_INVALID_HEX;
		}
	}
	salt_length = (uint8_t) (salt_length_str / 2);

	data = LDNS_XMALLOC(uint8_t, 1 + salt_length);
        if(!data) {
	        LDNS_FREE(salt);
                return LDNS_STATUS_MEM_ERR;
        }
	data[0] = salt_length;
	memcpy(&data[1], salt, salt_length);
	*rd = ldns_rdf_new_frm_data(LDNS_RDF_TYPE_NSEC3_SALT, 1 + salt_length, data);
	LDNS_FREE(data);
	LDNS_FREE(salt);

	return *rd?LDNS_STATUS_OK:LDNS_STATUS_MEM_ERR;
}

ldns_status
ldns_str2rdf_period(ldns_rdf **rd,const char *period)
{
        uint32_t p;
        const char *end;

        /* Allocate required space... */
        p = ldns_str2period(period, &end);

        if (*end != 0) {
		return LDNS_STATUS_ERR;
        } else {
                p = (uint32_t) htonl(p);
		*rd = ldns_rdf_new_frm_data(
			LDNS_RDF_TYPE_PERIOD, sizeof(uint32_t), &p);
        }
	return *rd?LDNS_STATUS_OK:LDNS_STATUS_MEM_ERR;
}

ldns_status
ldns_str2rdf_int32(ldns_rdf **rd, const char *longstr)
{
	char *end;
	uint16_t *r = NULL;
	uint32_t l;

	r = (uint16_t*)LDNS_MALLOC(uint32_t);
        if(!r) return LDNS_STATUS_MEM_ERR;
	errno = 0; /* must set to zero before call,
			note race condition on errno */
	if(*longstr == '-')
		l = htonl((uint32_t)strtol((char*)longstr, &end, 10));
	else	l = htonl((uint32_t)strtoul((char*)longstr, &end, 10));

	if(*end != 0) {
		LDNS_FREE(r);
		return LDNS_STATUS_ERR;
     } else {
		if (errno == ERANGE) {
			LDNS_FREE(r);
			return LDNS_STATUS_SYNTAX_INTEGER_OVERFLOW;
		}
		memcpy(r, &l, sizeof(uint32_t));
		*rd = ldns_rdf_new_frm_data(
			LDNS_RDF_TYPE_INT32, sizeof(uint32_t), r);
		LDNS_FREE(r);
	        return *rd?LDNS_STATUS_OK:LDNS_STATUS_MEM_ERR;
	}
}

ldns_status
ldns_str2rdf_int8(ldns_rdf **rd, const char *bytestr)
{
	char *end;
	uint8_t *r = NULL;

	r = LDNS_MALLOC(uint8_t);
        if(!r) return LDNS_STATUS_MEM_ERR;

	*r = (uint8_t)strtol((char*)bytestr, &end, 10);

        if(*end != 0) {
		LDNS_FREE(r);
		return LDNS_STATUS_ERR;
        } else {
		*rd = ldns_rdf_new_frm_data(
			LDNS_RDF_TYPE_INT8, sizeof(uint8_t), r);
		LDNS_FREE(r);
	        return *rd?LDNS_STATUS_OK:LDNS_STATUS_MEM_ERR;
        }
}


/*
 * Checks whether the escaped value at **s is an octal value or
 * a 'normally' escaped character (and not eos)
 *
 * The string pointer at *s is increased by either 0 (on error), 1 (on
 * normal escapes), or 3 (on octals)
 *
 * Returns the number of bytes read from the escaped string, or
 * 0 on error
 */
static int
parse_escape(uint8_t *s, uint8_t *q) {
	uint16_t val;
	if (strlen((char *)s) > 3 &&
	    isdigit((int) s[1]) &&
	    isdigit((int) s[2]) &&
	    isdigit((int) s[3])) {
		/* cast this so it fits */
		val = (uint16_t) ldns_hexdigit_to_int((char) s[1]) * 100 +
		                ldns_hexdigit_to_int((char) s[2]) * 10 +
		                ldns_hexdigit_to_int((char) s[3]);
		if (val > 255) {
			/* outside range */
			return 0;
		}
		*q = (uint8_t) val;
                return 3;
	} else {
		s++;
		if (*s == '\0' || isdigit((int) *s)) {
			/* apparently the string terminator
			 * or a digit has been escaped...
		         */
			return 0;
		}
		*q = *s;
		return 1;
	}
}

/*
 * No special care is taken, all dots are translated into
 * label seperators.
 * Could be made more efficient....we do 3 memcpy's in total...
 */
ldns_status
ldns_str2rdf_dname(ldns_rdf **d, const char *str)
{
	size_t len;

	int esc;
	uint8_t *s, *q, *pq, label_len;
	uint8_t buf[LDNS_MAX_DOMAINLEN + 1];
	*d = NULL;

	len = strlen((char*)str);
	/* octet representation can make strings a lot longer than actual length */
	if (len > LDNS_MAX_DOMAINLEN * 4) {
		return LDNS_STATUS_DOMAINNAME_OVERFLOW;
	}
	if (0 == len) {
		return LDNS_STATUS_DOMAINNAME_UNDERFLOW;
	}

	/* root label */
	if (1 == len && *str == '.') {
		*d = ldns_rdf_new_frm_data(LDNS_RDF_TYPE_DNAME, 1, "\0");
		return LDNS_STATUS_OK;
	}

	/* get on with the rest */

	/* s is on the current character in the string
         * pq points to where the labellength is going to go
         * label_len keeps track of the current label's length
	 * q builds the dname inside the buf array
	 */
	len = 0;
	q = buf+1;
	pq = buf;
	label_len = 0;
	for (s = (uint8_t *)str; *s; s++, q++) {
		if (q > buf + LDNS_MAX_DOMAINLEN) {
			return LDNS_STATUS_DOMAINNAME_OVERFLOW;
		}
		*q = 0;
		switch (*s) {
		case '.':
			if (label_len > LDNS_MAX_LABELLEN) {
				return LDNS_STATUS_LABEL_OVERFLOW;
			}
			if (label_len == 0) {
				return LDNS_STATUS_EMPTY_LABEL;
			}
			len += label_len + 1;
			*pq = label_len;
			label_len = 0;
			pq = q;
			break;
		case '\\':
			/* octet value or literal char */
			esc = parse_escape(s, q);
			if (esc > 0) {
				s += esc;
				label_len++;
			} else {
				return LDNS_STATUS_SYNTAX_BAD_ESCAPE;
			}
			break;
		default:
			*q = *s;
			label_len++;
		}
	}

	/* add root label if last char was not '.' */
	if (!ldns_dname_str_absolute(str)) {
		if (q > buf + LDNS_MAX_DOMAINLEN) {
			return LDNS_STATUS_DOMAINNAME_OVERFLOW;
		}
                if (label_len > LDNS_MAX_LABELLEN) {
                        return LDNS_STATUS_LABEL_OVERFLOW;
                }
                if (label_len == 0) { /* label_len 0 but not . at end? */
                        return LDNS_STATUS_EMPTY_LABEL;
                }
		len += label_len + 1;
		*pq = label_len;
		*q = 0;
	}
	len++;

	*d = ldns_rdf_new_frm_data(LDNS_RDF_TYPE_DNAME, len, buf);
	return LDNS_STATUS_OK;
}

ldns_status
ldns_str2rdf_a(ldns_rdf **rd, const char *str)
{
	in_addr_t address;
        if (inet_pton(AF_INET, (char*)str, &address) != 1) {
                return LDNS_STATUS_INVALID_IP4;
        } else {
		*rd = ldns_rdf_new_frm_data(
			LDNS_RDF_TYPE_A, sizeof(address), &address);
        }
	return *rd?LDNS_STATUS_OK:LDNS_STATUS_MEM_ERR;
}

ldns_status
ldns_str2rdf_aaaa(ldns_rdf **rd, const char *str)
{
	uint8_t address[LDNS_IP6ADDRLEN + 1];

	if (inet_pton(AF_INET6, (char*)str, address) != 1) {
		return LDNS_STATUS_INVALID_IP6;
	} else {
		*rd = ldns_rdf_new_frm_data(
			LDNS_RDF_TYPE_AAAA, sizeof(address) - 1, &address);
	}
	return *rd?LDNS_STATUS_OK:LDNS_STATUS_MEM_ERR;
}

ldns_status
ldns_str2rdf_str(ldns_rdf **rd, const char *str)
{
	uint8_t *data;
	size_t i, str_i, esc_i;

	if (strlen(str) > 255) {
		return LDNS_STATUS_INVALID_STR;
	}

	data = LDNS_XMALLOC(uint8_t, strlen(str) + 1);
        if(!data) return LDNS_STATUS_MEM_ERR;
	i = 1;

	for (str_i = 0; str_i < strlen(str); str_i++) {
		if (str[str_i] == '\\') {
			/* octet value or literal char */
			esc_i = (size_t) parse_escape((uint8_t*) &str[str_i], (uint8_t*) &data[i]);
			if (esc_i == 0) {
				LDNS_FREE(data);
				return LDNS_STATUS_SYNTAX_BAD_ESCAPE;
			}
			str_i += esc_i;
		} else {
			data[i] = (uint8_t) str[str_i];
		}
		i++;
	}
	data[0] = i - 1;
	*rd = ldns_rdf_new_frm_data(LDNS_RDF_TYPE_STR, i, data);

	LDNS_FREE(data);
	return *rd?LDNS_STATUS_OK:LDNS_STATUS_MEM_ERR;
}

ldns_status
ldns_str2rdf_apl(ldns_rdf **rd, const char *str)
{
	const char *my_str = str;

	char *my_ip_str;
	size_t ip_str_len;

	uint16_t family;
	bool negation;
	uint8_t afdlength = 0;
	uint8_t *afdpart;
	uint8_t prefix;

	uint8_t *data;

	size_t i = 0;

	/* [!]afi:address/prefix */
	if (strlen(my_str) < 2
			|| strchr(my_str, ':') == NULL
			|| strchr(my_str, '/') == NULL
			|| strchr(my_str, ':') > strchr(my_str, '/')) {
		return LDNS_STATUS_INVALID_STR;
	}

	if (my_str[0] == '!') {
		negation = true;
		my_str += 1;
	} else {
		negation = false;
	}

	family = (uint16_t) atoi(my_str);

	my_str = strchr(my_str, ':') + 1;

	/* need ip addr and only ip addr for inet_pton */
	ip_str_len = (size_t) (strchr(my_str, '/') - my_str);
	my_ip_str = LDNS_XMALLOC(char, ip_str_len + 1);
        if(!my_ip_str) return LDNS_STATUS_MEM_ERR;
	strncpy(my_ip_str, my_str, ip_str_len + 1);
	my_ip_str[ip_str_len] = '\0';

	if (family == 1) {
		/* ipv4 */
		afdpart = LDNS_XMALLOC(uint8_t, 4);
                if(!afdpart) {
                        LDNS_FREE(my_ip_str);
                        return LDNS_STATUS_MEM_ERR;
                }
		if (inet_pton(AF_INET, my_ip_str, afdpart) == 0) {
                        LDNS_FREE(my_ip_str);
                        LDNS_FREE(afdpart);
			return LDNS_STATUS_INVALID_STR;
		}
		for (i = 0; i < 4; i++) {
			if (afdpart[i] != 0) {
				afdlength = i + 1;
			}
		}
	} else if (family == 2) {
		/* ipv6 */
		afdpart = LDNS_XMALLOC(uint8_t, 16);
                if(!afdpart) {
                        LDNS_FREE(my_ip_str);
                        return LDNS_STATUS_MEM_ERR;
                }
		if (inet_pton(AF_INET6, my_ip_str, afdpart) == 0) {
                        LDNS_FREE(my_ip_str);
                        LDNS_FREE(afdpart);
			return LDNS_STATUS_INVALID_STR;
		}
		for (i = 0; i < 16; i++) {
			if (afdpart[i] != 0) {
				afdlength = i + 1;
			}
		}
	} else {
		/* unknown family */
		LDNS_FREE(my_ip_str);
		return LDNS_STATUS_INVALID_STR;
	}

	my_str = strchr(my_str, '/') + 1;
	prefix = (uint8_t) atoi(my_str);

	data = LDNS_XMALLOC(uint8_t, 4 + afdlength);
        if(!data) {
		LDNS_FREE(afdpart);
		LDNS_FREE(my_ip_str);
		return LDNS_STATUS_INVALID_STR;
        }
	ldns_write_uint16(data, family);
	data[2] = prefix;
	data[3] = afdlength;
	if (negation) {
		/* set bit 1 of byte 3 */
		data[3] = data[3] | 0x80;
	}

	memcpy(data + 4, afdpart, afdlength);

	*rd = ldns_rdf_new_frm_data(LDNS_RDF_TYPE_APL, afdlength + 4, data);
	LDNS_FREE(afdpart);
	LDNS_FREE(data);
	LDNS_FREE(my_ip_str);

	return *rd?LDNS_STATUS_OK:LDNS_STATUS_MEM_ERR;
}

ldns_status
ldns_str2rdf_b64(ldns_rdf **rd, const char *str)
{
	uint8_t *buffer;
	int16_t i;

	buffer = LDNS_XMALLOC(uint8_t, ldns_b64_ntop_calculate_size(strlen(str)));
        if(!buffer) {
                return LDNS_STATUS_MEM_ERR;
        }

	i = (uint16_t)ldns_b64_pton((const char*)str, buffer,
						   ldns_b64_ntop_calculate_size(strlen(str)));
	if (-1 == i) {
		LDNS_FREE(buffer);
		return LDNS_STATUS_INVALID_B64;
	} else {
		*rd = ldns_rdf_new_frm_data(
			LDNS_RDF_TYPE_B64, (uint16_t) i, buffer);
	}
	LDNS_FREE(buffer);

	return *rd?LDNS_STATUS_OK:LDNS_STATUS_MEM_ERR;
}

ldns_status
ldns_str2rdf_b32_ext(ldns_rdf **rd, const char *str)
{
	uint8_t *buffer;
	int i;
	/* first byte contains length of actual b32 data */
	uint8_t len = ldns_b32_pton_calculate_size(strlen(str));
	buffer = LDNS_XMALLOC(uint8_t, len + 1);
        if(!buffer) {
                return LDNS_STATUS_MEM_ERR;
        }
	buffer[0] = len;

	i = ldns_b32_pton_extended_hex((const char*)str, strlen(str), buffer + 1,
							 ldns_b32_ntop_calculate_size(strlen(str)));
	if (i < 0) {
                LDNS_FREE(buffer);
		return LDNS_STATUS_INVALID_B32_EXT;
	} else {
		*rd = ldns_rdf_new_frm_data(
			LDNS_RDF_TYPE_B32_EXT, (uint16_t) i + 1, buffer);
	}
	LDNS_FREE(buffer);

	return *rd?LDNS_STATUS_OK:LDNS_STATUS_MEM_ERR;
}

ldns_status
ldns_str2rdf_hex(ldns_rdf **rd, const char *str)
{
	uint8_t *t, *t_orig;
	int i;
	size_t len;

	len = strlen(str);

	if (len > LDNS_MAX_RDFLEN * 2) {
		return LDNS_STATUS_LABEL_OVERFLOW;
	} else {
		t = LDNS_XMALLOC(uint8_t, (len / 2) + 1);
                if(!t) {
                        return LDNS_STATUS_MEM_ERR;
                }
		t_orig = t;
		/* Now process octet by octet... */
		while (*str) {
			*t = 0;
			if (isspace((int) *str)) {
				str++;
			} else {
				for (i = 16; i >= 1; i -= 15) {
					while (*str && isspace((int) *str)) { str++; }
					if (*str) {
						if (isxdigit((int) *str)) {
							*t += ldns_hexdigit_to_int(*str) * i;
						} else {
                                                        LDNS_FREE(t_orig);
							return LDNS_STATUS_ERR;
						}
						++str;
					}
				}
				++t;
			}
		}
		*rd = ldns_rdf_new_frm_data(LDNS_RDF_TYPE_HEX,
		                            (size_t) (t - t_orig),
		                            t_orig);
		LDNS_FREE(t_orig);
	}
	return *rd?LDNS_STATUS_OK:LDNS_STATUS_MEM_ERR;
}

ldns_status
ldns_str2rdf_nsec(ldns_rdf **rd, const char *str)
{
	const char *delimiters = "\n\t ";
	char *token = LDNS_XMALLOC(char, LDNS_MAX_RDFLEN);
	ldns_buffer *str_buf;
	ssize_t c;
	uint16_t cur_type;
	size_t type_count = 0;
	ldns_rr_type type_list[65536];
	if(!token) return LDNS_STATUS_MEM_ERR;
	if(rd == NULL) {
		LDNS_FREE(token);
		return LDNS_STATUS_NULL;
	}

	str_buf = LDNS_MALLOC(ldns_buffer);
	if(!str_buf) {
		LDNS_FREE(token);
		return LDNS_STATUS_MEM_ERR;
	}
	ldns_buffer_new_frm_data(str_buf, (char *)str, strlen(str));
	if(ldns_buffer_status(str_buf) != LDNS_STATUS_OK) {
		LDNS_FREE(str_buf);
		LDNS_FREE(token);
		return LDNS_STATUS_MEM_ERR;
	}

	while ((c = ldns_bget_token(str_buf, token, delimiters, LDNS_MAX_RDFLEN)) != -1 && c != 0) {
                if(type_count >= sizeof(type_list)) {
		        LDNS_FREE(str_buf);
		        LDNS_FREE(token);
                        return LDNS_STATUS_ERR;
                }
		cur_type = ldns_get_rr_type_by_name(token);
		type_list[type_count] = cur_type;
		type_count++;
	}

	*rd = ldns_dnssec_create_nsec_bitmap(type_list,
	                                     type_count,
	                                     LDNS_RR_TYPE_NSEC);

	LDNS_FREE(token);
	ldns_buffer_free(str_buf);
	return *rd?LDNS_STATUS_OK:LDNS_STATUS_MEM_ERR;
}

ldns_status
ldns_str2rdf_type(ldns_rdf **rd, const char *str)
{
	uint16_t type;
	type = htons(ldns_get_rr_type_by_name(str));
	/* ldns_rr_type is a 16 bit value */
	*rd = ldns_rdf_new_frm_data(
		LDNS_RDF_TYPE_TYPE, sizeof(uint16_t), &type);
	return *rd?LDNS_STATUS_OK:LDNS_STATUS_MEM_ERR;
}

ldns_status
ldns_str2rdf_class(ldns_rdf **rd, const char *str)
{
	uint16_t klass;
	klass = htons(ldns_get_rr_class_by_name(str));
	/* class is 16 bit */
	*rd = ldns_rdf_new_frm_data(
		LDNS_RDF_TYPE_CLASS, sizeof(uint16_t), &klass);
	return *rd?LDNS_STATUS_OK:LDNS_STATUS_MEM_ERR;
}

/* An certificate alg field can either be specified as a 8 bits number
 * or by its symbolic name. Handle both
 */
ldns_status
ldns_str2rdf_cert_alg(ldns_rdf **rd, const char *str)
{
	ldns_lookup_table *lt;
	ldns_status st;
	uint8_t idd[2];
	lt = ldns_lookup_by_name(ldns_cert_algorithms, str);
	st = LDNS_STATUS_OK;

	if (lt) {
		ldns_write_uint16(idd, (uint16_t) lt->id);
		*rd = ldns_rdf_new_frm_data(
			LDNS_RDF_TYPE_INT16, sizeof(uint16_t), idd);
		if (!*rd) {
			st = LDNS_STATUS_ERR;
		}
	} else {
		/* try as-is (a number) */
		st = ldns_str2rdf_int16(rd, str);
		if (st == LDNS_STATUS_OK &&
		    ldns_rdf2native_int16(*rd) == 0) {
			st = LDNS_STATUS_CERT_BAD_ALGORITHM;
		}
	}

	return st;
}

/* An alg field can either be specified as a 8 bits number
 * or by its symbolic name. Handle both
 */
ldns_status
ldns_str2rdf_alg(ldns_rdf **rd, const char *str)
{
	ldns_lookup_table *lt;
	ldns_status st;

	lt = ldns_lookup_by_name(ldns_algorithms, str);
	st = LDNS_STATUS_OK;

	if (lt) {
		/* it was given as a integer */
		*rd = ldns_native2rdf_int8(LDNS_RDF_TYPE_INT8, (uint8_t) lt->id);
		if (!*rd) {
			st = LDNS_STATUS_ERR;
		}
	} else {
		/* try as-is (a number) */
		st = ldns_str2rdf_int8(rd, str);
	}
	return st;
}

ldns_status
ldns_str2rdf_unknown( ATTR_UNUSED(ldns_rdf **rd)
		    , ATTR_UNUSED(const char *str)
		    )
{
	/* this should be caught in an earlier time (general str2host for
	   rr's */
	return LDNS_STATUS_NOT_IMPL;
}

ldns_status
ldns_str2rdf_tsig( ATTR_UNUSED(ldns_rdf **rd)
		 , ATTR_UNUSED(const char *str)
		 )
{
	/* there is no string representation for TSIG rrs */
	return LDNS_STATUS_NOT_IMPL;
}

ldns_status
ldns_str2rdf_service( ATTR_UNUSED(ldns_rdf **rd)
		    , ATTR_UNUSED(const char *str)
		    )
{
	/* is this used? is this actually WKS? or SRV? */
	return LDNS_STATUS_NOT_IMPL;
}

static int
loc_parse_cm(char* my_str, char** endstr, uint8_t* m, uint8_t* e)
{
	/* read <digits>[.<digits>][mM] */
	/* into mantissa exponent format for LOC type */
	uint32_t meters = 0, cm = 0, val;
	while (isblank(*my_str)) {
		my_str++;
	}
	meters = (uint32_t)strtol(my_str, &my_str, 10);
	if (*my_str == '.') {
		my_str++;
		cm = (uint32_t)strtol(my_str, &my_str, 10);
	}
	if (meters >= 1) {
		*e = 2;
		val = meters;
	} else	{
		*e = 0;
		val = cm;
	}
	while(val >= 10) {
		(*e)++;
		val /= 10;
	}
	*m = (uint8_t)val;

	if (*e > 9)
		return 0;
	if (*my_str == 'm' || *my_str == 'M') {
		my_str++;
	}
	*endstr = my_str;
	return 1;
}

ldns_status
ldns_str2rdf_loc(ldns_rdf **rd, const char *str)
{
	uint32_t latitude = 0;
	uint32_t longitude = 0;
	uint32_t altitude = 0;

	uint8_t *data;
	uint32_t equator = (uint32_t) ldns_power(2, 31);

	uint32_t h = 0;
	uint32_t m = 0;
	uint8_t size_b = 1, size_e = 2;
	uint8_t horiz_pre_b = 1, horiz_pre_e = 6;
	uint8_t vert_pre_b = 1, vert_pre_e = 3;

	double s = 0.0;
	bool northerness;
	bool easterness;

	char *my_str = (char *) str;

	/* only support version 0 */
	if (isdigit((int) *my_str)) {
		h = (uint32_t) strtol(my_str, &my_str, 10);
	} else {
		return LDNS_STATUS_INVALID_STR;
	}

	while (isblank((int) *my_str)) {
		my_str++;
	}

	if (isdigit((int) *my_str)) {
		m = (uint32_t) strtol(my_str, &my_str, 10);
	} else if (*my_str == 'N' || *my_str == 'S') {
		goto north;
	} else {
		return LDNS_STATUS_INVALID_STR;
	}

	while (isblank((int) *my_str)) {
		my_str++;
	}

	if (isdigit((int) *my_str)) {
		s = strtod(my_str, &my_str);
	}
north:
	while (isblank((int) *my_str)) {
		my_str++;
	}

	if (*my_str == 'N') {
		northerness = true;
	} else if (*my_str == 'S') {
		northerness = false;
	} else {
		return LDNS_STATUS_INVALID_STR;
	}

	my_str++;

	/* store number */
	s = 1000.0 * s;
	/* add a little to make floor in conversion a round */
	s += 0.0005;
	latitude = (uint32_t) s;
	latitude += 1000 * 60 * m;
	latitude += 1000 * 60 * 60 * h;
	if (northerness) {
		latitude = equator + latitude;
	} else {
		latitude = equator - latitude;
	}
	while (isblank(*my_str)) {
		my_str++;
	}

	if (isdigit((int) *my_str)) {
		h = (uint32_t) strtol(my_str, &my_str, 10);
	} else {
		return LDNS_STATUS_INVALID_STR;
	}

	while (isblank((int) *my_str)) {
		my_str++;
	}

	if (isdigit((int) *my_str)) {
		m = (uint32_t) strtol(my_str, &my_str, 10);
	} else if (*my_str == 'E' || *my_str == 'W') {
		goto east;
	} else {
		return LDNS_STATUS_INVALID_STR;
	}

	while (isblank(*my_str)) {
		my_str++;
	}

	if (isdigit((int) *my_str)) {
		s = strtod(my_str, &my_str);
	}

east:
	while (isblank(*my_str)) {
		my_str++;
	}

	if (*my_str == 'E') {
		easterness = true;
	} else if (*my_str == 'W') {
		easterness = false;
	} else {
		return LDNS_STATUS_INVALID_STR;
	}

	my_str++;

	/* store number */
	s *= 1000.0;
	/* add a little to make floor in conversion a round */
	s += 0.0005;
	longitude = (uint32_t) s;
	longitude += 1000 * 60 * m;
	longitude += 1000 * 60 * 60 * h;

	if (easterness) {
		longitude += equator;
	} else {
		longitude = equator - longitude;
	}

	altitude = (uint32_t)(strtod(my_str, &my_str)*100.0 +
		10000000.0 + 0.5);
	if (*my_str == 'm' || *my_str == 'M') {
		my_str++;
	}

	if (strlen(my_str) > 0) {
		if(!loc_parse_cm(my_str, &my_str, &size_b, &size_e))
			return LDNS_STATUS_INVALID_STR;
	}

	if (strlen(my_str) > 0) {
		if(!loc_parse_cm(my_str, &my_str, &horiz_pre_b, &horiz_pre_e))
			return LDNS_STATUS_INVALID_STR;
	}

	if (strlen(my_str) > 0) {
		if(!loc_parse_cm(my_str, &my_str, &vert_pre_b, &vert_pre_e))
			return LDNS_STATUS_INVALID_STR;
	}

	data = LDNS_XMALLOC(uint8_t, 16);
        if(!data) {
                return LDNS_STATUS_MEM_ERR;
        }
	data[0] = 0;
	data[1] = 0;
	data[1] = ((size_b << 4) & 0xf0) | (size_e & 0x0f);
	data[2] = ((horiz_pre_b << 4) & 0xf0) | (horiz_pre_e & 0x0f);
	data[3] = ((vert_pre_b << 4) & 0xf0) | (vert_pre_e & 0x0f);
	ldns_write_uint32(data + 4, latitude);
	ldns_write_uint32(data + 8, longitude);
	ldns_write_uint32(data + 12, altitude);

	*rd = ldns_rdf_new_frm_data(
		LDNS_RDF_TYPE_LOC, 16, data);

	LDNS_FREE(data);
	return *rd?LDNS_STATUS_OK:LDNS_STATUS_MEM_ERR;
}

ldns_status
ldns_str2rdf_wks(ldns_rdf **rd, const char *str)
{
	uint8_t *bitmap = NULL;
	uint8_t *data;
	int bm_len = 0;

	struct protoent *proto = NULL;
	struct servent *serv = NULL;
	int serv_port;

	ldns_buffer *str_buf;

	char *proto_str = NULL;
	char *token;
	if(strlen(str) == 0)
		token = LDNS_XMALLOC(char, 50);
	else 	token = LDNS_XMALLOC(char, strlen(str)+2);
	if(!token) return LDNS_STATUS_MEM_ERR;

	str_buf = LDNS_MALLOC(ldns_buffer);
	if(!str_buf) {LDNS_FREE(token); return LDNS_STATUS_MEM_ERR;}
	ldns_buffer_new_frm_data(str_buf, (char *)str, strlen(str));
	if(ldns_buffer_status(str_buf) != LDNS_STATUS_OK) {
		LDNS_FREE(str_buf);
		LDNS_FREE(token);
		return LDNS_STATUS_MEM_ERR;
	}

	while(ldns_bget_token(str_buf, token, "\t\n ", strlen(str)) > 0) {
		if (!proto_str) {
			proto_str = strdup(token);
			if (!proto_str) {
				LDNS_FREE(bitmap);
				LDNS_FREE(token);
	                        ldns_buffer_free(str_buf);
				return LDNS_STATUS_INVALID_STR;
			}
		} else {
			serv = getservbyname(token, proto_str);
			if (serv) {
				serv_port = (int) ntohs((uint16_t) serv->s_port);
			} else {
				serv_port = atoi(token);
			}
			if (serv_port / 8 >= bm_len) {
				uint8_t *b2 = LDNS_XREALLOC(bitmap, uint8_t, (serv_port / 8) + 1);
                                if(!b2) {
					LDNS_FREE(bitmap);
				        LDNS_FREE(token);
	                                ldns_buffer_free(str_buf);
				        free(proto_str);
				        return LDNS_STATUS_INVALID_STR;
                                }
				bitmap = b2;
				/* set to zero to be sure */
				for (; bm_len <= serv_port / 8; bm_len++) {
					bitmap[bm_len] = 0;
				}
			}
			ldns_set_bit(bitmap + (serv_port / 8), 7 - (serv_port % 8), true);
		}
	}

	if (!proto_str || !bitmap) {
		LDNS_FREE(bitmap);
		LDNS_FREE(token);
	        ldns_buffer_free(str_buf);
	        free(proto_str);
		return LDNS_STATUS_INVALID_STR;
	}

	data = LDNS_XMALLOC(uint8_t, bm_len + 1);
        if(!data) {
	        LDNS_FREE(token);
	        ldns_buffer_free(str_buf);
	        LDNS_FREE(bitmap);
	        free(proto_str);
	        return LDNS_STATUS_INVALID_STR;
        }
    if (proto_str)
		proto = getprotobyname(proto_str);
	if (proto) {
		data[0] = (uint8_t) proto->p_proto;
	} else if (proto_str) {
		data[0] = (uint8_t) atoi(proto_str);
	}
	memcpy(data + 1, bitmap, (size_t) bm_len);

	*rd = ldns_rdf_new_frm_data(LDNS_RDF_TYPE_WKS, (uint16_t) (bm_len + 1), data);

	LDNS_FREE(data);
	LDNS_FREE(token);
	ldns_buffer_free(str_buf);
	LDNS_FREE(bitmap);
	free(proto_str);
#ifdef HAVE_ENDSERVENT
	endservent();
#endif
#ifdef HAVE_ENDPROTOENT
	endprotoent();
#endif

	if(!*rd) return LDNS_STATUS_MEM_ERR;

	return LDNS_STATUS_OK;
}

ldns_status
ldns_str2rdf_nsap(ldns_rdf **rd, const char *str)
{
    size_t len, i;
    char* nsap_str = (char*) str;

	/* just a hex string with optional dots? */
	if (str[0] != '0' || str[1] != 'x') {
		return LDNS_STATUS_INVALID_STR;
	} else {
		len = strlen(str);
		for (i=0; i < len; i++) {
			if (nsap_str[i] == '.')
				nsap_str[i] = ' ';
        }
		return ldns_str2rdf_hex(rd, str+2);
	}
}

ldns_status
ldns_str2rdf_atma(ldns_rdf **rd, const char *str)
{
    size_t len, i;
    char* atma_str = (char*) str;
	ldns_status status;

	/* just a hex string with optional dots? */
	len = strlen(str);
	for (i=0; i < len; i++) {
		if (atma_str[i] == '.')
			atma_str[i] = ' ';
	}
	status = ldns_str2rdf_hex(rd, str);
    if (status != LDNS_STATUS_OK) {
		; /* probably in e.164 format than */
	}
	return status;
}

ldns_status
ldns_str2rdf_ipseckey(ldns_rdf **rd, const char *str)
{
	uint8_t precedence = 0;
	uint8_t gateway_type = 0;
	uint8_t algorithm = 0;
	char* gateway = NULL;
	char* publickey = NULL;
	uint8_t *data;
	ldns_buffer *str_buf;
	char *token;
	int token_count = 0;
	int ipseckey_len = 0;
	ldns_rdf* gateway_rdf = NULL;
	ldns_rdf* publickey_rdf = NULL;
	ldns_status status = LDNS_STATUS_OK;
	
	if(strlen(str) == 0)
		token = LDNS_XMALLOC(char, 256);
	else	token = LDNS_XMALLOC(char, strlen(str)+2);
	if(!token) return LDNS_STATUS_MEM_ERR;

	str_buf = LDNS_MALLOC(ldns_buffer);
	if(!str_buf) {LDNS_FREE(token); return LDNS_STATUS_MEM_ERR;}
	ldns_buffer_new_frm_data(str_buf, (char *)str, strlen(str));
	if(ldns_buffer_status(str_buf) != LDNS_STATUS_OK) {
		LDNS_FREE(str_buf);
		LDNS_FREE(token);
		return LDNS_STATUS_MEM_ERR;
	}
	while(ldns_bget_token(str_buf, token, "\t\n ", strlen(str)) > 0) {
		switch (token_count) {
				case 0:
					precedence = (uint8_t)atoi(token);
					break;
				case 1:
					gateway_type = (uint8_t)atoi(token);
					break;
				case 2:
					algorithm = (uint8_t)atoi(token);
					break;
				case 3:
					gateway = strdup(token);
					if (!gateway || (gateway_type == 0 &&
							(token[0] != '.' || token[1] != '\0'))) {
						LDNS_FREE(gateway);
						LDNS_FREE(token);
						ldns_buffer_free(str_buf);
						return LDNS_STATUS_INVALID_STR;
					}
					break;
				case 4:
					publickey = strdup(token);
					break;
				default:
					LDNS_FREE(token);
					ldns_buffer_free(str_buf);
					return LDNS_STATUS_INVALID_STR;
					break;
		}
		token_count++;
	}

	if (!gateway || !publickey) {
		if (gateway)
			LDNS_FREE(gateway);
		if (publickey)
			LDNS_FREE(publickey);
		LDNS_FREE(token);
		ldns_buffer_free(str_buf);
		return LDNS_STATUS_INVALID_STR;
	}

	if (gateway_type == 1) {
		status = ldns_str2rdf_a(&gateway_rdf, gateway);
	} else if (gateway_type == 2) {
		status = ldns_str2rdf_aaaa(&gateway_rdf, gateway);
	} else if (gateway_type == 3) {
		status = ldns_str2rdf_dname(&gateway_rdf, gateway);
	}

	if (status != LDNS_STATUS_OK) {
		if (gateway)
			LDNS_FREE(gateway);
		if (publickey)
			LDNS_FREE(publickey);
		LDNS_FREE(token);
		ldns_buffer_free(str_buf);
		return LDNS_STATUS_INVALID_STR;
	}

	status = ldns_str2rdf_b64(&publickey_rdf, publickey);

	if (status != LDNS_STATUS_OK) {
		if (gateway)
			LDNS_FREE(gateway);
		if (publickey)
			LDNS_FREE(publickey);
		LDNS_FREE(token);
		ldns_buffer_free(str_buf);
		if (gateway_rdf) ldns_rdf_free(gateway_rdf);
		return LDNS_STATUS_INVALID_STR;
	}

	/* now copy all into one ipseckey rdf */
	if (gateway_type)
		ipseckey_len = 3 + (int)ldns_rdf_size(gateway_rdf) + (int)ldns_rdf_size(publickey_rdf);
	else
		ipseckey_len = 3 + (int)ldns_rdf_size(publickey_rdf);

	data = LDNS_XMALLOC(uint8_t, ipseckey_len);
	if(!data) {
		if (gateway)
			LDNS_FREE(gateway);
		if (publickey)
			LDNS_FREE(publickey);
		LDNS_FREE(token);
		ldns_buffer_free(str_buf);
		if (gateway_rdf) ldns_rdf_free(gateway_rdf);
		if (publickey_rdf) ldns_rdf_free(publickey_rdf);
		return LDNS_STATUS_MEM_ERR;
	}

	data[0] = precedence;
	data[1] = gateway_type;
	data[2] = algorithm;

	if (gateway_type) {
		memcpy(data + 3,
			ldns_rdf_data(gateway_rdf), ldns_rdf_size(gateway_rdf));
		memcpy(data + 3 + ldns_rdf_size(gateway_rdf),
			ldns_rdf_data(publickey_rdf), ldns_rdf_size(publickey_rdf));
	} else {
		memcpy(data + 3,
			ldns_rdf_data(publickey_rdf), ldns_rdf_size(publickey_rdf));
	}

	*rd = ldns_rdf_new_frm_data(LDNS_RDF_TYPE_IPSECKEY, (uint16_t) ipseckey_len, data);

	if (gateway)
		LDNS_FREE(gateway);
	if (publickey)
		LDNS_FREE(publickey);
	LDNS_FREE(token);
	ldns_buffer_free(str_buf);
	ldns_rdf_free(gateway_rdf);
	ldns_rdf_free(publickey_rdf);
	LDNS_FREE(data);
	if(!*rd) return LDNS_STATUS_MEM_ERR;
	return LDNS_STATUS_OK;
}

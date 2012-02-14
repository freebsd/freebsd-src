/*
 * UPnP WPS Device - Web connections
 * Copyright (c) 2000-2003 Intel Corporation
 * Copyright (c) 2006-2007 Sony Corporation
 * Copyright (c) 2008-2009 Atheros Communications
 * Copyright (c) 2009, Jouni Malinen <j@w1.fi>
 *
 * See wps_upnp.c for more details on licensing and code history.
 */

#include "includes.h"
#include <fcntl.h>

#include "common.h"
#include "base64.h"
#include "eloop.h"
#include "uuid.h"
#include "httpread.h"
#include "wps_i.h"
#include "wps_upnp.h"
#include "wps_upnp_i.h"

/***************************************************************************
 * Web connections (we serve pages of info about ourselves, handle
 * requests, etc. etc.).
 **************************************************************************/

#define WEB_CONNECTION_TIMEOUT_SEC 30   /* Drop web connection after t.o. */
#define WEB_CONNECTION_MAX_READ 8000    /* Max we'll read for TCP request */
#define MAX_WEB_CONNECTIONS 10          /* max simultaneous web connects */


static const char *urn_wfawlanconfig =
	"urn:schemas-wifialliance-org:service:WFAWLANConfig:1";
static const char *http_server_hdr =
	"Server: unspecified, UPnP/1.0, unspecified\r\n";
static const char *http_connection_close =
	"Connection: close\r\n";

/*
 * Incoming web connections are recorded in this struct.
 * A web connection is a TCP connection to us, the server;
 * it is called a "web connection" because we use http and serve
 * data that looks like web pages.
 * State information is need to track the connection until we figure
 * out what they want and what we want to do about it.
 */
struct web_connection {
	/* double linked list */
	struct web_connection *next;
	struct web_connection *prev;
	struct upnp_wps_device_sm *sm; /* parent */
	int sd; /* socket to read from */
	struct sockaddr_in cli_addr;
	int sd_registered; /* nonzero if we must cancel registration */
	struct httpread *hread; /* state machine for reading socket */
	int n_rcvd_data; /* how much data read so far */
	int done; /* internal flag, set when we've finished */
};


/*
 * XML parsing and formatting
 *
 * XML is a markup language based on unicode; usually (and in our case,
 * always!) based on utf-8. utf-8 uses a variable number of bytes per
 * character. utf-8 has the advantage that all non-ASCII unicode characters are
 * represented by sequences of non-ascii (high bit set) bytes, whereas ASCII
 * characters are single ascii bytes, thus we can use typical text processing.
 *
 * (One other interesting thing about utf-8 is that it is possible to look at
 * any random byte and determine if it is the first byte of a character as
 * versus a continuation byte).
 *
 * The base syntax of XML uses a few ASCII punctionation characters; any
 * characters that would appear in the payload data are rewritten using
 * sequences, e.g., &amp; for ampersand(&) and &lt for left angle bracket (<).
 * Five such escapes total (more can be defined but that does not apply to our
 * case). Thus we can safely parse for angle brackets etc.
 *
 * XML describes tree structures of tagged data, with each element beginning
 * with an opening tag <label> and ending with a closing tag </label> with
 * matching label. (There is also a self-closing tag <label/> which is supposed
 * to be equivalent to <label></label>, i.e., no payload, but we are unlikely
 * to see it for our purpose).
 *
 * Actually the opening tags are a little more complicated because they can
 * contain "attributes" after the label (delimited by ascii space or tab chars)
 * of the form attribute_label="value" or attribute_label='value'; as it turns
 * out we do not have to read any of these attributes, just ignore them.
 *
 * Labels are any sequence of chars other than space, tab, right angle bracket
 * (and ?), but may have an inner structure of <namespace><colon><plain_label>.
 * As it turns out, we can ignore the namespaces, in fact we can ignore the
 * entire tree hierarchy, because the plain labels we are looking for will be
 * unique (not in general, but for this application). We do however have to be
 * careful to skip over the namespaces.
 *
 * In generating XML we have to be more careful, but that is easy because
 * everything we do is pretty canned. The only real care to take is to escape
 * any special chars in our payload.
 */

/**
 * xml_next_tag - Advance to next tag
 * @in: Input
 * @out: OUT: start of tag just after '<'
 * @out_tagname: OUT: start of name of tag, skipping namespace
 * @end: OUT: one after tag
 * Returns: 0 on success, 1 on failure
 *
 * A tag has form:
 *     <left angle bracket><...><right angle bracket>
 * Within the angle brackets, there is an optional leading forward slash (which
 * makes the tag an ending tag), then an optional leading label (followed by
 * colon) and then the tag name itself.
 *
 * Note that angle brackets present in the original data must have been encoded
 * as &lt; and &gt; so they will not trouble us.
 */
static int xml_next_tag(char *in, char **out, char **out_tagname,
			char **end)
{
	while (*in && *in != '<')
		in++;
	if (*in != '<')
		return 1;
	*out = ++in;
	if (*in == '/')
		in++;
	*out_tagname = in; /* maybe */
	while (isalnum(*in) || *in == '-')
		in++;
	if (*in == ':')
		*out_tagname = ++in;
	while (*in && *in != '>')
		in++;
	if (*in != '>')
		return 1;
	*end = ++in;
	return 0;
}


/* xml_data_encode -- format data for xml file, escaping special characters.
 *
 * Note that we assume we are using utf8 both as input and as output!
 * In utf8, characters may be classed as follows:
 *     0xxxxxxx(2) -- 1 byte ascii char
 *     11xxxxxx(2) -- 1st byte of multi-byte char w/ unicode value >= 0x80
 *         110xxxxx(2) -- 1st byte of 2 byte sequence (5 payload bits here)
 *         1110xxxx(2) -- 1st byte of 3 byte sequence (4 payload bits here)
 *         11110xxx(2) -- 1st byte of 4 byte sequence (3 payload bits here)
 *      10xxxxxx(2) -- extension byte (6 payload bits per byte)
 *      Some values implied by the above are however illegal because they
 *      do not represent unicode chars or are not the shortest encoding.
 * Actually, we can almost entirely ignore the above and just do
 * text processing same as for ascii text.
 *
 * XML is written with arbitrary unicode characters, except that five
 * characters have special meaning and so must be escaped where they
 * appear in payload data... which we do here.
 */
static void xml_data_encode(struct wpabuf *buf, const char *data, int len)
{
	int i;
	for (i = 0; i < len; i++) {
		u8 c = ((u8 *) data)[i];
		if (c == '<') {
			wpabuf_put_str(buf, "&lt;");
			continue;
		}
		if (c == '>') {
			wpabuf_put_str(buf, "&gt;");
			continue;
		}
		if (c == '&') {
			wpabuf_put_str(buf, "&amp;");
			continue;
		}
		if (c == '\'') {
			wpabuf_put_str(buf, "&apos;");
			continue;
		}
		if (c == '"') {
			wpabuf_put_str(buf, "&quot;");
			continue;
		}
		/*
		 * We could try to represent control characters using the
		 * sequence: &#x; where x is replaced by a hex numeral, but not
		 * clear why we would do this.
		 */
		wpabuf_put_u8(buf, c);
	}
}


/* xml_add_tagged_data -- format tagged data as a new xml line.
 *
 * tag must not have any special chars.
 * data may have special chars, which are escaped.
 */
static void xml_add_tagged_data(struct wpabuf *buf, const char *tag,
				const char *data)
{
	wpabuf_printf(buf, "<%s>", tag);
	xml_data_encode(buf, data, os_strlen(data));
	wpabuf_printf(buf, "</%s>\n", tag);
}


/* A POST body looks something like (per upnp spec):
 * <?xml version="1.0"?>
 * <s:Envelope
 *     xmlns:s="http://schemas.xmlsoap.org/soap/envelope/"
 *     s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
 *   <s:Body>
 *     <u:actionName xmlns:u="urn:schemas-upnp-org:service:serviceType:v">
 *       <argumentName>in arg value</argumentName>
 *       other in args and their values go here, if any
 *     </u:actionName>
 *   </s:Body>
 * </s:Envelope>
 *
 * where :
 *      s: might be some other namespace name followed by colon
 *      u: might be some other namespace name followed by colon
 *      actionName will be replaced according to action requested
 *      schema following actionName will be WFA scheme instead
 *      argumentName will be actual argument name
 *      (in arg value) will be actual argument value
 */
static int
upnp_get_first_document_item(char *doc, const char *item, char **value)
{
	const char *match = item;
	int match_len = os_strlen(item);
	char *tag;
	char *tagname;
	char *end;

	*value = NULL;          /* default, bad */

	/*
	 * This is crude: ignore any possible tag name conflicts and go right
	 * to the first tag of this name. This should be ok for the limited
	 * domain of UPnP messages.
	 */
	for (;;) {
		if (xml_next_tag(doc, &tag, &tagname, &end))
			return 1;
		doc = end;
		if (!os_strncasecmp(tagname, match, match_len) &&
		    *tag != '/' &&
		    (tagname[match_len] == '>' ||
		     !isgraph(tagname[match_len]))) {
			break;
		}
	}
	end = doc;
	while (*end && *end != '<')
		end++;
	*value = os_zalloc(1 + (end - doc));
	if (*value == NULL)
		return 1;
	os_memcpy(*value, doc, end - doc);
	return 0;
}


/*
 * "Files" that we serve via HTTP. The format of these files is given by
 * WFA WPS specifications. Extra white space has been removed to save space.
 */

static const char wps_scpd_xml[] =
"<?xml version=\"1.0\"?>\n"
"<scpd xmlns=\"urn:schemas-upnp-org:service-1-0\">\n"
"<specVersion><major>1</major><minor>0</minor></specVersion>\n"
"<actionList>\n"
"<action>\n"
"<name>GetDeviceInfo</name>\n"
"<argumentList>\n"
"<argument>\n"
"<name>NewDeviceInfo</name>\n"
"<direction>out</direction>\n"
"<relatedStateVariable>DeviceInfo</relatedStateVariable>\n"
"</argument>\n"
"</argumentList>\n"
"</action>\n"
"<action>\n"
"<name>PutMessage</name>\n"
"<argumentList>\n"
"<argument>\n"
"<name>NewInMessage</name>\n"
"<direction>in</direction>\n"
"<relatedStateVariable>InMessage</relatedStateVariable>\n"
"</argument>\n"
"<argument>\n"
"<name>NewOutMessage</name>\n"
"<direction>out</direction>\n"
"<relatedStateVariable>OutMessage</relatedStateVariable>\n"
"</argument>\n"
"</argumentList>\n"
"</action>\n"
"<action>\n"
"<name>GetAPSettings</name>\n"
"<argumentList>\n"
"<argument>\n"
"<name>NewMessage</name>\n"
"<direction>in</direction>\n"
"<relatedStateVariable>Message</relatedStateVariable>\n"
"</argument>\n"
"<argument>\n"
"<name>NewAPSettings</name>\n"
"<direction>out</direction>\n"
"<relatedStateVariable>APSettings</relatedStateVariable>\n"
"</argument>\n"
"</argumentList>\n"
"</action>\n"
"<action>\n"
"<name>SetAPSettings</name>\n"
"<argumentList>\n"
"<argument>\n"
"<name>APSettings</name>\n"
"<direction>in</direction>\n"
"<relatedStateVariable>APSettings</relatedStateVariable>\n"
"</argument>\n"
"</argumentList>\n"
"</action>\n"
"<action>\n"
"<name>DelAPSettings</name>\n"
"<argumentList>\n"
"<argument>\n"
"<name>NewAPSettings</name>\n"
"<direction>in</direction>\n"
"<relatedStateVariable>APSettings</relatedStateVariable>\n"
"</argument>\n"
"</argumentList>\n"
"</action>\n"
"<action>\n"
"<name>GetSTASettings</name>\n"
"<argumentList>\n"
"<argument>\n"
"<name>NewMessage</name>\n"
"<direction>in</direction>\n"
"<relatedStateVariable>Message</relatedStateVariable>\n"
"</argument>\n"
"<argument>\n"
"<name>NewSTASettings</name>\n"
"<direction>out</direction>\n"
"<relatedStateVariable>STASettings</relatedStateVariable>\n"
"</argument>\n"
"</argumentList>\n"
"</action>\n"
"<action>\n"
"<name>SetSTASettings</name>\n"
"<argumentList>\n"
"<argument>\n"
"<name>NewSTASettings</name>\n"
"<direction>out</direction>\n"
"<relatedStateVariable>STASettings</relatedStateVariable>\n"
"</argument>\n"
"</argumentList>\n"
"</action>\n"
"<action>\n"
"<name>DelSTASettings</name>\n"
"<argumentList>\n"
"<argument>\n"
"<name>NewSTASettings</name>\n"
"<direction>in</direction>\n"
"<relatedStateVariable>STASettings</relatedStateVariable>\n"
"</argument>\n"
"</argumentList>\n"
"</action>\n"
"<action>\n"
"<name>PutWLANResponse</name>\n"
"<argumentList>\n"
"<argument>\n"
"<name>NewMessage</name>\n"
"<direction>in</direction>\n"
"<relatedStateVariable>Message</relatedStateVariable>\n"
"</argument>\n"
"<argument>\n"
"<name>NewWLANEventType</name>\n"
"<direction>in</direction>\n"
"<relatedStateVariable>WLANEventType</relatedStateVariable>\n"
"</argument>\n"
"<argument>\n"
"<name>NewWLANEventMAC</name>\n"
"<direction>in</direction>\n"
"<relatedStateVariable>WLANEventMAC</relatedStateVariable>\n"
"</argument>\n"
"</argumentList>\n"
"</action>\n"
"<action>\n"
"<name>SetSelectedRegistrar</name>\n"
"<argumentList>\n"
"<argument>\n"
"<name>NewMessage</name>\n"
"<direction>in</direction>\n"
"<relatedStateVariable>Message</relatedStateVariable>\n"
"</argument>\n"
"</argumentList>\n"
"</action>\n"
"<action>\n"
"<name>RebootAP</name>\n"
"<argumentList>\n"
"<argument>\n"
"<name>NewAPSettings</name>\n"
"<direction>in</direction>\n"
"<relatedStateVariable>APSettings</relatedStateVariable>\n"
"</argument>\n"
"</argumentList>\n"
"</action>\n"
"<action>\n"
"<name>ResetAP</name>\n"
"<argumentList>\n"
"<argument>\n"
"<name>NewMessage</name>\n"
"<direction>in</direction>\n"
"<relatedStateVariable>Message</relatedStateVariable>\n"
"</argument>\n"
"</argumentList>\n"
"</action>\n"
"<action>\n"
"<name>RebootSTA</name>\n"
"<argumentList>\n"
"<argument>\n"
"<name>NewSTASettings</name>\n"
"<direction>in</direction>\n"
"<relatedStateVariable>APSettings</relatedStateVariable>\n"
"</argument>\n"
"</argumentList>\n"
"</action>\n"
"<action>\n"
"<name>ResetSTA</name>\n"
"<argumentList>\n"
"<argument>\n"
"<name>NewMessage</name>\n"
"<direction>in</direction>\n"
"<relatedStateVariable>Message</relatedStateVariable>\n"
"</argument>\n"
"</argumentList>\n"
"</action>\n"
"</actionList>\n"
"<serviceStateTable>\n"
"<stateVariable sendEvents=\"no\">\n"
"<name>Message</name>\n"
"<dataType>bin.base64</dataType>\n"
"</stateVariable>\n"
"<stateVariable sendEvents=\"no\">\n"
"<name>InMessage</name>\n"
"<dataType>bin.base64</dataType>\n"
"</stateVariable>\n"
"<stateVariable sendEvents=\"no\">\n"
"<name>OutMessage</name>\n"
"<dataType>bin.base64</dataType>\n"
"</stateVariable>\n"
"<stateVariable sendEvents=\"no\">\n"
"<name>DeviceInfo</name>\n"
"<dataType>bin.base64</dataType>\n"
"</stateVariable>\n"
"<stateVariable sendEvents=\"no\">\n"
"<name>APSettings</name>\n"
"<dataType>bin.base64</dataType>\n"
"</stateVariable>\n"
"<stateVariable sendEvents=\"yes\">\n"
"<name>APStatus</name>\n"
"<dataType>ui1</dataType>\n"
"</stateVariable>\n"
"<stateVariable sendEvents=\"no\">\n"
"<name>STASettings</name>\n"
"<dataType>bin.base64</dataType>\n"
"</stateVariable>\n"
"<stateVariable sendEvents=\"yes\">\n"
"<name>STAStatus</name>\n"
"<dataType>ui1</dataType>\n"
"</stateVariable>\n"
"<stateVariable sendEvents=\"yes\">\n"
"<name>WLANEvent</name>\n"
"<dataType>bin.base64</dataType>\n"
"</stateVariable>\n"
"<stateVariable sendEvents=\"no\">\n"
"<name>WLANEventType</name>\n"
"<dataType>ui1</dataType>\n"
"</stateVariable>\n"
"<stateVariable sendEvents=\"no\">\n"
"<name>WLANEventMAC</name>\n"
"<dataType>string</dataType>\n"
"</stateVariable>\n"
"<stateVariable sendEvents=\"no\">\n"
"<name>WLANResponse</name>\n"
"<dataType>bin.base64</dataType>\n"
"</stateVariable>\n"
"</serviceStateTable>\n"
"</scpd>\n"
;


static const char *wps_device_xml_prefix =
	"<?xml version=\"1.0\"?>\n"
	"<root xmlns=\"urn:schemas-upnp-org:device-1-0\">\n"
	"<specVersion>\n"
	"<major>1</major>\n"
	"<minor>0</minor>\n"
	"</specVersion>\n"
	"<device>\n"
	"<deviceType>urn:schemas-wifialliance-org:device:WFADevice:1"
	"</deviceType>\n";

static const char *wps_device_xml_postfix =
	"<serviceList>\n"
	"<service>\n"
	"<serviceType>urn:schemas-wifialliance-org:service:WFAWLANConfig:1"
	"</serviceType>\n"
	"<serviceId>urn:wifialliance-org:serviceId:WFAWLANConfig1</serviceId>"
	"\n"
	"<SCPDURL>" UPNP_WPS_SCPD_XML_FILE "</SCPDURL>\n"
	"<controlURL>" UPNP_WPS_DEVICE_CONTROL_FILE "</controlURL>\n"
	"<eventSubURL>" UPNP_WPS_DEVICE_EVENT_FILE "</eventSubURL>\n"
	"</service>\n"
	"</serviceList>\n"
	"</device>\n"
	"</root>\n";


/* format_wps_device_xml -- produce content of "file" wps_device.xml
 * (UPNP_WPS_DEVICE_XML_FILE)
 */
static void format_wps_device_xml(struct upnp_wps_device_sm *sm,
				  struct wpabuf *buf)
{
	const char *s;
	char uuid_string[80];

	wpabuf_put_str(buf, wps_device_xml_prefix);

	/*
	 * Add required fields with default values if not configured. Add
	 * optional and recommended fields only if configured.
	 */
	s = sm->wps->friendly_name;
	s = ((s && *s) ? s : "WPS Access Point");
	xml_add_tagged_data(buf, "friendlyName", s);

	s = sm->wps->dev.manufacturer;
	s = ((s && *s) ? s : "");
	xml_add_tagged_data(buf, "manufacturer", s);

	if (sm->wps->manufacturer_url)
		xml_add_tagged_data(buf, "manufacturerURL",
				    sm->wps->manufacturer_url);

	if (sm->wps->model_description)
		xml_add_tagged_data(buf, "modelDescription",
				    sm->wps->model_description);

	s = sm->wps->dev.model_name;
	s = ((s && *s) ? s : "");
	xml_add_tagged_data(buf, "modelName", s);

	if (sm->wps->dev.model_number)
		xml_add_tagged_data(buf, "modelNumber",
				    sm->wps->dev.model_number);

	if (sm->wps->model_url)
		xml_add_tagged_data(buf, "modelURL", sm->wps->model_url);

	if (sm->wps->dev.serial_number)
		xml_add_tagged_data(buf, "serialNumber",
				    sm->wps->dev.serial_number);

	uuid_bin2str(sm->wps->uuid, uuid_string, sizeof(uuid_string));
	s = uuid_string;
	/* Need "uuid:" prefix, thus we can't use xml_add_tagged_data()
	 * easily...
	 */
	wpabuf_put_str(buf, "<UDN>uuid:");
	xml_data_encode(buf, s, os_strlen(s));
	wpabuf_put_str(buf, "</UDN>\n");

	if (sm->wps->upc)
		xml_add_tagged_data(buf, "UPC", sm->wps->upc);

	wpabuf_put_str(buf, wps_device_xml_postfix);
}


void web_connection_stop(struct web_connection *c)
{
	struct upnp_wps_device_sm *sm = c->sm;

	httpread_destroy(c->hread);
	c->hread = NULL;
	close(c->sd);
	c->sd = -1;
	if (c->next == c) {
		sm->web_connections = NULL;
	} else {
		if (sm->web_connections == c)
			sm->web_connections = c->next;
		c->next->prev = c->prev;
		c->prev->next = c->next;
	}
	os_free(c);
	sm->n_web_connections--;
}


static void http_put_reply_code(struct wpabuf *buf, enum http_reply_code code)
{
	wpabuf_put_str(buf, "HTTP/1.1 ");
	switch (code) {
	case HTTP_OK:
		wpabuf_put_str(buf, "200 OK\r\n");
		break;
	case HTTP_BAD_REQUEST:
		wpabuf_put_str(buf, "400 Bad request\r\n");
		break;
	case HTTP_PRECONDITION_FAILED:
		wpabuf_put_str(buf, "412 Precondition failed\r\n");
		break;
	case HTTP_UNIMPLEMENTED:
		wpabuf_put_str(buf, "501 Unimplemented\r\n");
		break;
	case HTTP_INTERNAL_SERVER_ERROR:
	default:
		wpabuf_put_str(buf, "500 Internal server error\r\n");
		break;
	}
}


static void http_put_date(struct wpabuf *buf)
{
	wpabuf_put_str(buf, "Date: ");
	format_date(buf);
	wpabuf_put_str(buf, "\r\n");
}


static void http_put_empty(struct wpabuf *buf, enum http_reply_code code)
{
	http_put_reply_code(buf, code);
	wpabuf_put_str(buf, http_server_hdr);
	wpabuf_put_str(buf, http_connection_close);
	wpabuf_put_str(buf, "Content-Length: 0\r\n"
		       "\r\n");
}


/* Given that we have received a header w/ GET, act upon it
 *
 * Format of GET (case-insensitive):
 *
 * First line must be:
 *      GET /<file> HTTP/1.1
 * Since we don't do anything fancy we just ignore other lines.
 *
 * Our response (if no error) which includes only required lines is:
 * HTTP/1.1 200 OK
 * Connection: close
 * Content-Type: text/xml
 * Date: <rfc1123-date>
 *
 * Header lines must end with \r\n
 * Per RFC 2616, content-length: is not required but connection:close
 * would appear to be required (given that we will be closing it!).
 */
static void web_connection_parse_get(struct web_connection *c, char *filename)
{
	struct upnp_wps_device_sm *sm = c->sm;
	struct wpabuf *buf; /* output buffer, allocated */
	char *put_length_here;
	char *body_start;
	enum {
		GET_DEVICE_XML_FILE,
		GET_SCPD_XML_FILE
	} req;
	size_t extra_len = 0;
	int body_length;
	char len_buf[10];

	/*
	 * It is not required that filenames be case insensitive but it is
	 * allowed and cannot hurt here.
	 */
	if (filename == NULL)
		filename = "(null)"; /* just in case */
	if (os_strcasecmp(filename, UPNP_WPS_DEVICE_XML_FILE) == 0) {
		wpa_printf(MSG_DEBUG, "WPS UPnP: HTTP GET for device XML");
		req = GET_DEVICE_XML_FILE;
		extra_len = 3000;
		if (sm->wps->friendly_name)
			extra_len += os_strlen(sm->wps->friendly_name);
		if (sm->wps->manufacturer_url)
			extra_len += os_strlen(sm->wps->manufacturer_url);
		if (sm->wps->model_description)
			extra_len += os_strlen(sm->wps->model_description);
		if (sm->wps->model_url)
			extra_len += os_strlen(sm->wps->model_url);
		if (sm->wps->upc)
			extra_len += os_strlen(sm->wps->upc);
	} else if (!os_strcasecmp(filename, UPNP_WPS_SCPD_XML_FILE)) {
		wpa_printf(MSG_DEBUG, "WPS UPnP: HTTP GET for SCPD XML");
		req = GET_SCPD_XML_FILE;
		extra_len = os_strlen(wps_scpd_xml);
	} else {
		/* File not found */
		wpa_printf(MSG_DEBUG, "WPS UPnP: HTTP GET file not found: %s",
			   filename);
		buf = wpabuf_alloc(200);
		if (buf == NULL)
			return;
		wpabuf_put_str(buf,
			       "HTTP/1.1 404 Not Found\r\n"
			       "Connection: close\r\n");

		http_put_date(buf);

		/* terminating empty line */
		wpabuf_put_str(buf, "\r\n");

		goto send_buf;
	}

	buf = wpabuf_alloc(1000 + extra_len);
	if (buf == NULL)
		return;

	wpabuf_put_str(buf,
		       "HTTP/1.1 200 OK\r\n"
		       "Content-Type: text/xml; charset=\"utf-8\"\r\n");
	wpabuf_put_str(buf, "Server: Unspecified, UPnP/1.0, Unspecified\r\n");
	wpabuf_put_str(buf, "Connection: close\r\n");
	wpabuf_put_str(buf, "Content-Length: ");
	/*
	 * We will paste the length in later, leaving some extra whitespace.
	 * HTTP code is supposed to be tolerant of extra whitespace.
	 */
	put_length_here = wpabuf_put(buf, 0);
	wpabuf_put_str(buf, "        \r\n");

	http_put_date(buf);

	/* terminating empty line */
	wpabuf_put_str(buf, "\r\n");

	body_start = wpabuf_put(buf, 0);

	switch (req) {
	case GET_DEVICE_XML_FILE:
		format_wps_device_xml(sm, buf);
		break;
	case GET_SCPD_XML_FILE:
		wpabuf_put_str(buf, wps_scpd_xml);
		break;
	}

	/* Now patch in the content length at the end */
	body_length = (char *) wpabuf_put(buf, 0) - body_start;
	os_snprintf(len_buf, 10, "%d", body_length);
	os_memcpy(put_length_here, len_buf, os_strlen(len_buf));

send_buf:
	send_wpabuf(c->sd, buf);
	wpabuf_free(buf);
}


static struct wpabuf * web_get_item(char *data, const char *name,
				    enum http_reply_code *ret)
{
	char *msg;
	struct wpabuf *buf;
	unsigned char *decoded;
	size_t len;

	if (upnp_get_first_document_item(data, name, &msg)) {
		*ret = UPNP_ARG_VALUE_INVALID;
		return NULL;
	}

	decoded = base64_decode((unsigned char *) msg, os_strlen(msg), &len);
	os_free(msg);
	if (decoded == NULL) {
		*ret = UPNP_OUT_OF_MEMORY;
		return NULL;
	}

	buf = wpabuf_alloc_ext_data(decoded, len);
	if (buf == NULL) {
		os_free(decoded);
		*ret = UPNP_OUT_OF_MEMORY;
		return NULL;
	}
	return buf;
}


static enum http_reply_code
web_process_get_device_info(struct upnp_wps_device_sm *sm,
			    struct wpabuf **reply, const char **replyname)
{
	static const char *name = "NewDeviceInfo";

	wpa_printf(MSG_DEBUG, "WPS UPnP: GetDeviceInfo");
	if (sm->ctx->rx_req_get_device_info == NULL)
		return HTTP_INTERNAL_SERVER_ERROR;
	*reply = sm->ctx->rx_req_get_device_info(sm->priv, &sm->peer);
	if (*reply == NULL) {
		wpa_printf(MSG_INFO, "WPS UPnP: Failed to get DeviceInfo");
		return HTTP_INTERNAL_SERVER_ERROR;
	}
	*replyname = name;
	return HTTP_OK;
}


static enum http_reply_code
web_process_put_message(struct upnp_wps_device_sm *sm, char *data,
			struct wpabuf **reply, const char **replyname)
{
	struct wpabuf *msg;
	static const char *name = "NewOutMessage";
	enum http_reply_code ret;

	/*
	 * PutMessage is used by external UPnP-based Registrar to perform WPS
	 * operation with the access point itself; as compared with
	 * PutWLANResponse which is for proxying.
	 */
	wpa_printf(MSG_DEBUG, "WPS UPnP: PutMessage");
	if (sm->ctx->rx_req_put_message == NULL)
		return HTTP_INTERNAL_SERVER_ERROR;
	msg = web_get_item(data, "NewInMessage", &ret);
	if (msg == NULL)
		return ret;
	*reply = sm->ctx->rx_req_put_message(sm->priv, &sm->peer, msg);
	wpabuf_free(msg);
	if (*reply == NULL)
		return HTTP_INTERNAL_SERVER_ERROR;
	*replyname = name;
	return HTTP_OK;
}


static enum http_reply_code
web_process_get_ap_settings(struct upnp_wps_device_sm *sm, char *data,
			    struct wpabuf **reply, const char **replyname)
{
	struct wpabuf *msg;
	static const char *name = "NewAPSettings";
	enum http_reply_code ret;

	wpa_printf(MSG_DEBUG, "WPS UPnP: GetAPSettings");
	if (sm->ctx->rx_req_get_ap_settings == NULL)
		return HTTP_INTERNAL_SERVER_ERROR;
	msg = web_get_item(data, "NewMessage", &ret);
	if (msg == NULL)
		return ret;
	*reply = sm->ctx->rx_req_get_ap_settings(sm->priv, msg);
	wpabuf_free(msg);
	if (*reply == NULL)
		return HTTP_INTERNAL_SERVER_ERROR;
	*replyname = name;
	return HTTP_OK;
}


static enum http_reply_code
web_process_set_ap_settings(struct upnp_wps_device_sm *sm, char *data,
			    struct wpabuf **reply, const char **replyname)
{
	struct wpabuf *msg;
	enum http_reply_code ret;

	wpa_printf(MSG_DEBUG, "WPS UPnP: SetAPSettings");
	msg = web_get_item(data, "NewAPSettings", &ret);
	if (msg == NULL)
		return ret;
	if (!sm->ctx->rx_req_set_ap_settings ||
	    sm->ctx->rx_req_set_ap_settings(sm->priv, msg)) {
		wpabuf_free(msg);
		return HTTP_INTERNAL_SERVER_ERROR;
	}
	wpabuf_free(msg);
	*replyname = NULL;
	*reply = NULL;
	return HTTP_OK;
}


static enum http_reply_code
web_process_del_ap_settings(struct upnp_wps_device_sm *sm, char *data,
			    struct wpabuf **reply, const char **replyname)
{
	struct wpabuf *msg;
	enum http_reply_code ret;

	wpa_printf(MSG_DEBUG, "WPS UPnP: DelAPSettings");
	msg = web_get_item(data, "NewAPSettings", &ret);
	if (msg == NULL)
		return ret;
	if (!sm->ctx->rx_req_del_ap_settings ||
	    sm->ctx->rx_req_del_ap_settings(sm->priv, msg)) {
		wpabuf_free(msg);
		return HTTP_INTERNAL_SERVER_ERROR;
	}
	wpabuf_free(msg);
	*replyname = NULL;
	*reply = NULL;
	return HTTP_OK;
}


static enum http_reply_code
web_process_get_sta_settings(struct upnp_wps_device_sm *sm, char *data,
			     struct wpabuf **reply, const char **replyname)
{
	struct wpabuf *msg;
	static const char *name = "NewSTASettings";
	enum http_reply_code ret;

	wpa_printf(MSG_DEBUG, "WPS UPnP: GetSTASettings");
	if (sm->ctx->rx_req_get_sta_settings == NULL)
		return HTTP_INTERNAL_SERVER_ERROR;
	msg = web_get_item(data, "NewMessage", &ret);
	if (msg == NULL)
		return ret;
	*reply = sm->ctx->rx_req_get_sta_settings(sm->priv, msg);
	wpabuf_free(msg);
	if (*reply == NULL)
		return HTTP_INTERNAL_SERVER_ERROR;
	*replyname = name;
	return HTTP_OK;
}


static enum http_reply_code
web_process_set_sta_settings(struct upnp_wps_device_sm *sm, char *data,
			     struct wpabuf **reply, const char **replyname)
{
	struct wpabuf *msg;
	enum http_reply_code ret;

	wpa_printf(MSG_DEBUG, "WPS UPnP: SetSTASettings");
	msg = web_get_item(data, "NewSTASettings", &ret);
	if (msg == NULL)
		return ret;
	if (!sm->ctx->rx_req_set_sta_settings ||
	    sm->ctx->rx_req_set_sta_settings(sm->priv, msg)) {
		wpabuf_free(msg);
		return HTTP_INTERNAL_SERVER_ERROR;
	}
	wpabuf_free(msg);
	*replyname = NULL;
	*reply = NULL;
	return HTTP_OK;
}


static enum http_reply_code
web_process_del_sta_settings(struct upnp_wps_device_sm *sm, char *data,
			     struct wpabuf **reply, const char **replyname)
{
	struct wpabuf *msg;
	enum http_reply_code ret;

	wpa_printf(MSG_DEBUG, "WPS UPnP: DelSTASettings");
	msg = web_get_item(data, "NewSTASettings", &ret);
	if (msg == NULL)
		return ret;
	if (!sm->ctx->rx_req_del_sta_settings ||
	    sm->ctx->rx_req_del_sta_settings(sm->priv, msg)) {
		wpabuf_free(msg);
		return HTTP_INTERNAL_SERVER_ERROR;
	}
	wpabuf_free(msg);
	*replyname = NULL;
	*reply = NULL;
	return HTTP_OK;
}


static enum http_reply_code
web_process_put_wlan_response(struct upnp_wps_device_sm *sm, char *data,
			      struct wpabuf **reply, const char **replyname)
{
	struct wpabuf *msg;
	enum http_reply_code ret;
	u8 macaddr[ETH_ALEN];
	int ev_type;
	int type;
	char *val;

	/*
	 * External UPnP-based Registrar is passing us a message to be proxied
	 * over to a Wi-Fi -based client of ours.
	 */

	wpa_printf(MSG_DEBUG, "WPS UPnP: PutWLANResponse");
	msg = web_get_item(data, "NewMessage", &ret);
	if (msg == NULL)
		return ret;
	if (upnp_get_first_document_item(data, "NewWLANEventType", &val)) {
		wpabuf_free(msg);
		return UPNP_ARG_VALUE_INVALID;
	}
	ev_type = atol(val);
	os_free(val);
	val = NULL;
	if (upnp_get_first_document_item(data, "NewWLANEventMAC", &val) ||
	    hwaddr_aton(val, macaddr)) {
		wpabuf_free(msg);
		os_free(val);
		return UPNP_ARG_VALUE_INVALID;
	}
	os_free(val);
	if (ev_type == UPNP_WPS_WLANEVENT_TYPE_EAP) {
		struct wps_parse_attr attr;
		if (wps_parse_msg(msg, &attr) < 0 ||
		    attr.msg_type == NULL)
			type = -1;
		else
			type = *attr.msg_type;
		wpa_printf(MSG_DEBUG, "WPS UPnP: Message Type %d", type);
	} else
		type = -1;
	if (!sm->ctx->rx_req_put_wlan_response ||
	    sm->ctx->rx_req_put_wlan_response(sm->priv, ev_type, macaddr, msg,
					      type)) {
		wpa_printf(MSG_INFO, "WPS UPnP: Fail: sm->ctx->"
			   "rx_req_put_wlan_response");
		wpabuf_free(msg);
		return HTTP_INTERNAL_SERVER_ERROR;
	}
	wpabuf_free(msg);
	*replyname = NULL;
	*reply = NULL;
	return HTTP_OK;
}


static enum http_reply_code
web_process_set_selected_registrar(struct upnp_wps_device_sm *sm, char *data,
				   struct wpabuf **reply,
				   const char **replyname)
{
	struct wpabuf *msg;
	enum http_reply_code ret;

	wpa_printf(MSG_DEBUG, "WPS UPnP: SetSelectedRegistrar");
	msg = web_get_item(data, "NewMessage", &ret);
	if (msg == NULL)
		return ret;
	if (!sm->ctx->rx_req_set_selected_registrar ||
	    sm->ctx->rx_req_set_selected_registrar(sm->priv, msg)) {
		wpabuf_free(msg);
		return HTTP_INTERNAL_SERVER_ERROR;
	}
	wpabuf_free(msg);
	*replyname = NULL;
	*reply = NULL;
	return HTTP_OK;
}


static enum http_reply_code
web_process_reboot_ap(struct upnp_wps_device_sm *sm, char *data,
		      struct wpabuf **reply, const char **replyname)
{
	struct wpabuf *msg;
	enum http_reply_code ret;

	wpa_printf(MSG_DEBUG, "WPS UPnP: RebootAP");
	msg = web_get_item(data, "NewAPSettings", &ret);
	if (msg == NULL)
		return ret;
	if (!sm->ctx->rx_req_reboot_ap ||
	    sm->ctx->rx_req_reboot_ap(sm->priv, msg)) {
		wpabuf_free(msg);
		return HTTP_INTERNAL_SERVER_ERROR;
	}
	wpabuf_free(msg);
	*replyname = NULL;
	*reply = NULL;
	return HTTP_OK;
}


static enum http_reply_code
web_process_reset_ap(struct upnp_wps_device_sm *sm, char *data,
		     struct wpabuf **reply, const char **replyname)
{
	struct wpabuf *msg;
	enum http_reply_code ret;

	wpa_printf(MSG_DEBUG, "WPS UPnP: ResetAP");
	msg = web_get_item(data, "NewMessage", &ret);
	if (msg == NULL)
		return ret;
	if (!sm->ctx->rx_req_reset_ap ||
	    sm->ctx->rx_req_reset_ap(sm->priv, msg)) {
		wpabuf_free(msg);
		return HTTP_INTERNAL_SERVER_ERROR;
	}
	wpabuf_free(msg);
	*replyname = NULL;
	*reply = NULL;
	return HTTP_OK;
}


static enum http_reply_code
web_process_reboot_sta(struct upnp_wps_device_sm *sm, char *data,
		       struct wpabuf **reply, const char **replyname)
{
	struct wpabuf *msg;
	enum http_reply_code ret;

	wpa_printf(MSG_DEBUG, "WPS UPnP: RebootSTA");
	msg = web_get_item(data, "NewSTASettings", &ret);
	if (msg == NULL)
		return ret;
	if (!sm->ctx->rx_req_reboot_sta ||
	    sm->ctx->rx_req_reboot_sta(sm->priv, msg)) {
		wpabuf_free(msg);
		return HTTP_INTERNAL_SERVER_ERROR;
	}
	wpabuf_free(msg);
	*replyname = NULL;
	*reply = NULL;
	return HTTP_OK;
}


static enum http_reply_code
web_process_reset_sta(struct upnp_wps_device_sm *sm, char *data,
		      struct wpabuf **reply, const char **replyname)
{
	struct wpabuf *msg;
	enum http_reply_code ret;

	wpa_printf(MSG_DEBUG, "WPS UPnP: ResetSTA");
	msg = web_get_item(data, "NewMessage", &ret);
	if (msg == NULL)
		return ret;
	if (!sm->ctx->rx_req_reset_sta ||
	    sm->ctx->rx_req_reset_sta(sm->priv, msg)) {
		wpabuf_free(msg);
		return HTTP_INTERNAL_SERVER_ERROR;
	}
	wpabuf_free(msg);
	*replyname = NULL;
	*reply = NULL;
	return HTTP_OK;
}


static const char *soap_prefix =
	"<?xml version=\"1.0\"?>\n"
	"<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
	"s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
	"<s:Body>\n";
static const char *soap_postfix =
	"</s:Body>\n</s:Envelope>\n";

static const char *soap_error_prefix =
	"<s:Fault>\n"
	"<faultcode>s:Client</faultcode>\n"
	"<faultstring>UPnPError</faultstring>\n"
	"<detail>\n"
	"<UPnPError xmlns=\"urn:schemas-upnp-org:control-1-0\">\n";
static const char *soap_error_postfix =
	"<errorDescription>Error</errorDescription>\n"
	"</UPnPError>\n"
	"</detail>\n"
	"</s:Fault>\n";

static void web_connection_send_reply(struct web_connection *c,
				      enum http_reply_code ret,
				      const char *action, int action_len,
				      const struct wpabuf *reply,
				      const char *replyname)
{
	struct wpabuf *buf;
	char *replydata;
	char *put_length_here = NULL;
	char *body_start = NULL;

	if (reply) {
		size_t len;
		replydata = (char *) base64_encode(wpabuf_head(reply),
						   wpabuf_len(reply), &len);
	} else
		replydata = NULL;

	/* Parameters of the response:
	 *      action(action_len) -- action we are responding to
	 *      replyname -- a name we need for the reply
	 *      replydata -- NULL or null-terminated string
	 */
	buf = wpabuf_alloc(1000 + (replydata ? os_strlen(replydata) : 0U) +
			   (action_len > 0 ? action_len * 2 : 0));
	if (buf == NULL) {
		wpa_printf(MSG_INFO, "WPS UPnP: Cannot allocate reply to "
			   "POST");
		wpabuf_free(buf);
		os_free(replydata);
		return;
	}

	/*
	 * Assuming we will be successful, put in the output header first.
	 * Note: we do not keep connections alive (and httpread does
	 * not support it)... therefore we must have Connection: close.
	 */
	if (ret == HTTP_OK) {
		wpabuf_put_str(buf,
			       "HTTP/1.1 200 OK\r\n"
			       "Content-Type: text/xml; "
			       "charset=\"utf-8\"\r\n");
	} else {
		wpabuf_printf(buf, "HTTP/1.1 %d Error\r\n", ret);
	}
	wpabuf_put_str(buf, http_connection_close);

	wpabuf_put_str(buf, "Content-Length: ");
	/*
	 * We will paste the length in later, leaving some extra whitespace.
	 * HTTP code is supposed to be tolerant of extra whitespace.
	 */
	put_length_here = wpabuf_put(buf, 0);
	wpabuf_put_str(buf, "        \r\n");

	http_put_date(buf);

	/* terminating empty line */
	wpabuf_put_str(buf, "\r\n");

	body_start = wpabuf_put(buf, 0);

	if (ret == HTTP_OK) {
		wpabuf_put_str(buf, soap_prefix);
		wpabuf_put_str(buf, "<u:");
		wpabuf_put_data(buf, action, action_len);
		wpabuf_put_str(buf, "Response xmlns:u=\"");
		wpabuf_put_str(buf, urn_wfawlanconfig);
		wpabuf_put_str(buf, "\">\n");
		if (replydata && replyname) {
			/* TODO: might possibly need to escape part of reply
			 * data? ...
			 * probably not, unlikely to have ampersand(&) or left
			 * angle bracket (<) in it...
			 */
			wpabuf_printf(buf, "<%s>", replyname);
			wpabuf_put_str(buf, replydata);
			wpabuf_printf(buf, "</%s>\n", replyname);
		}
		wpabuf_put_str(buf, "</u:");
		wpabuf_put_data(buf, action, action_len);
		wpabuf_put_str(buf, "Response>\n");
		wpabuf_put_str(buf, soap_postfix);
	} else {
		/* Error case */
		wpabuf_put_str(buf, soap_prefix);
		wpabuf_put_str(buf, soap_error_prefix);
		wpabuf_printf(buf, "<errorCode>%d</errorCode>\n", ret);
		wpabuf_put_str(buf, soap_error_postfix);
		wpabuf_put_str(buf, soap_postfix);
	}
	os_free(replydata);

	/* Now patch in the content length at the end */
	if (body_start && put_length_here) {
		int body_length = (char *) wpabuf_put(buf, 0) - body_start;
		char len_buf[10];
		os_snprintf(len_buf, sizeof(len_buf), "%d", body_length);
		os_memcpy(put_length_here, len_buf, os_strlen(len_buf));
	}

	send_wpabuf(c->sd, buf);
	wpabuf_free(buf);
}


static const char * web_get_action(struct web_connection *c,
				   const char *filename, size_t *action_len)
{
	const char *match;
	int match_len;
	char *b;
	char *action;

	*action_len = 0;
	if (os_strcasecmp(filename, UPNP_WPS_DEVICE_CONTROL_FILE)) {
		wpa_printf(MSG_INFO, "WPS UPnP: Invalid POST filename %s",
			   filename);
		return NULL;
	}
	/* The SOAPAction line of the header tells us what we want to do */
	b = httpread_hdr_line_get(c->hread, "SOAPAction:");
	if (b == NULL)
		return NULL;
	if (*b == '"')
		b++;
	else
		return NULL;
	match = urn_wfawlanconfig;
	match_len = os_strlen(urn_wfawlanconfig) - 1;
	if (os_strncasecmp(b, match, match_len))
		return NULL;
	b += match_len;
	/* skip over version */
	while (isgraph(*b) && *b != '#')
		b++;
	if (*b != '#')
		return NULL;
	b++;
	/* Following the sharp(#) should be the action and a double quote */
	action = b;
	while (isgraph(*b) && *b != '"')
		b++;
	if (*b != '"')
		return NULL;
	*action_len = b - action;
	return action;
}


/* Given that we have received a header w/ POST, act upon it
 *
 * Format of POST (case-insensitive):
 *
 * First line must be:
 *      POST /<file> HTTP/1.1
 * Since we don't do anything fancy we just ignore other lines.
 *
 * Our response (if no error) which includes only required lines is:
 * HTTP/1.1 200 OK
 * Connection: close
 * Content-Type: text/xml
 * Date: <rfc1123-date>
 *
 * Header lines must end with \r\n
 * Per RFC 2616, content-length: is not required but connection:close
 * would appear to be required (given that we will be closing it!).
 */
static void web_connection_parse_post(struct web_connection *c,
				      const char *filename)
{
	enum http_reply_code ret;
	struct upnp_wps_device_sm *sm = c->sm;
	char *data = httpread_data_get(c->hread); /* body of http msg */
	const char *action;
	size_t action_len;
	const char *replyname = NULL; /* argument name for the reply */
	struct wpabuf *reply = NULL; /* data for the reply */

	ret = UPNP_INVALID_ACTION;
	action = web_get_action(c, filename, &action_len);
	if (action == NULL)
		goto bad;

	/*
	 * There are quite a few possible actions. Although we appear to
	 * support them all here, not all of them are necessarily supported by
	 * callbacks at higher levels.
	 */
	if (!os_strncasecmp("GetDeviceInfo", action, action_len))
		ret = web_process_get_device_info(sm, &reply, &replyname);
	else if (!os_strncasecmp("PutMessage", action, action_len))
		ret = web_process_put_message(sm, data, &reply, &replyname);
	else if (!os_strncasecmp("GetAPSettings", action, action_len))
		ret = web_process_get_ap_settings(sm, data, &reply,
						  &replyname);
	else if (!os_strncasecmp("SetAPSettings", action, action_len))
		ret = web_process_set_ap_settings(sm, data, &reply,
						  &replyname);
	else if (!os_strncasecmp("DelAPSettings", action, action_len))
		ret = web_process_del_ap_settings(sm, data, &reply,
						  &replyname);
	else if (!os_strncasecmp("GetSTASettings", action, action_len))
		ret = web_process_get_sta_settings(sm, data, &reply,
						   &replyname);
	else if (!os_strncasecmp("SetSTASettings", action, action_len))
		ret = web_process_set_sta_settings(sm, data, &reply,
						   &replyname);
	else if (!os_strncasecmp("DelSTASettings", action, action_len))
		ret = web_process_del_sta_settings(sm, data, &reply,
						  &replyname);
	else if (!os_strncasecmp("PutWLANResponse", action, action_len))
		ret = web_process_put_wlan_response(sm, data, &reply,
						    &replyname);
	else if (!os_strncasecmp("SetSelectedRegistrar", action, action_len))
		ret = web_process_set_selected_registrar(sm, data, &reply,
							 &replyname);
	else if (!os_strncasecmp("RebootAP", action, action_len))
		ret = web_process_reboot_ap(sm, data, &reply, &replyname);
	else if (!os_strncasecmp("ResetAP", action, action_len))
		ret = web_process_reset_ap(sm, data, &reply, &replyname);
	else if (!os_strncasecmp("RebootSTA", action, action_len))
		ret = web_process_reboot_sta(sm, data, &reply, &replyname);
	else if (!os_strncasecmp("ResetSTA", action, action_len))
		ret = web_process_reset_sta(sm, data, &reply, &replyname);
	else
		wpa_printf(MSG_INFO, "WPS UPnP: Unknown POST type");

bad:
	if (ret != HTTP_OK)
		wpa_printf(MSG_INFO, "WPS UPnP: POST failure ret=%d", ret);
	web_connection_send_reply(c, ret, action, action_len, reply,
				  replyname);
	wpabuf_free(reply);
}


/* Given that we have received a header w/ SUBSCRIBE, act upon it
 *
 * Format of SUBSCRIBE (case-insensitive):
 *
 * First line must be:
 *      SUBSCRIBE /wps_event HTTP/1.1
 *
 * Our response (if no error) which includes only required lines is:
 * HTTP/1.1 200 OK
 * Server: xx, UPnP/1.0, xx
 * SID: uuid:xxxxxxxxx
 * Timeout: Second-<n>
 * Content-Length: 0
 * Date: xxxx
 *
 * Header lines must end with \r\n
 * Per RFC 2616, content-length: is not required but connection:close
 * would appear to be required (given that we will be closing it!).
 */
static void web_connection_parse_subscribe(struct web_connection *c,
					   const char *filename)
{
	struct upnp_wps_device_sm *sm = c->sm;
	struct wpabuf *buf;
	char *b;
	char *hdr = httpread_hdr_get(c->hread);
	char *h;
	char *match;
	int match_len;
	char *end;
	int len;
	int got_nt = 0;
	u8 uuid[UUID_LEN];
	int got_uuid = 0;
	char *callback_urls = NULL;
	struct subscription *s = NULL;
	enum http_reply_code ret = HTTP_INTERNAL_SERVER_ERROR;

	buf = wpabuf_alloc(1000);
	if (buf == NULL)
		return;

	/* Parse/validate headers */
	h = hdr;
	/* First line: SUBSCRIBE /wps_event HTTP/1.1
	 * has already been parsed.
	 */
	if (os_strcasecmp(filename, UPNP_WPS_DEVICE_EVENT_FILE) != 0) {
		ret = HTTP_PRECONDITION_FAILED;
		goto error;
	}
	wpa_printf(MSG_DEBUG, "WPS UPnP: HTTP SUBSCRIBE for event");
	end = os_strchr(h, '\n');

	for (; end != NULL; h = end + 1) {
		/* Option line by option line */
		h = end + 1;
		end = os_strchr(h, '\n');
		if (end == NULL)
			break; /* no unterminated lines allowed */

		/* NT assures that it is our type of subscription;
		 * not used for a renewl.
		 **/
		match = "NT:";
		match_len = os_strlen(match);
		if (os_strncasecmp(h, match, match_len) == 0) {
			h += match_len;
			while (*h == ' ' || *h == '\t')
				h++;
			match = "upnp:event";
			match_len = os_strlen(match);
			if (os_strncasecmp(h, match, match_len) != 0) {
				ret = HTTP_BAD_REQUEST;
				goto error;
			}
			got_nt = 1;
			continue;
		}
		/* HOST should refer to us */
#if 0
		match = "HOST:";
		match_len = os_strlen(match);
		if (os_strncasecmp(h, match, match_len) == 0) {
			h += match_len;
			while (*h == ' ' || *h == '\t')
				h++;
			.....
		}
#endif
		/* CALLBACK gives one or more URLs for NOTIFYs
		 * to be sent as a result of the subscription.
		 * Each URL is enclosed in angle brackets.
		 */
		match = "CALLBACK:";
		match_len = os_strlen(match);
		if (os_strncasecmp(h, match, match_len) == 0) {
			h += match_len;
			while (*h == ' ' || *h == '\t')
				h++;
			len = end - h;
			os_free(callback_urls);
			callback_urls = os_malloc(len + 1);
			if (callback_urls == NULL) {
				ret = HTTP_INTERNAL_SERVER_ERROR;
				goto error;
			}
			os_memcpy(callback_urls, h, len);
			callback_urls[len] = 0;
			continue;
		}
		/* SID is only for renewal */
		match = "SID:";
		match_len = os_strlen(match);
		if (os_strncasecmp(h, match, match_len) == 0) {
			h += match_len;
			while (*h == ' ' || *h == '\t')
				h++;
			match = "uuid:";
			match_len = os_strlen(match);
			if (os_strncasecmp(h, match, match_len) != 0) {
				ret = HTTP_BAD_REQUEST;
				goto error;
			}
			h += match_len;
			while (*h == ' ' || *h == '\t')
				h++;
			if (uuid_str2bin(h, uuid)) {
				ret = HTTP_BAD_REQUEST;
				goto error;
			}
			got_uuid = 1;
			continue;
		}
		/* TIMEOUT is requested timeout, but apparently we can
		 * just ignore this.
		 */
	}

	if (got_uuid) {
		/* renewal */
		if (callback_urls) {
			ret = HTTP_BAD_REQUEST;
			goto error;
		}
		s = subscription_renew(sm, uuid);
		if (s == NULL) {
			ret = HTTP_PRECONDITION_FAILED;
			goto error;
		}
	} else if (callback_urls) {
		if (!got_nt) {
			ret = HTTP_PRECONDITION_FAILED;
			goto error;
		}
		s = subscription_start(sm, callback_urls);
		if (s == NULL) {
			ret = HTTP_INTERNAL_SERVER_ERROR;
			goto error;
		}
		callback_urls = NULL;   /* is now owned by subscription */
	} else {
		ret = HTTP_PRECONDITION_FAILED;
		goto error;
	}

	/* success */
	http_put_reply_code(buf, HTTP_OK);
	wpabuf_put_str(buf, http_server_hdr);
	wpabuf_put_str(buf, http_connection_close);
	wpabuf_put_str(buf, "Content-Length: 0\r\n");
	wpabuf_put_str(buf, "SID: uuid:");
	/* subscription id */
	b = wpabuf_put(buf, 0);
	uuid_bin2str(s->uuid, b, 80);
	wpabuf_put(buf, os_strlen(b));
	wpabuf_put_str(buf, "\r\n");
	wpabuf_printf(buf, "Timeout: Second-%d\r\n", UPNP_SUBSCRIBE_SEC);
	http_put_date(buf);
	/* And empty line to terminate header: */
	wpabuf_put_str(buf, "\r\n");

	send_wpabuf(c->sd, buf);
	wpabuf_free(buf);
	os_free(callback_urls);
	return;

error:
	/* Per UPnP spec:
	* Errors
	* Incompatible headers
	*   400 Bad Request. If SID header and one of NT or CALLBACK headers
	*     are present, the publisher must respond with HTTP error
	*     400 Bad Request.
	* Missing or invalid CALLBACK
	*   412 Precondition Failed. If CALLBACK header is missing or does not
	*     contain a valid HTTP URL, the publisher must respond with HTTP
	*     error 412 Precondition Failed.
	* Invalid NT
	*   412 Precondition Failed. If NT header does not equal upnp:event,
	*     the publisher must respond with HTTP error 412 Precondition
	*     Failed.
	* [For resubscription, use 412 if unknown uuid].
	* Unable to accept subscription
	*   5xx. If a publisher is not able to accept a subscription (such as
	*     due to insufficient resources), it must respond with a
	*     HTTP 500-series error code.
	*   599 Too many subscriptions (not a standard HTTP error)
	*/
	http_put_empty(buf, ret);
	send_wpabuf(c->sd, buf);
	wpabuf_free(buf);
}


/* Given that we have received a header w/ UNSUBSCRIBE, act upon it
 *
 * Format of UNSUBSCRIBE (case-insensitive):
 *
 * First line must be:
 *      UNSUBSCRIBE /wps_event HTTP/1.1
 *
 * Our response (if no error) which includes only required lines is:
 * HTTP/1.1 200 OK
 * Content-Length: 0
 *
 * Header lines must end with \r\n
 * Per RFC 2616, content-length: is not required but connection:close
 * would appear to be required (given that we will be closing it!).
 */
static void web_connection_parse_unsubscribe(struct web_connection *c,
					     const char *filename)
{
	struct upnp_wps_device_sm *sm = c->sm;
	struct wpabuf *buf;
	char *hdr = httpread_hdr_get(c->hread);
	char *h;
	char *match;
	int match_len;
	char *end;
	u8 uuid[UUID_LEN];
	int got_uuid = 0;
	struct subscription *s = NULL;
	enum http_reply_code ret = HTTP_INTERNAL_SERVER_ERROR;

	/* Parse/validate headers */
	h = hdr;
	/* First line: UNSUBSCRIBE /wps_event HTTP/1.1
	 * has already been parsed.
	 */
	if (os_strcasecmp(filename, UPNP_WPS_DEVICE_EVENT_FILE) != 0) {
		ret = HTTP_PRECONDITION_FAILED;
		goto send_msg;
	}
	wpa_printf(MSG_DEBUG, "WPS UPnP: HTTP UNSUBSCRIBE for event");
	end = os_strchr(h, '\n');

	for (; end != NULL; h = end + 1) {
		/* Option line by option line */
		h = end + 1;
		end = os_strchr(h, '\n');
		if (end == NULL)
			break; /* no unterminated lines allowed */

		/* HOST should refer to us */
#if 0
		match = "HOST:";
		match_len = os_strlen(match);
		if (os_strncasecmp(h, match, match_len) == 0) {
			h += match_len;
			while (*h == ' ' || *h == '\t')
				h++;
			.....
		}
#endif
		/* SID is only for renewal */
		match = "SID:";
		match_len = os_strlen(match);
		if (os_strncasecmp(h, match, match_len) == 0) {
			h += match_len;
			while (*h == ' ' || *h == '\t')
				h++;
			match = "uuid:";
			match_len = os_strlen(match);
			if (os_strncasecmp(h, match, match_len) != 0) {
				ret = HTTP_BAD_REQUEST;
				goto send_msg;
			}
			h += match_len;
			while (*h == ' ' || *h == '\t')
				h++;
			if (uuid_str2bin(h, uuid)) {
				ret = HTTP_BAD_REQUEST;
				goto send_msg;
			}
			got_uuid = 1;
			continue;
		}
	}

	if (got_uuid) {
		s = subscription_find(sm, uuid);
		if (s) {
			wpa_printf(MSG_DEBUG, "WPS UPnP: Unsubscribing %p %s",
				   s,
				   (s && s->addr_list &&
				    s->addr_list->domain_and_port) ?
				   s->addr_list->domain_and_port : "-null-");
			subscription_unlink(s);
			subscription_destroy(s);
		}
	} else {
		wpa_printf(MSG_INFO, "WPS UPnP: Unsubscribe fails (not "
			   "found)");
		ret = HTTP_PRECONDITION_FAILED;
		goto send_msg;
	}

	ret = HTTP_OK;

send_msg:
	buf = wpabuf_alloc(200);
	if (buf == NULL)
		return;
	http_put_empty(buf, ret);
	send_wpabuf(c->sd, buf);
	wpabuf_free(buf);
}


/* Send error in response to unknown requests */
static void web_connection_unimplemented(struct web_connection *c)
{
	struct wpabuf *buf;
	buf = wpabuf_alloc(200);
	if (buf == NULL)
		return;
	http_put_empty(buf, HTTP_UNIMPLEMENTED);
	send_wpabuf(c->sd, buf);
	wpabuf_free(buf);
}



/* Called when we have gotten an apparently valid http request.
 */
static void web_connection_check_data(struct web_connection *c)
{
	struct httpread *hread = c->hread;
	enum httpread_hdr_type htype = httpread_hdr_type_get(hread);
	/* char *data = httpread_data_get(hread); */
	char *filename = httpread_uri_get(hread);

	c->done = 1;
	if (!filename) {
		wpa_printf(MSG_INFO, "WPS UPnP: Could not get HTTP URI");
		return;
	}
	/* Trim leading slashes from filename */
	while (*filename == '/')
		filename++;

	wpa_printf(MSG_DEBUG, "WPS UPnP: Got HTTP request type %d from %s:%d",
		   htype, inet_ntoa(c->cli_addr.sin_addr),
		   htons(c->cli_addr.sin_port));

	switch (htype) {
	case HTTPREAD_HDR_TYPE_GET:
		web_connection_parse_get(c, filename);
		break;
	case HTTPREAD_HDR_TYPE_POST:
		web_connection_parse_post(c, filename);
		break;
	case HTTPREAD_HDR_TYPE_SUBSCRIBE:
		web_connection_parse_subscribe(c, filename);
		break;
	case HTTPREAD_HDR_TYPE_UNSUBSCRIBE:
		web_connection_parse_unsubscribe(c, filename);
		break;
		/* We are not required to support M-POST; just plain
		 * POST is supposed to work, so we only support that.
		 * If for some reason we need to support M-POST, it is
		 * mostly the same as POST, with small differences.
		 */
	default:
		/* Send 501 for anything else */
		web_connection_unimplemented(c);
		break;
	}
}



/* called back when we have gotten request */
static void web_connection_got_file_handler(struct httpread *handle,
					    void *cookie,
					    enum httpread_event en)
{
	struct web_connection *c = cookie;

	if (en == HTTPREAD_EVENT_FILE_READY)
		web_connection_check_data(c);
	web_connection_stop(c);
}


/* web_connection_start - Start web connection
 * @sm: WPS UPnP state machine from upnp_wps_device_init()
 * @sd: Socket descriptor
 * @addr: Client address
 *
 * The socket descriptor sd is handed over for ownership by the WPS UPnP
 * state machine.
 */
static void web_connection_start(struct upnp_wps_device_sm *sm,
				 int sd, struct sockaddr_in *addr)
{
	struct web_connection *c = NULL;

	/* if too many connections, bail */
	if (sm->n_web_connections >= MAX_WEB_CONNECTIONS) {
		close(sd);
		return;
	}

	c = os_zalloc(sizeof(*c));
	if (c == NULL)
		return;
	os_memcpy(&c->cli_addr, addr, sizeof(c->cli_addr));
	c->sm = sm;
	c->sd = sd;
#if 0
	/*
	 * Setting non-blocking should not be necessary for read, and can mess
	 * up sending where blocking might be better.
	 */
	if (fcntl(sd, F_SETFL, O_NONBLOCK) != 0)
		break;
#endif
	c->hread = httpread_create(c->sd, web_connection_got_file_handler,
				   c /* cookie */,
				   WEB_CONNECTION_MAX_READ,
				   WEB_CONNECTION_TIMEOUT_SEC);
	if (c->hread == NULL)
		goto fail;
	if (sm->web_connections) {
		c->next = sm->web_connections;
		c->prev = c->next->prev;
		c->prev->next = c;
		c->next->prev = c;
	} else {
		sm->web_connections = c->next = c->prev = c;
	}
	sm->n_web_connections++;
	return;

fail:
	if (c)
		web_connection_stop(c);
}


/*
 * Listening for web connections
 * We have a single TCP listening port, and hand off connections as we get
 * them.
 */

void web_listener_stop(struct upnp_wps_device_sm *sm)
{
	if (sm->web_sd_registered) {
		sm->web_sd_registered = 0;
		eloop_unregister_sock(sm->web_sd, EVENT_TYPE_READ);
	}
	if (sm->web_sd >= 0)
		close(sm->web_sd);
	sm->web_sd = -1;
}


static void web_listener_handler(int sd, void *eloop_ctx, void *sock_ctx)
{
	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(addr);
	struct upnp_wps_device_sm *sm = sock_ctx;
	int new_sd;

	/* Create state for new connection */
	/* Remember so we can cancel if need be */
	new_sd = accept(sm->web_sd, (struct sockaddr *) &addr, &addr_len);
	if (new_sd < 0) {
		wpa_printf(MSG_ERROR, "WPS UPnP: web listener accept "
			   "errno=%d (%s) web_sd=%d",
			   errno, strerror(errno), sm->web_sd);
		return;
	}
	web_connection_start(sm, new_sd, &addr);
}


int web_listener_start(struct upnp_wps_device_sm *sm)
{
	struct sockaddr_in addr;
	int port;

	sm->web_sd = socket(AF_INET, SOCK_STREAM, 0);
	if (sm->web_sd < 0)
		goto fail;
	if (fcntl(sm->web_sd, F_SETFL, O_NONBLOCK) != 0)
		goto fail;
	port = 49152;  /* first non-reserved port */
	for (;;) {
		os_memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = sm->ip_addr;
		addr.sin_port = htons(port);
		if (bind(sm->web_sd, (struct sockaddr *) &addr,
			 sizeof(addr)) == 0)
			break;
		if (errno == EADDRINUSE) {
			/* search for unused port */
			if (++port == 65535)
				goto fail;
			continue;
		}
		goto fail;
	}
	if (listen(sm->web_sd, 10 /* max backlog */) != 0)
		goto fail;
	if (fcntl(sm->web_sd, F_SETFL, O_NONBLOCK) != 0)
		goto fail;
	if (eloop_register_sock(sm->web_sd, EVENT_TYPE_READ,
				web_listener_handler, NULL, sm))
		goto fail;
	sm->web_sd_registered = 1;
	sm->web_port = port;

	return 0;

fail:
	/* Error */
	web_listener_stop(sm);
	return -1;
}

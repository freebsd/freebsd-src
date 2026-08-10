/*
 * XML wrapper for libxml2
 * Copyright (c) 2012-2013, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#define LIBXML_VALID_ENABLED
#include <libxml/tree.h>
#include <libxml/xmlschemastypes.h>

#include "common.h"
#include "base64.h"
#include "xml-utils.h"


struct xml_node_ctx {
	void *ctx;
};


void xml_node_free(struct xml_node_ctx *ctx, xml_node_t *node)
{
	xmlFreeNode((xmlNodePtr) node);
}


xml_node_t * xml_node_from_buf(struct xml_node_ctx *ctx, const char *buf)
{
	xmlDocPtr doc;
	xmlNodePtr node;

	doc = xmlParseMemory(buf, strlen(buf));
	if (doc == NULL)
		return NULL;
	node = xmlDocGetRootElement(doc);
	node = xmlCopyNode(node, 1);
	xmlFreeDoc(doc);

	return (xml_node_t *) node;
}


const char * xml_node_get_localname(struct xml_node_ctx *ctx,
				    xml_node_t *node)
{
	return (const char *) ((xmlNodePtr) node)->name;
}


char * xml_node_to_str(struct xml_node_ctx *ctx, xml_node_t *node)
{
	xmlChar *buf;
	int bufsiz;
	char *ret, *pos;
	xmlNodePtr n = (xmlNodePtr) node;
	xmlDocPtr doc;

	doc = xmlNewDoc((xmlChar *) "1.0");
	n = xmlDocCopyNode(n, doc, 1);
	xmlDocSetRootElement(doc, n);
	xmlDocDumpFormatMemory(doc, &buf, &bufsiz, 0);
	xmlFreeDoc(doc);
	if (!buf)
		return NULL;
	pos = (char *) buf;
	if (strncmp(pos, "<?xml", 5) == 0) {
		pos = strchr(pos, '>');
		if (pos)
			pos++;
		while (pos && (*pos == '\r' || *pos == '\n'))
			pos++;
	}
	if (pos)
		ret = os_strdup(pos);
	else
		ret = NULL;
	xmlFree(buf);

	if (ret) {
		pos = ret;
		if (pos[0]) {
			while (pos[1])
				pos++;
		}
		while (pos >= ret && *pos == '\n')
			*pos-- = '\0';
	}

	return ret;
}


xml_node_t * xml_node_create_root(struct xml_node_ctx *ctx, const char *ns_uri,
				  const char *ns_prefix,
				  xml_namespace_t **ret_ns, const char *name)
{
	xmlNodePtr node;
	xmlNsPtr ns = NULL;

	node = xmlNewNode(NULL, (const xmlChar *) name);
	if (node == NULL)
		return NULL;
	if (ns_uri) {
		ns = xmlNewNs(node, (const xmlChar *) ns_uri,
			      (const xmlChar *) ns_prefix);
		xmlSetNs(node, ns);
	}

	if (ret_ns)
		*ret_ns = (xml_namespace_t *) ns;

	return (xml_node_t *) node;
}


xml_node_t * xml_node_create(struct xml_node_ctx *ctx, xml_node_t *parent,
			     xml_namespace_t *ns, const char *name)
{
	xmlNodePtr node;
	node = xmlNewChild((xmlNodePtr) parent, (xmlNsPtr) ns,
			   (const xmlChar *) name, NULL);
	return (xml_node_t *) node;
}


xml_node_t * xml_node_create_text(struct xml_node_ctx *ctx,
				  xml_node_t *parent, xml_namespace_t *ns,
				  const char *name, const char *value)
{
	xmlNodePtr node;
	node = xmlNewTextChild((xmlNodePtr) parent, (xmlNsPtr) ns,
			       (const xmlChar *) name, (const xmlChar *) value);
	return (xml_node_t *) node;
}


xml_node_t * xml_node_create_text_ns(struct xml_node_ctx *ctx,
				     xml_node_t *parent, const char *ns_uri,
				     const char *name, const char *value)
{
	xmlNodePtr node;
	xmlNsPtr ns;

	node = xmlNewTextChild((xmlNodePtr) parent, NULL,
			       (const xmlChar *) name, (const xmlChar *) value);
	ns = xmlNewNs(node, (const xmlChar *) ns_uri, NULL);
	xmlSetNs(node, ns);
	return (xml_node_t *) node;
}


void xml_node_set_text(struct xml_node_ctx *ctx, xml_node_t *node,
		       const char *value)
{
	/* TODO: escape XML special chars in value */
	xmlNodeSetContent((xmlNodePtr) node, (xmlChar *) value);
}


xml_node_t * xml_node_first_child(struct xml_node_ctx *ctx,
				  xml_node_t *parent)
{
	return (xml_node_t *) ((xmlNodePtr) parent)->children;
}


xml_node_t * xml_node_next_sibling(struct xml_node_ctx *ctx,
				   xml_node_t *node)
{
	return (xml_node_t *) ((xmlNodePtr) node)->next;
}


int xml_node_is_element(struct xml_node_ctx *ctx, xml_node_t *node)
{
	return ((xmlNodePtr) node)->type == XML_ELEMENT_NODE;
}


char * xml_node_get_text(struct xml_node_ctx *ctx, xml_node_t *node)
{
	if (xmlChildElementCount((xmlNodePtr) node) > 0)
		return NULL;
	return (char *) xmlNodeGetContent((xmlNodePtr) node);
}


void xml_node_get_text_free(struct xml_node_ctx *ctx, char *val)
{
	if (val)
		xmlFree((xmlChar *) val);
}


char * xml_node_get_base64_text(struct xml_node_ctx *ctx, xml_node_t *node,
				int *ret_len)
{
	char *txt;
	unsigned char *ret;
	size_t len;

	txt = xml_node_get_text(ctx, node);
	if (txt == NULL)
		return NULL;

	ret = base64_decode(txt, strlen(txt), &len);
	if (ret_len)
		*ret_len = len;
	xml_node_get_text_free(ctx, txt);
	if (ret == NULL)
		return NULL;
	txt = os_malloc(len + 1);
	if (txt == NULL) {
		os_free(ret);
		return NULL;
	}
	os_memcpy(txt, ret, len);
	txt[len] = '\0';
	os_free(ret);
	return txt;
}


struct xml_node_ctx * xml_node_init_ctx(void *upper_ctx,
					const void *env)
{
	struct xml_node_ctx *xctx;

	xctx = os_zalloc(sizeof(*xctx));
	if (xctx == NULL)
		return NULL;
	xctx->ctx = upper_ctx;

	LIBXML_TEST_VERSION

	return xctx;
}


void xml_node_deinit_ctx(struct xml_node_ctx *ctx)
{
	xmlSchemaCleanupTypes();
	xmlCleanupParser();
	xmlMemoryDump();
	os_free(ctx);
}

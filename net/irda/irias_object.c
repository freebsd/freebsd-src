/*********************************************************************
 *                
 * Filename:      irias_object.c
 * Version:       0.3
 * Description:   IAS object database and functions
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Thu Oct  1 22:50:04 1998
 * Modified at:   Wed Dec 15 11:23:16 1999
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 * 
 *     Copyright (c) 1998-1999 Dag Brattli, All Rights Reserved.
 *      
 *     This program is free software; you can redistribute it and/or 
 *     modify it under the terms of the GNU General Public License as 
 *     published by the Free Software Foundation; either version 2 of 
 *     the License, or (at your option) any later version.
 *  
 *     Neither Dag Brattli nor University of Tromsø admit liability nor
 *     provide warranty for any of this software. This material is 
 *     provided "AS-IS" and at no charge.
 *     
 ********************************************************************/

#include <linux/string.h>
#include <linux/socket.h>

#include <net/irda/irda.h>
#include <net/irda/irmod.h>
#include <net/irda/irias_object.h>

hashbin_t *objects = NULL;

/*
 *  Used when a missing value needs to be returned
 */
struct ias_value missing = { IAS_MISSING, 0, 0, 0, {0}};

/*
 * Function strndup (str, max)
 *
 *    My own kernel version of strndup!
 *
 * Faster, check boundary... Jean II
 */
char *strndup(char *str, int max)
{
	char *new_str;
	int len;
	
	/* Check string */
	if (str == NULL)
		return NULL;
	/* Check length, truncate */
	len = strlen(str);
	if(len > max)
		len = max;

	/* Allocate new string */
        new_str = kmalloc(len + 1, GFP_ATOMIC);
        if (new_str == NULL) {
		WARNING("%s(), Unable to kmalloc!\n", __FUNCTION__);
		return NULL;
	}

	/* Copy and truncate */
	memcpy(new_str, str, len);
	new_str[len] = '\0';
	
	return new_str;
}

/*
 * Function ias_new_object (name, id)
 *
 *    Create a new IAS object
 *
 */
struct ias_object *irias_new_object( char *name, int id)
{
        struct ias_object *obj;
	
	IRDA_DEBUG( 4, "%s()\n", __FUNCTION__);

	obj = (struct ias_object *) kmalloc(sizeof(struct ias_object), 
					    GFP_ATOMIC);
	if (obj == NULL) {
		IRDA_DEBUG(0, "%s(), Unable to allocate object!\n", __FUNCTION__);
		return NULL;
	}
	memset(obj, 0, sizeof( struct ias_object));

	obj->magic = IAS_OBJECT_MAGIC;
	obj->name = strndup(name, IAS_MAX_CLASSNAME);
	obj->id = id;

	obj->attribs = hashbin_new(HB_LOCAL);
	
	return obj;
}

/*
 * Function irias_delete_attrib (attrib)
 *
 *    Delete given attribute and deallocate all its memory
 *
 */
void __irias_delete_attrib(struct ias_attrib *attrib)
{
	ASSERT(attrib != NULL, return;);
	ASSERT(attrib->magic == IAS_ATTRIB_MAGIC, return;);

	if (attrib->name)
		kfree(attrib->name);

	irias_delete_value(attrib->value);
	attrib->magic = ~IAS_ATTRIB_MAGIC;
	
	kfree(attrib);
}

void __irias_delete_object(struct ias_object *obj)
{
	ASSERT(obj != NULL, return;);
	ASSERT(obj->magic == IAS_OBJECT_MAGIC, return;);

	if (obj->name)
		kfree(obj->name);
	
	hashbin_delete(obj->attribs, (FREE_FUNC) __irias_delete_attrib);
	
	obj->magic = ~IAS_OBJECT_MAGIC;
	
	kfree(obj);
}

/*
 * Function irias_delete_object (obj)
 *
 *    Remove object from hashbin and deallocate all attributes assosiated with
 *    with this object and the object itself
 *
 */
int irias_delete_object(struct ias_object *obj) 
{
	struct ias_object *node;

	ASSERT(obj != NULL, return -1;);
	ASSERT(obj->magic == IAS_OBJECT_MAGIC, return -1;);

	node = hashbin_remove(objects, 0, obj->name);
	if (!node)
		return 0; /* Already removed */

	__irias_delete_object(node);

	return 0;
}

/*
 * Function irias_delete_attrib (obj)
 *
 *    Remove attribute from hashbin and, if it was the last attribute of
 *    the object, remove the object as well.
 *
 */
int irias_delete_attrib(struct ias_object *obj, struct ias_attrib *attrib) 
{
	struct ias_attrib *node;

	ASSERT(obj != NULL, return -1;);
	ASSERT(obj->magic == IAS_OBJECT_MAGIC, return -1;);
	ASSERT(attrib != NULL, return -1;);

	/* Remove attribute from object */
	node = hashbin_remove(obj->attribs, 0, attrib->name);
	if (!node)
		return 0; /* Already removed or non-existent */

	/* Deallocate attribute */
	__irias_delete_attrib(node);

	/* Check if object has still some attributes */
	node = (struct ias_attrib *) hashbin_get_first(obj->attribs);
	if (!node)
		irias_delete_object(obj);

	return 0;
}

/*
 * Function irias_insert_object (obj)
 *
 *    Insert an object into the LM-IAS database
 *
 */
void irias_insert_object(struct ias_object *obj)
{
	ASSERT(obj != NULL, return;);
	ASSERT(obj->magic == IAS_OBJECT_MAGIC, return;);
	
	hashbin_insert(objects, (irda_queue_t *) obj, 0, obj->name);
}

/*
 * Function irias_find_object (name)
 *
 *    Find object with given name
 *
 */
struct ias_object *irias_find_object(char *name)
{
	ASSERT(name != NULL, return NULL;);

	return hashbin_find(objects, 0, name);
}

/*
 * Function irias_find_attrib (obj, name)
 *
 *    Find named attribute in object
 *
 */
struct ias_attrib *irias_find_attrib(struct ias_object *obj, char *name)
{
	struct ias_attrib *attrib;

	ASSERT(obj != NULL, return NULL;);
	ASSERT(obj->magic == IAS_OBJECT_MAGIC, return NULL;);
	ASSERT(name != NULL, return NULL;);

	attrib = hashbin_find(obj->attribs, 0, name);
	if (attrib == NULL)
		return NULL;

	return attrib;
}

/*
 * Function irias_add_attribute (obj, attrib)
 *
 *    Add attribute to object
 *
 */
void irias_add_attrib( struct ias_object *obj, struct ias_attrib *attrib,
		       int owner)
{
	ASSERT(obj != NULL, return;);
	ASSERT(obj->magic == IAS_OBJECT_MAGIC, return;);
	
	ASSERT(attrib != NULL, return;);
	ASSERT(attrib->magic == IAS_ATTRIB_MAGIC, return;);

	/* Set if attrib is owned by kernel or user space */
	attrib->value->owner = owner;

	hashbin_insert(obj->attribs, (irda_queue_t *) attrib, 0, attrib->name);
}

/*
 * Function irias_object_change_attribute (obj_name, attrib_name, new_value)
 *
 *    Change the value of an objects attribute.
 *
 */
int irias_object_change_attribute(char *obj_name, char *attrib_name, 
				  struct ias_value *new_value) 
{
	struct ias_object *obj;
	struct ias_attrib *attrib;

	/* Find object */
	obj = hashbin_find(objects, 0, obj_name);
	if (obj == NULL) {
		WARNING("%s(), Unable to find object: %s\n", __FUNCTION__,
			obj_name);
		return -1;
	}

	/* Find attribute */
	attrib = hashbin_find(obj->attribs, 0, attrib_name);
	if (attrib == NULL) {
		WARNING("%s(), Unable to find attribute: %s\n", __FUNCTION__,
			attrib_name);
		return -1;
	}
	
	if ( attrib->value->type != new_value->type) {
		IRDA_DEBUG( 0, "%s(), changing value type not allowed!\n", __FUNCTION__);
		return -1;
	}

	/* Delete old value */
	irias_delete_value(attrib->value);
	
	/* Insert new value */
	attrib->value = new_value;

	/* Success */
	return 0;
}

/*
 * Function irias_object_add_integer_attrib (obj, name, value)
 *
 *    Add an integer attribute to an LM-IAS object
 *
 */
void irias_add_integer_attrib(struct ias_object *obj, char *name, int value,
			      int owner)
{
	struct ias_attrib *attrib;

	ASSERT(obj != NULL, return;);
	ASSERT(obj->magic == IAS_OBJECT_MAGIC, return;);
	ASSERT(name != NULL, return;);
	
	attrib = (struct ias_attrib *) kmalloc(sizeof(struct ias_attrib), 
					       GFP_ATOMIC);
	if (attrib == NULL) {
		WARNING("%s(), Unable to allocate attribute!\n", __FUNCTION__);
		return;
	}
	memset(attrib, 0, sizeof( struct ias_attrib));

	attrib->magic = IAS_ATTRIB_MAGIC;
	attrib->name = strndup(name, IAS_MAX_ATTRIBNAME);

	/* Insert value */
	attrib->value = irias_new_integer_value(value);
	
	irias_add_attrib(obj, attrib, owner);
}

 /*
 * Function irias_add_octseq_attrib (obj, name, octet_seq, len)
 *
 *    Add a octet sequence attribute to an LM-IAS object
 *
 */

void irias_add_octseq_attrib(struct ias_object *obj, char *name, __u8 *octets,
			     int len, int owner)
{
	struct ias_attrib *attrib;
	
	ASSERT(obj != NULL, return;);
	ASSERT(obj->magic == IAS_OBJECT_MAGIC, return;);
	
	ASSERT(name != NULL, return;);
	ASSERT(octets != NULL, return;);
	
	attrib = (struct ias_attrib *) kmalloc(sizeof(struct ias_attrib), 
					       GFP_ATOMIC);
	if (attrib == NULL) {
		WARNING("%s(), Unable to allocate attribute!\n", __FUNCTION__);
		return;
	}
	memset(attrib, 0, sizeof( struct ias_attrib));
	
	attrib->magic = IAS_ATTRIB_MAGIC;
	attrib->name = strndup(name, IAS_MAX_ATTRIBNAME);
	
	attrib->value = irias_new_octseq_value( octets, len);
	
	irias_add_attrib(obj, attrib, owner);
}

/*
 * Function irias_object_add_string_attrib (obj, string)
 *
 *    Add a string attribute to an LM-IAS object
 *
 */
void irias_add_string_attrib(struct ias_object *obj, char *name, char *value,
			     int owner)
{
	struct ias_attrib *attrib;

	ASSERT(obj != NULL, return;);
	ASSERT(obj->magic == IAS_OBJECT_MAGIC, return;);

	ASSERT(name != NULL, return;);
	ASSERT(value != NULL, return;);
	
	attrib = (struct ias_attrib *) kmalloc(sizeof( struct ias_attrib), 
					       GFP_ATOMIC);
	if (attrib == NULL) {
		WARNING("%s(), Unable to allocate attribute!\n", __FUNCTION__);
		return;
	}
	memset(attrib, 0, sizeof( struct ias_attrib));

	attrib->magic = IAS_ATTRIB_MAGIC;
	attrib->name = strndup(name, IAS_MAX_ATTRIBNAME);

	attrib->value = irias_new_string_value(value);

	irias_add_attrib(obj, attrib, owner);
}

/*
 * Function irias_new_integer_value (integer)
 *
 *    Create new IAS integer value
 *
 */
struct ias_value *irias_new_integer_value(int integer)
{
	struct ias_value *value;

	value = kmalloc(sizeof(struct ias_value), GFP_ATOMIC);
	if (value == NULL) {
		WARNING("%s(), Unable to kmalloc!\n", __FUNCTION__);
		return NULL;
	}
	memset(value, 0, sizeof(struct ias_value));

	value->type = IAS_INTEGER;
	value->len = 4;
	value->t.integer = integer;

	return value;
}

/*
 * Function irias_new_string_value (string)
 *
 *    Create new IAS string value
 *
 * Per IrLMP 1.1, 4.3.3.2, strings are up to 256 chars - Jean II
 */
struct ias_value *irias_new_string_value(char *string)
{
	struct ias_value *value;

	value = kmalloc(sizeof(struct ias_value), GFP_ATOMIC);
	if (value == NULL) {
		WARNING("%s(), Unable to kmalloc!\n", __FUNCTION__);
		return NULL;
	}
	memset( value, 0, sizeof( struct ias_value));

	value->type = IAS_STRING;
	value->charset = CS_ASCII;
	value->t.string = strndup(string, IAS_MAX_STRING);
	value->len = strlen(value->t.string);

	return value;
}


/*
 * Function irias_new_octseq_value (octets, len)
 *
 *    Create new IAS octet-sequence value
 *
 * Per IrLMP 1.1, 4.3.3.2, octet-sequence are up to 1024 bytes - Jean II
 */
struct ias_value *irias_new_octseq_value(__u8 *octseq , int len)
{
	struct ias_value *value;

	value = kmalloc(sizeof(struct ias_value), GFP_ATOMIC);
	if (value == NULL) {
		WARNING("%s(), Unable to kmalloc!\n", __FUNCTION__);
		return NULL;
	}
	memset(value, 0, sizeof(struct ias_value));

	value->type = IAS_OCT_SEQ;
	/* Check length */
	if(len > IAS_MAX_OCTET_STRING)
		len = IAS_MAX_OCTET_STRING;
	value->len = len;

	value->t.oct_seq = kmalloc(len, GFP_ATOMIC);
	if (value->t.oct_seq == NULL){
		WARNING("%s(), Unable to kmalloc!\n", __FUNCTION__);
		kfree(value);
		return NULL;
	}
	memcpy(value->t.oct_seq, octseq , len);
	return value;
}

struct ias_value *irias_new_missing_value(void)
{
	struct ias_value *value;

	value = kmalloc(sizeof(struct ias_value), GFP_ATOMIC);
	if (value == NULL) {
		WARNING("%s(), Unable to kmalloc!\n", __FUNCTION__);
		return NULL;
	}
	memset(value, 0, sizeof(struct ias_value));

	value->type = IAS_MISSING;
	value->len = 0;

	return value;
}

/*
 * Function irias_delete_value (value)
 *
 *    Delete IAS value
 *
 */
void irias_delete_value(struct ias_value *value)
{
	IRDA_DEBUG(4, "%s()\n", __FUNCTION__);

	ASSERT(value != NULL, return;);

	switch (value->type) {
	case IAS_INTEGER: /* Fallthrough */
	case IAS_MISSING:
		/* No need to deallocate */
		break;
	case IAS_STRING:
		/* If string, deallocate string */
		if (value->t.string != NULL)
			kfree(value->t.string);
		break;
	case IAS_OCT_SEQ:
		/* If byte stream, deallocate byte stream */
		 if (value->t.oct_seq != NULL)
			 kfree(value->t.oct_seq);
		 break;
	default:
		IRDA_DEBUG(0, "%s(), Unknown value type!\n", __FUNCTION__);
		break;
	}
	kfree(value);
}




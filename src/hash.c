/* Copyright (C) 2003 Thomas Weckert */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"
#include "mem.h"
#include "str_util.h"


/*
	Create a new hashtable.
*/
HASH_ITEM **init_hashtab(void) {
	register unsigned int i = 0;
	HASH_ITEM **hashtab = (HASH_ITEM **)NULL;


	hashtab = (HASH_ITEM**)_malloc( MAX_HASHSIZE * sizeof(HASH_ITEM*) );

	for (i=0;i<MAX_HASHSIZE;i++) {
		hashtab[i] = (HASH_ITEM*)NULL;
	}

	return ((HASH_ITEM**)hashtab);
}


/*
	Return the modulized hashvalue for a string.
	This hash fucntions isn't very somphisticated,
	but short and effective.
*/
unsigned int hash( char *str ) {
	unsigned int hashval = 0;

	for (hashval=0;*str!='\0';*str++) {
		hashval = *str + 31 * hashval;
	}

	return (hashval % MAX_HASHSIZE);
}


/*
	Return the hash item for a certain key or NULL from
	a given hashtable.
*/
HASH_ITEM *get_hash_item( HASH_ITEM **hashtab, char *key ) {
	unsigned int hashval = 0;
	HASH_ITEM *item = (HASH_ITEM*)NULL;

	if (hashtab==(HASH_ITEM**)NULL) {
		return ((HASH_ITEM*)NULL);
	}

	hashval = hash(key);
	for (item=hashtab[hashval];item!=(HASH_ITEM*)NULL;item=item->next) {
		if (strcmp(key,item->key)==0) {
			return (item);
		}
	}

	return ((HASH_ITEM*)NULL);
}


/*
	Remove a hash item from the hashtable.
	1: success
	0: item was not found
*/
unsigned int del_hash_item( HASH_ITEM **hashtab, char *key ) {
	unsigned int hashval = 0;
	HASH_ITEM
		*current = (HASH_ITEM*)NULL,
		*previous = (HASH_ITEM*)NULL,
		*tmp_item = (HASH_ITEM*)NULL;


	/* The hashtable doesnt exist */
	if (hashtab==(HASH_ITEM**)NULL) {
		return (0);
	}

	/* Check if the item that should be deleted is in the hashtable! */
	hashval = hash(key);
	if (hashtab[hashval]==(HASH_ITEM*)NULL) {
		return (0);
	}

	/* The item was found without a collision */
	if (strcmp((hashtab[hashval]->key),key)==0) {
		tmp_item = hashtab[hashval];
		hashtab[hashval] = tmp_item->next;

		free (tmp_item->key);
		tmp_item->key = (char*)NULL;

		if (tmp_item->type==ITEM_TYPE_STRING) {
			free(tmp_item->data);
			tmp_item->data = (void*)NULL;
		}
		else if(tmp_item->type==ITEM_TYPE_LBUF) {
			free_lbuf(tmp_item->data);
		}

		free(tmp_item);
		return (1);
	}
	else {
		/* We have to look up for the item in the linked list due to a collision */
		current = hashtab[hashval]->next;
		previous = hashtab[hashval];

		while (current!=(HASH_ITEM*)NULL)  {
			if (strcmp(current->data,key)==0) {
				/* The item that should be deleted is found! */
				previous->next = current->next;

				free (current->key);
				current->key = (char*)NULL;

				if (current->type==ITEM_TYPE_STRING) {
					free(current->data);
					current->data = (void*)NULL;
				}
				else if(current->type==ITEM_TYPE_LBUF) {
					free_lbuf(tmp_item->data);
				}

				free(current);
				return (1);
			}
			previous = current;
			current = current->next;
		}
	}

	/* The item that should be deleted is not in the hashtable */
	return (0);
}


/*
	Add a key/data pair to a given hashtable. Existing similar pairs
	will be overwritten.
*/
HASH_ITEM *add_hash_item( HASH_ITEM **hashtab, char *key, void *data, size_t size, unsigned int item_type ) {
	HASH_ITEM *item = (HASH_ITEM*)NULL;
	unsigned int hashval = 0;

	if (hashtab==(HASH_ITEM**)NULL) {
		return ((HASH_ITEM*)NULL);
	}

	if ((item=get_hash_item(hashtab,key))==(HASH_ITEM*)NULL) {
		/* The item is NOT in the hashtable */
		item = (HASH_ITEM*)_malloc( sizeof(HASH_ITEM) );
		if (item==(HASH_ITEM*)NULL) {
			return (HASH_ITEM*)NULL;
		}

		/* Allocate memory for the key */
		item->key = (char*)_malloc( _str_len(key)+1 );
		if (item->key==(char*)NULL) {
			return (HASH_ITEM*)NULL;
		}
		strcpy(item->key,key);
		item->key[_str_len(item->key)] = '\0';

		/* Allocate memory for the data */
		if (item_type==ITEM_TYPE_STRING) {
			item->data = (char*)_malloc( _str_len((char*)data)+1 );
			if ((char*)item->data==(char*)NULL) {
				return (HASH_ITEM*)NULL;
			}
			memcpy(item->data,data,size);
		}
		else if(item_type==ITEM_TYPE_LBUF) {
			item->data = (LINE_BUFFER*)data;
		}

		/* Insert the new item into the hashtab */
		hashval = hash(key);
		item->next = hashtab[hashval];
		hashtab[hashval] = item;

		/* Set the type of the item */
		item->type = item_type;
	}
	else {
		/* The item IS already in the hashtable */
		if (item_type==ITEM_TYPE_STRING) {
			/* Free the old data */
			if (item_type==ITEM_TYPE_STRING) {
				free((char*)item->data);
			}
			else if(item_type==ITEM_TYPE_LBUF) {
				free_lbuf((LINE_BUFFER*)item->data);
				item->data = (void*)NULL;
			}

			/* Allocate memory for the new data */
			if (item_type==ITEM_TYPE_STRING) {
				item->data = (char*)_malloc( _str_len((char*)data)+1 );
				if ((char*)item->data==(char*)NULL) {
					return (HASH_ITEM*)NULL;
				}
				memcpy(item->data,data,size);
			}
			else if(item_type==ITEM_TYPE_LBUF) {
				item->data = (LINE_BUFFER*)data;
			}
		}
	}

	return (item);
}


/*
	Trash a hashtable...
*/
void free_hashtab( HASH_ITEM **hashtab ) {
	register unsigned int i = 0;
	HASH_ITEM
		*del_item = (HASH_ITEM*)NULL,
		*tmp_item = (HASH_ITEM*)NULL;


	if (hashtab==(HASH_ITEM**)NULL) {
		return;
	}

	for (i=0;i<MAX_HASHSIZE;i++) {
		tmp_item = hashtab[i];

		while (tmp_item!=(HASH_ITEM*)NULL) {
			del_item = tmp_item;
			tmp_item = tmp_item->next;

			if (del_item->key!=(char*)NULL) {
				free(del_item->key);
				del_item->key = (char*)NULL;
			}

			if (del_item->data!=(void*)NULL) {
				if (del_item->type==ITEM_TYPE_STRING) {
					free(del_item->data);
				}
				del_item->data = (void*)NULL;
			}
			free(del_item);
			del_item = (HASH_ITEM*)NULL;
		}
	}

	free(hashtab);
	hashtab = (HASH_ITEM**)NULL;
}


/*
	Trash a line_buffer (for example a MHT block).
*/
void free_lbuf( LINE_BUFFER *lbuf ) {
	LINE_BUFFER
		*tmp = (LINE_BUFFER*)NULL;

	while (lbuf!=(LINE_BUFFER*)NULL) {
		tmp = lbuf;
		lbuf = lbuf->next;

		free(tmp->content);
		tmp->content = (char*)NULL;
		tmp->next = (LINE_BUFFER*)NULL;
		free(tmp);
		tmp = (LINE_BUFFER*)NULL;
	}
}

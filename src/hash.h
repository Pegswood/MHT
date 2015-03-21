/* Copyright (C) 2003 Thomas Weckert */

/* Definitions: */
#define MAX_HASHSIZE		16384
#define ITEM_TYPE_STRING	1
#define ITEM_TYPE_LBUF		2


/* Data structures: */
typedef struct HASH_PTR hash_ptr;
typedef struct HASH_PTR {
   hash_ptr *next;
   char *key;
   void *data;
   unsigned int type;
} HASH_ITEM;

typedef struct LBUF_PTR lbuf_ptr;
typedef struct LBUF_PTR {
	char *content;
	lbuf_ptr *next;
} LINE_BUFFER;


/* Prototypes: */
HASH_ITEM *add_hash_item( HASH_ITEM **hashtab, char *key, void *data, size_t size, unsigned int item_type );
HASH_ITEM *get_hash_item( HASH_ITEM **hashtab, char *key );
unsigned int hash( char *str );
HASH_ITEM **init_hashtab(void);
void free_hashtab( HASH_ITEM **hashtab );
unsigned int del_hash_item( HASH_ITEM **hashtab, char *key );
void free_lbuf( LINE_BUFFER *lbuf );

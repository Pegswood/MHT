/* Copyright (C) 2003 Thomas Weckert */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "mem.h"

/*
	Dynamically allocate a copy of a string.
*/
char *strdup( const char *original ) {
	char *result = (char*)NULL;

	result = (char*)_calloc(strlen(original)+1,sizeof(char));
	strcpy(result,original);
	result[strlen(original)] = '\0';

	return (result);
}


/*
	Dynamically allocate a copy of a memory block.
*/
void *memdup( const void *original, size_t size ) {
	void *result = (void *)_malloc(size);
	memcpy( result, original, size );
	return(result);
}


void *_malloc( size_t size ) {
	void *ptr = (void*)NULL;

	ptr = (void*)malloc(size);
	if (ptr==(void*)NULL) {
		fprintf(stdout,"FATA ERROR: cannot allocate new memory!");
		exit(EXIT_FAILURE);
	}

	return (ptr);
}


void *_calloc( unsigned int count, size_t size ) {
	void *ptr = (void*)NULL;

	ptr = (void*)calloc(count,size);
	if (ptr==(void*)NULL) {
		fprintf(stdout,"FATA ERROR: cannot allocate new memory!");
		exit(EXIT_FAILURE);
	}

	return (ptr);
}


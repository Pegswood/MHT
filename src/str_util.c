/* Copyright (C) 2003 Thomas Weckert */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "str_util.h"
#include "mem.h"
#include "mht_defs.h"

/*
	Insert "insert_str" into "dest_str" at pos "insert_pos" by replacing
	"replace_len" chars. "end_len" is the length textblock at the end that
	has to be moved forward or backward.
*/
char *strinsert( char *dest_str, char *insert_str, unsigned int replace_len, char *insert_pos ) {
	unsigned int
		insert_index = insert_pos-dest_str,
		end_len = (int)strlen(dest_str)-replace_len-insert_index,
		insert_len = (int)strlen(insert_str);

	memmove(dest_str+insert_index+insert_len, dest_str+insert_index+replace_len, end_len);
	memcpy(dest_str+insert_index, insert_str, insert_len);
	dest_str[insert_index+insert_len+end_len] = '\0';

	return (dest_str);
}


/*
	Convert a string to its lower case counterpart.
*/
char *strlwr( char *str ) {
	int
		i = 0;

	for (i=0; i<(int)strlen(str); i++) {
		str[i] = tolower(str[i]);
	}

	return (str);
}


/*
	Split str at every char out of sepchars and store the
	single tokens in args.
*/
int strsplit( char *str, char **args, char sepchar, unsigned int max_arg_count ) {
	unsigned int
		str_len = 0,
		i = 0,
		arg_count = 0,
		pos = 0;

	char
		tmp_str[MAX_LEN],
		last_char = '\0';


	tmp_str[0] = '\0';

	for (i=0,str_len=(int)strlen(str);i<str_len;i++) {
		/* The char is not the sepchar, so append it to the current argument */
		if (str[i]!=sepchar) {
			tmp_str[pos] = str[i];
			tmp_str[++pos] = '\0';
		}
		else if (str[i]==sepchar && last_char!=sepchar) {
			/*
				The char is the sepchar. Copy the current argument into the
				args vector. If this was the last argument, return, otherwise
				continue.
			*/
			tmp_str[pos] = '\0';

			if (arg_count<max_arg_count) {
				args[arg_count++] = strdup(tmp_str);
			}
			else {
				return (arg_count);
			}

			tmp_str[0] = '\0';
			pos = 0;
		}
		else if (str[i]==sepchar && last_char==sepchar) {
			/*
				The char is the sepchar, the last char was also the sepchar.
				We have an empty argument, which will be the NULL char!
			*/
			if (arg_count<max_arg_count) {
				args[arg_count++] = '\0';
			}
			else {
				return (arg_count);
			}

			tmp_str[0] = '\0';
			pos = 0;
		}

		last_char = str[i];
	}

	/*
		We went through the entire string, so we have to finish the last
		argument and clear everything up!
	*/
	if (pos>0) {
		tmp_str[pos] = '\0';

		if (arg_count<max_arg_count) {
			args[arg_count++] = strdup(tmp_str);
		}
		else {
			return (arg_count);
		}
	}

	return (arg_count);
}


/*
	Will also for NULL strings return a valid result.
*/
int _str_len( char *str ) {
	if (str==(char*)NULL) return (0);
	return (strlen(str));
}

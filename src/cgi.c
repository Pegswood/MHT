/* Copyright (C) 2003 Thomas Weckert */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "cgi.h"
#include "mht.h"
#include "hash.h"

/* Prototypes: */
char *cgi_c2x( char *from, char *to );
char cgi_x2c( char *hex_str );
void cgi_register_env_vars(void);


#define CGI_ENV_VARS_COUNT		22

char cgi_env_vars[CGI_ENV_VARS_COUNT][16] = {
	"DOCUMENT_ROOT",

	"HTTP_COOKIE",
	"HTTP_HOST",
	"HTTP_REFERER",
	"HTTP_USER_AGENT",
	"HTTPS",

	"PATH_INFO",
	"PATH_TRANSLATED",

	"QUERY_STRING",

	"REMOTE_ADDR",
	"REMOTE_HOST",
	"REMOTE_PORT",
	"REMOTE_USER",

	"REQUEST_METHOD",
	"REQUEST_URI",

	"SCRIPT_FILENAME",
	"SCRIPT_NAME",

	"SERVER_ADMIN",
	"SERVER_NAME",
	"SERVER_PORT",
	"SERVER_SOFTWARE",
	"SERVER_PROTOCOL"
};


/*
	Registers useful CGI environment variables as MHT macros.
*/
void cgi_register_env_vars(void) {
	int i = 0;
	char *var = (char *)NULL;

	for (i=0;i<CGI_ENV_VARS_COUNT;i++) {
		var = getenv(cgi_env_vars[i]);

		if (var!=(char*)NULL) {
			mht_register_macro(cgi_env_vars[i], var);
		}
	}
}


/*
	Read all the CGI input and store it in a hash- table,
	no matter what request method was used.
*/
void cgi_init(void) {
	unsigned int
		content_length = 0;

	char
		*tmp_value = (char*)NULL,
		*dummy = (char*)NULL,
		*method = (char*)NULL,
		*qs = (char*)NULL,
		*content_str = (char*)NULL,
		*data_pair = (char*)NULL,
		*eqpos = (char*)NULL,
		*name = (char*)NULL, *value = (char*)NULL;


	/* Get the request method */
	method = getenv("REQUEST_METHOD");

	/* Someone started this program on the console */
	if (method==(char*)NULL) {
		fprintf(stdout,"This program should run as a CGI!\n");
		exit(1);
	}

	/* Read in the CGI values, either from the environment or STDIN */
	if (strcmp(method,"POST")==0) {
		content_length = atoi(getenv("CONTENT_LENGTH"));

		if (content_length==0) return;

		content_str = (char*)malloc( sizeof(char) * (content_length+2) );
		fread( content_str, content_length, 1, stdin );
		content_str[content_length] = '\0';
	}
	else if (strcmp(method,"GET")==0) {
		qs = getenv("QUERY_STRING");

		if (qs==(char*)NULL) {
			return;
		}
		else {
			content_length = strlen(qs);
		}

		content_str = (char*)malloc( sizeof(char) * (content_length+2) );
		strncpy( content_str, qs, content_length );
		content_str[content_length] = '\0';
	}
	else {
		cgi_err_msg("Unknown or not supported request method!");
		exit(1);
	}

	/* Store the CGI input values as MHT macros */
	tmp_value = (char*)malloc( sizeof(char) * (content_length+2) );
	tmp_value[0] = '\0';
	data_pair = strtok(content_str,"&");
	while (data_pair!=(char*)NULL) {
		if ( (eqpos=strchr(data_pair,'='))!=(char*)NULL ) {
			*eqpos = '\0';
			name = data_pair;
			value = eqpos+1;

			if (strlen(value)>0) {
				cgi_unescape_str(name);
				cgi_unescape_str(value);

				/*
					Check if the macro already exists, for
					example if checkboxes are used: &meat=bacon&meat=salami
				*/
				if ((mht_search_macro(name,&dummy))!=0) {
					sprintf(tmp_value,"%s,%s",dummy,value);
					mht_register_macro(name,tmp_value);
				}
				else {
					sprintf(tmp_value,"%s",value);
					mht_register_macro(name,value);
				}
			}
		}
		data_pair = strtok(NULL,"&");
	}

	cgi_register_env_vars();
	free(content_str);
	free(tmp_value);
	content_str = (char*)NULL;
	tmp_value = (char*)NULL;

	/* Turn off buffering of stdout */
	setvbuf(stdout,NULL,_IONBF,0);
}



/*
	Show a simple HTML error page.
*/
void cgi_err_msg( char *err_msg ) {
	fprintf( stdout, "<HTML>\n<HEAD>\n<TITLE>CGIMHT Error!</TITLE>\n</HEAD>\n<BODY BGCOLOR=\"#FFFFFF\" TEXT=\"#000000\" LINK=\"#FF0000\" VLINK=\"#FFA200\" ALINK=\"#FFA200\">\n<H3><FONT COLOR=\"#FF0000\">CGIMHT Error!</FONT></H3>\n<HR>\n<B>%s</B>\n</BODY>\n</HTML>",err_msg);
}


/*
	Free up allocated resources resources.
*/
void cgi_exit(void) {
	fflush(stdout);
	return;
}


/*
	Convert a two-char hex string into the char it represents.
*/
char cgi_x2c( char *hex_str ) {
   register char digit;

   digit = (hex_str[0] >= 'A' ? ((hex_str[0] & 0xdf)-'A')+10 : (hex_str[0]-'0'));
   digit *= 16;
   digit += (hex_str[1] >= 'A' ? ((hex_str[1] & 0xdf)-'A')+10 : (hex_str[1]-'0'));
   return (digit);
}


/*
	Convert a char into the two-char hex string it represents.
*/
char *cgi_c2x( char *from, char *to ) {
	sprintf( to, "%%%02x", (int)*from );
    return (to);
}


/*
	Reduce any %xx escape sequences to the characters they represent.
*/
void cgi_unescape_str( char *str ) {
    register unsigned int i, j;

    for (i=0; i<strlen(str); i++) {
		if (str[i] == '+') {
			str[i] = ' ';
		}
	}

    for (i=0,j=0; str[j]; ++i,++j) {
        if ( (str[i]=str[j])=='%') {
            str[i] = cgi_x2c(&str[j+1]);
            j+= 2;
        }
    }
    str[i] = '\0';
}


/*
	Escape all the unsave characters in a string into their hexadecimal values.
*/
char *cgi_escape_str( char *str ) {
	char
		*result = (char*)NULL,
		*iptr = (char*)NULL,
		*rptr = (char*)NULL,
		hex[4];


	if (!str) {
		return((char*)NULL);
	}

	result = (char*)malloc( sizeof(char) * ((strlen(str)*4)+1) );
	rptr = result;

	for (iptr=str; *iptr; *iptr++) {
		*rptr = '\0';

		if (isalnum(*iptr)) {
			*(rptr++) = *iptr;
		}
		else {
			if (*iptr==' ') {
				*(rptr++) = '+';
			}
			else {
				strcat(rptr, cgi_c2x(iptr,hex));
				rptr += 3;
			}
		}
	}

	*rptr = '\0';
	return (result);
}

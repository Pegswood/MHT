/* Copyright (C) 2003 Thomas Weckert */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mht.h"


/* Definitions: */
#define	HELP_STRING		"This is mht2html, the Macro-Hyper-Text to HTML compiler.\nmht2html supports the following arguments:\n\n-h print this screen\n-v print the release version and compile date\n-p process a MHT file specified by the absolute path\n\nexample usage:\n$> mht2html -p /tmp/file.mht\n\nFor further information about MHT, email to info at weckert.org\n"


/* Prototypes: */
int main( int argc, char **argvv );
void show_mht_error( int mht_error );


int main( int argc, char **argv ) {
	int
		mht_error = 0;

	char
		*mht_version_msg = (char*)NULL;


	/* Process the command line arguments */
	if (argc>=2) {
		/* Initialize MHT */
		mht_init();

		/* Try to read the username, style path and user path from the environment. */
#ifdef WIN32
		mht_register_env("USERNAME","user");
#else
		mht_register_env("USER","user");
#endif
		mht_register_env("MHTUSERPATH","userpath");
		mht_register_env("MHTSCRIPTPATH","scriptpath");

		if (strcmp(argv[1],"-v")==0) {
			mht_search_macro("mht_version_msg",&mht_version_msg);
			fprintf(stdout,"%s\n",mht_version_msg);
		}

		else if (strcmp(argv[1],"-h")==0) {
			fprintf(stdout,"%s",HELP_STRING);
		}

		else if (strcmp(argv[1],"-p")==0) {
			if (argc>=3) {
				/* Process the MHT file */
				mht_error = mht_quickopen(stdout,argv[2]);

				if (mht_error!=0) {
					show_mht_error(mht_error);
					return (0);
				}
			}
			else {
				fprintf(stdout,"%s",HELP_STRING);
			}
		}
		else {
			fprintf(stdout,"%s",HELP_STRING);
		}

		/* Exit MHT */
		mht_exit();
	}
	else {
		fprintf(stdout,"%s",HELP_STRING);
	}

	return (0);
}


void show_mht_error( int mht_error ) {
	char
		*mht_err_msg = (char*)NULL,
		*mht_err_line = (char*)NULL;

	if (mht_error!=0) {
		if ( mht_search_macro("mht_err_msg",&mht_err_msg)!=0) {
			fprintf(stdout,"\nMHT parse error!\nMessage: %s\n",mht_err_msg);
			if ( mht_search_macro("mht_err_line",&mht_err_line)!=0) {
				fprintf(stdout,"Line: %s\n",mht_err_line);
			}
			/*
			if ( mht_search_macro("mht_err_code",&mht_err_code)!=0) {
				fprintf(stdout,"Code: %s\n",mht_err_code);
			}
			*/
		}
		else {
			fprintf(stdout,"\nMHT-Error:\nCode:%d\n",mht_error);
		}
	}
}


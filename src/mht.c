/* Copyright (C) 2003 Thomas Weckert */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>

#include "hash.h"
#include "mht.h"
#include "mht_defs.h"
#include "mem.h"
#include "str_util.h"


/* Definitions: */
#define MAX_ARG_COUNT			32			/* The max. number of allowed arguments of a macro */
#define	MHT_UMLAUT_COUNT		7			/* The number of currently supported umlauts */
#define MAX_IF_COUNT			128			/* The max. number of nested if-conditionals */
#define MAX_MHT_KEYW_LEN		15			/* The max. length of a MHT keyword */
#define MAX_MHT_KEYW_COUNT		22			/* The number of currently supported MHT keywords */
#define MHT_VERSION				"1.2"		/* The current MHT version string */
#define	MAX_OUTFILE_HANDLES		65			/* 0 is a imaginary file handle to print to all file handles! */
#define MAX_FILE_INCLUSION		128			/* The max. number of included files (file 1 includes file 2, file 2 includes file 3, ..., file 127 includes file 128 */


/* Error codes: */
#define MHT_OK										0
#define MHT_ERR_FILE_NOT_FOUND						1
#define MHT_ERR_BEGIN_DIRECTIVE_INSIDE_BLOCK		2
#define MHT_ERR_BEGIN_DIRECTIVE_FHANDLE_NOT_CLOSED	3
#define MHT_ERR_END_DIRECTIVE_FHANDLE_NOT_CLOSED	4
#define MHT_ERR_END_DIRECTIVE_BLOCKNAME_WRONG		5
#define MHT_ERR_SETVAR_PARAM_MISSING				6
#define MHT_ERR_SETVAR_PARAM_UNRECOGNIZED			7
#define MHT_ERR_SETVAR_VAR_UNRECOGNIZED				8
#define	MHT_ERR_SETFILE_IO_OPERATION_MISSING		9
#define MHT_ERR_SETFILE_IO_OPERATION_UNRECOGNIZED	10
#define MHT_ERR_SETFILE_IO_FNAME_MISSING			11
#define MHT_ERR_SETFILE_IO_DRIVE_FULL				12
#define MHT_ERR_SETFILE_IO_DRIVE_WRITE_PROTECTED	13
#define MHT_ERR_SETFILE_IO_DRIVE_GENERAL_IO_ERR		14
#define MHT_ERR_SETFILE_IO_TEXT_FHANDLE_NOT_CLOSED	15
#define MHT_ERR_SETFILE_IO_FHANDLE_MISSING			16
#define MHT_ERR_SETFILE_IO_UNKNOWN_ACTION			17
#define MHT_ERR_SETFILE_IO_UNKNOWN_FILETYPE			18
#define MHT_ERR_IF_COUNT_TOO_MANY_LEVELS			19
#define	MHT_ERR_IF_COUNT_IF_OR_ELIF_WITHOUT_ARG		20
#define MHT_ERR_IF_COUNT_TOO_MANY_ENDIF				21
#define MHT_ERR_IF_COUNT_IF_IS_MISSING				22
#define MHT_ERR_IF_COUNT_ARGUMENT_WRONG_ARG			23
#define MHT_ERR_IF_COUNT_ENDIF_IS_MISSING			24
#define	MHT_ERR_PROCESS_BLOCK_NOT_FOUND				25
#define	MHT_ERR_PROCESS_DIRECTIVE_WITHOUT_ARGS		26
#define	MHT_ERR_DEF_DIRECTIVE_WITHOUT_ARGS			27
#define MHT_ERR_DEF_DIRECTIVE_WITHOUT_DEFINITION	28
#define MHT_ERR_DEFEX_DIRECTIVE_WITHOUT_ARGS		29
#define MHT_ERR_DEFEX_DIRECTIVE_WITHOUT_DEFINITION	30
#define MHT_ERR_UNDEF_DIRECTIVE_WITHOUT_ARGS		31
#define MHT_ERR_UNDEF_BLOCK_DIRECTIVE_WITHOUT_ARGS	32
#define MHT_ERR_INCLUDE_DIRECTIVE_WITHOUT_ARGS		33
#define MHT_ERR_MHTVAR_DIRECTIVE_WITHOUT_ARGS		34
#define MHT_ERR_LOOP_DIRECTIVE_WITHOUT_ARGS			35
#define MHT_ERR_LOOP_WITHOUT_ENOUGH_PARAMETERS		36
#define MHT_ERR_LOOP_NO_INTEGER_PARAMETERS			37
#define MHT_ERR_TOO_DEEP_FILE_INCLUSION				38
#define MHT_ERR_END_DIRECTIVE_OUTSIDE_BLOCK			39


/* These macros make the code to maintain the if-contexts/levels correct more readable: */
#define INC_IF_CONTEXT		mht.current_if_context++
#define DEC_IF_CONTEXT		mht.current_if_context--
#define GET_IF_CONTEXT		mht.current_if_context
#define INC_IF_LEVEL		mht.if_context[mht.current_if_context].current_if_level++
#define DEC_IF_LEVEL		mht.if_context[mht.current_if_context].current_if_level--
#define GET_IF_LEVEL		mht.if_context[mht.current_if_context].current_if_level


/*
	You can save a little time by checking the first characters
	of the strings in question before doing the call to strcmp.
*/
#define QUICK_STRCMP(a,b)	(*(a)!=*(b) ? (int)((unsigned char) *(a) - (unsigned char) *(b)) : strcmp((a), (b)))


/* Data structures: */


/*
	Every conditional block if-else-endif is
	represented by an instance of this struct.
*/
typedef struct {
	unsigned int was_true;	/* There has already been a true conditional block? */
	unsigned int is_true;	/* The current conditional block is true? */
	unsigned int there_is_an_if;	/* In case of an else/elsif, was there a if conditional? */
} COND_INFO;


/* Every MHT file has its own conditional context. */
typedef struct {
	unsigned int current_if_level; /* The level of a nested if-conditional block */
	COND_INFO if_stack[MAX_IF_COUNT]; /* The conditional infos are stored in a stack, its size also determines the max. number of nested if-conditionals */
} COND_CONTEXT;


/* Global data structure of the MHT processor */
typedef struct {
	HASH_ITEM **macros;		/* All MHT macros are stored in this hash */
	HASH_ITEM **blocks;		/* All MHT blocks are stored in this hash */
	HASH_ITEM **block_params;	/* All parameters of invoked blocks are stored in this hash */
	FILE *out;		/* The current MHT output stream */
	FILE *out_bak;	/* In case a new output stream was choosen, the previous one is saved here to switch back */
	unsigned int conv_umlauts;	/* 1 if German umlauts should be converted into their HTML equivalents, 0 otherwise */
	unsigned int write_to_file;	/* 1 if one or more file handle(s) are opened, 0 otherwise */
	unsigned int read_block;	/* 1 if MHT reads the lines of a block in quickopen, 0 otherwise */
	unsigned int killspace;		/* 1 if MHT should remove all whitespaces, 0 otherwise */
	unsigned int writeoutput;	/* 0 if MHT shouldn't write to the output stream(s), 1 otherwise */
	int active_fhandle;		/* The currently active file handle to which MHT is writing */
	unsigned int fhandles;	/* The number of "typed" file handles */
	char **fhandle_type;	/* Pointer array of all file types */
	FILE **fhandle_fptr;	/* Pointer array of all file handles */
	COND_CONTEXT if_context[MAX_FILE_INCLUSION];	/* All if-conditional contexts are stored in this array */
	unsigned int current_if_context;	/* The level of the current if-conditional context */
	unsigned int recursive_file_inclusion;	/* How many files (a includes b, b, includes c,...) have been included so far? */
	unsigned int error_macros_registered;
} MHT_INFO;


/* Global vars: */
char
	tmp_str[MAX_LEN],
	tmp_str1[MAX_LEN];

MHT_INFO mht;

char *mht_html_umlauts[MHT_UMLAUT_COUNT] = {
	"&auml;", "&ouml;", "&uuml;", "&Auml;", "&Ouml;", "&Uuml;", "&szlig;"
};


char mht_html_replace_chars[MHT_UMLAUT_COUNT] = {
	'ä', 'ö', 'ü', 'Ä', 'Ö', 'Ü', 'ß'
};

#ifdef WIN32
char mht_win_umlauts[MHT_UMLAUT_COUNT] = {
	L'\344', L'\366', L'\374', L'\304', L'\326', L'\334', L'\337'
};
#endif


char mht_ger_wday[7][11] = {
	"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"
};

char mht_ger_month[12][10] = {
	"Januar", "Februar", "März", "April", "Mai", "Juni", "Juli", "August",
	"September", "Oktober", "November", "Dezember"
};

char mht_eng_wday[7][10] = {
	"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

char mht_eng_month[12][10] = {
	"January", "February", "March", "April", "May", "June", "July", "August",
	"September", "October", "November", "December"
};


/* All MHT keywords in alphabetical order */
char mht_keyw[MAX_MHT_KEYW_COUNT][MAX_MHT_KEYW_LEN] = {
	"begin", "def", "defex", "echo", "echoln",
	"elif", "else", "end", "endif", "file", "if", "include",
	"loop", "mhtexit", "mhtfile", "mhtvar", "pause",
	"process", "undef", "undefblock", "write", "writeln"
};

/* MHT error messages */
char *mht_error_str[] = {
	"No errors!",
	"Specified MHT (include) file not found!",
	"#begin directive inside a block found, blocks may not be cascaded!",
	"Outside a block opened filehandles must be closed before a #begin directive!",
	"Inside a block opened filehandles must be closed before an #end directive!",
	"The blocknames of the #begin and #end directive do not match!",
	"#mhtvar was called without a value!",
	"#mhtvar was called without a variable!",
	"#mhtvar was called with an unknown variable!",
	"#mhtfile was called without an operation!",
	"#mhtfile was called with an unknown operation!",
	"Cannot open a filehandle without a given filename!",
	"Cannot open a filehandle, drive is full!",
	"Cannot open a filehandle, drive is write protected!",
	"Cannot open a filehandle, a general I/O error occured!",
	"Another open filehandle must be closed before a new filehandle can be opened!",
	"Empty #file directive without any arguments found!",
	"Unknown action in a #mhtfile directive found! Valid actions are type or open.",
	"Undefined file type in a #mhtfile or #file directive found!",
	"You have too many cascaded #if directives!",
	"Empty #if/#elif directive without any arguments!",
	"The number of #if and #endif directives do not match!",
	"Before an #else or #elif directive, a #if directive is needed!",
	"The argument for #if/#elif should be {TRUE|FALSE} or {1|0}!",
	"There is an #if directive which is not closed by #endif found!",
	"Block in #process directive not found!",
	"Empty #process directive without any arguments found!",
	"Empty #def directive without any arguments found!",
	"There is a #def directive with a macro, but without a definition found!",
	"Empty #defex directive without any arguments found!",
	"There is a #defex directive with a macro, but without a definition found!",
	"Empty #undef directive without any arguments found!",
	"Empty #undefblock directive without any arguments found!",
	"Empty #include directive without any arguments found!",
	"Empty #mhtvar directive without any arguments found!",
	"Block in #loop directive not found!",
	"There is a #loop directive with insufficient parameters found!",
	"There is a #loop directive with non-digit parameters found!",
	"Too many recursive file inclusions!",
	"#end directive without a opening #begin directive found!"
};


/* Prototypes aof all "private" functions: */
int mht_register_block( char *block_name, LINE_BUFFER *first_line );
LINE_BUFFER *mht_search_block( char *block_name );
int mht_process_line( char *line );
void mht_print_line( char *line );
int mht_setvar( char *mhtvar, char *value );
char *mht_replace_macro_params( int macro_arg_count, char **macro_args, char *expanded_macro );
char *mht_replace_umlauts( char *str );
int mht_setfile_io( char *action, char *type, char *fname );
int mht_set_if_count( char *if_directive, char *if_arg );
unsigned int mht_search_block_param( char *block_param, char **result );
int mht_register_block_param( char *block_param, char *param );
int mht_undef_block_param( char *block_param );
char *mht_killspace( char *str );
char *is_mht_keyword( char *pos );
int keyw_strcmp( const void *el1, const void *el2 );
char *mht_expand( char *input );
int mht_free_block( char *block );
int mht_get_block_params( char *str, char **args );
void mht_replace_unexpanded_params( int macro_arg_count, char **macro_args );
int mht_process_with_params( FILE *out, char *blockname, char **block_params, int block_param_count );
char *mht_trim( char *line );
int mht_loop( FILE *out, char *blockname, char **block_params, int block_param_count );
void mht_register_error_macros( char *err_msg, char *err_line, int err_code );


/* Implementation: */

/*
	Initialize the MHT structure.
*/
void mht_init(void) {
	unsigned int
		year = 0,
		month = 0,
		wday = 0,
		mday = 0,
		i = 0, j = 0;

	time_t rawtime;
	struct tm *timeinfo;
	char time_string[128];


	mht.macros = init_hashtab();
	mht.blocks = init_hashtab();
	mht.block_params = init_hashtab();

	/* register some basic macros */
	time(&rawtime);
	timeinfo = localtime(&rawtime);

	strftime(time_string, 128, "%m/%d/%Y", timeinfo);
	mht_register_macro("short_date",time_string);
	strftime(time_string,128,"%d.%m.%Y", timeinfo);
	mht_register_macro("kurzes_datum",time_string);

	year = (timeinfo->tm_year < 2000) ? 1900 + timeinfo->tm_year : timeinfo->tm_year;
	month = timeinfo->tm_mon;
	wday = timeinfo->tm_wday;
	mday = timeinfo->tm_mday;

	sprintf(time_string,"%s, %s %d, %d", mht_eng_wday[wday], mht_eng_month[month], mday, year);
	mht_register_macro("long_date",time_string);
	sprintf(time_string,"%s, %d. %s %d", mht_ger_wday[wday], mday, mht_ger_month[month], year);
	mht_register_macro("langes_datum",time_string);

	if (timeinfo->tm_min<10) {
		sprintf(tmp_str,"0%d",timeinfo->tm_min);
	}
	else {
		sprintf(tmp_str,"%d",timeinfo->tm_min);
	}
	sprintf(time_string,"%d:%s",timeinfo->tm_hour,tmp_str);
	mht_register_macro("time",time_string);

	mht_register_macro("crlf","\n");
	mht_register_macro("space"," ");
	mht_register_macro("tab","\t");
	mht_register_macro("null","");
	sprintf(tmp_str,"MHT macro processor version %s, compiled %s",MHT_VERSION,__DATE__);
	mht_register_macro("mht_version_msg",tmp_str);

	/* Default settings of the MHT vars */
	mht.out = stdout;
	mht.out_bak = (FILE*)NULL;
	mht.conv_umlauts = 0;
	mht.write_to_file = 0;
	mht.killspace = 0;
	mht.writeoutput = 1;

	/* Set the IF-stack to default values */
	for (i=0;i<MAX_FILE_INCLUSION;i++) {
		for (j=0;j<MAX_IF_COUNT;j++) {
			mht.if_context[i].if_stack[j].is_true = 0;
			mht.if_context[i].if_stack[j].was_true = 0;
			mht.if_context[i].if_stack[j].there_is_an_if = 0;
		}
		mht.if_context[i].current_if_level = 0;
	}

	mht.current_if_context = 0;
	mht.read_block = 0;
	mht.recursive_file_inclusion = 0;
	mht.error_macros_registered = 0;

	/* Initialize output file handles */
	mht.active_fhandle = -1;
	mht.fhandles = 0;
	mht.fhandle_fptr = (FILE **)_calloc(MAX_OUTFILE_HANDLES,sizeof(FILE*));
	mht.fhandle_type = (char **)_calloc(MAX_OUTFILE_HANDLES,sizeof(char*));

	for (i=0;i<MAX_OUTFILE_HANDLES;i++) {
		mht.fhandle_fptr[i] = (FILE*)NULL;
		mht.fhandle_type[i] = (char*)NULL;
	}
	mht.fhandle_type[0] = strdup("all");
}


/*
	Trash the MHT structure.
*/
void mht_exit(void) {
	HASH_ITEM *tmp_item = (HASH_ITEM*)NULL;
	register unsigned int i = 0;


	/* Free the MHT macros */
	free_hashtab(mht.macros);

	/* Free the MHT block parameters */
	free_hashtab(mht.block_params);

	/* Check if we have to free the MHT blocks */
	if (mht.blocks==(HASH_ITEM**)NULL) {
		return;
	}

	/* Each MHT block is a line buffer, go and free each block separately! */
	for (i=0; i<MAX_HASHSIZE; i++) {
		tmp_item = mht.blocks[i];

		if (tmp_item!=(HASH_ITEM*)NULL) {
			free_lbuf( tmp_item->data );
		}
	}

	/* Free the hashtable data structure of the MHT blocks */
	free_hashtab(mht.blocks);
}


/*
	Register a new macro. If the macro is already registered,
	the macro is overwritten wit the new definition. Returns
	1 if the macro was successful registered, 0 otherwise.
*/
int mht_register_macro( char *name, char *definition ) {
	return( (add_hash_item(mht.macros,name,(char*)definition,(size_t)_str_len(definition)+1,ITEM_TYPE_STRING))!=(HASH_ITEM*)NULL ? 1 : 0 );
}


/*
	Search for a macro. Returns 1 if the macro is registered, 0 otherwise.
	If the macro is registered, result will point to the definition string
	of the macro.
*/
int mht_search_macro( char *name, char **result ) {
	unsigned int found = 0;
	HASH_ITEM *tmp_item = (HASH_ITEM*)NULL;

	if ((tmp_item=get_hash_item(mht.macros,name))==(HASH_ITEM*)NULL) {
		found = 0;
		(*result) = (char*)NULL;
	}
	else {
		found = 1;
		(*result) = (char*)tmp_item->data;
	}

	return (found);
}


/*
	Register a new MHT block.
*/
int mht_register_block( char *block_name, LINE_BUFFER *first_line ) {
	return ( (add_hash_item(mht.blocks,(char*)block_name,(LINE_BUFFER*)first_line,sizeof(LINE_BUFFER),ITEM_TYPE_LBUF))!=(HASH_ITEM*)NULL ? 1 : 0 );
}


/*
	Search for a registered block.
*/
LINE_BUFFER *mht_search_block( char *block_name ) {
	HASH_ITEM
		*tmp_item = (HASH_ITEM*)NULL;

	tmp_item = get_hash_item(mht.blocks,block_name);
	return ( (tmp_item==(HASH_ITEM*)NULL) ? (LINE_BUFFER*)NULL : (LINE_BUFFER*)tmp_item->data );
}


/*
	Register a block parameter. If the parameter is already registered,
	the parameter is overwritten wit the new definition. Returns 1 if
	the parameter was successful registered, 0 otherwise. The name of a
	block parameter is "block.%n" for the n-th parameter of block "block".
*/
int mht_register_block_param( char *block_param, char *param ) {
	return( (add_hash_item(mht.block_params,block_param,(char*)param,(size_t)_str_len(param)+1,ITEM_TYPE_STRING))!=(HASH_ITEM*)NULL ? 1 : 0 );
}


/*
	Undef (erase) a registered block parameter.
*/
int mht_undef_block_param( char *block_param ) {
	return( del_hash_item(mht.block_params,block_param) );
}


/*
	Search for a block parameter. Returns 1 if the parameter is registered,
	0 otherwise. If the parameter is registered, result will point to the
	definition string of the parameter.
*/
unsigned int mht_search_block_param( char *block_param, char **result ) {
	unsigned int found = 0;
	HASH_ITEM *tmp_item = (HASH_ITEM*)NULL;

	if ((tmp_item=get_hash_item(mht.block_params,block_param))==(HASH_ITEM*)NULL) {
		found = 0;
		(*result) = (char*)NULL;
	}
	else {
		found = 1;
		(*result) = (char*)tmp_item->data;
	}

	return (found);
}


/*
	Undef (erase) a registered macro.
*/
int mht_undef_macro( char *name ) {
	return( del_hash_item(mht.macros,name) );
}


/*
	Free (erase) a MHT block.
*/
int mht_undef_block( char *block ) {
	return( del_hash_item(mht.blocks,block) );
}



/*
	Load a MHT source file. All MHT directives outside a block
	a processed immediately after they are read in, blocks are
	stored in a hash.
*/
int mht_quickopen( FILE *out, char *fname ) {
	FILE
		*fptr = (FILE*)NULL;

	LINE_BUFFER
		*current_line = (LINE_BUFFER*)NULL,
		*current_mht_block = (LINE_BUFFER*)NULL;

	char
		line[MAX_LEN],
		block_name[128],
		*tmp = (char*)NULL,
		*token_ptr = (char*)NULL;

	unsigned int
		mht_err = MHT_OK,
		first_line = 0,
		last_line = 0,
		line_count = 0,
		i = 0;


	mht.out = out;
	fptr = fopen( fname, "r" );

	if (fptr==NULL) {
		mht_register_macro("mht_err_msg",mht_error_str[MHT_ERR_FILE_NOT_FOUND]);
		return (MHT_ERR_FILE_NOT_FOUND);
	}

	if (mht.recursive_file_inclusion>=MAX_FILE_INCLUSION) {
		return (MHT_ERR_TOO_DEEP_FILE_INCLUSION);
	}

	block_name[0] = '\0';
	line[0] = '\0';
	mht.recursive_file_inclusion++;

	/*
		Switch to a new if-context for this MHT file.
		Everything outside the MHT blocks, before any
		if-conditional, is TRUE per default (evident).
	*/
	INC_IF_CONTEXT;
	mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].is_true = 1;

	while (!feof(fptr)) {
		line_count++;
		fgets( line, MAX_LEN, fptr );

		strcpy(tmp_str,line);
		tmp = tmp_str;
		while (isspace(*tmp)) {
			tmp++;
		}

		token_ptr = strtok(tmp," \t\n\r\0");
		last_line = 0;

		if (token_ptr!=(char*)NULL) {
			if ( QUICK_STRCMP(strlwr(token_ptr),"#begin")==0 ) {
				/* A block cannot contain another block */
				if (mht.read_block==1) {
					mht_register_error_macros(mht_error_str[MHT_ERR_BEGIN_DIRECTIVE_INSIDE_BLOCK],line,MHT_ERR_BEGIN_DIRECTIVE_INSIDE_BLOCK);
					return (MHT_ERR_BEGIN_DIRECTIVE_INSIDE_BLOCK);
				}

				/* Filehandles must be closed before reading a block */
				if (mht.write_to_file==1) {
					mht_register_error_macros(mht_error_str[MHT_ERR_BEGIN_DIRECTIVE_FHANDLE_NOT_CLOSED],line,MHT_ERR_BEGIN_DIRECTIVE_FHANDLE_NOT_CLOSED);
					return (MHT_ERR_BEGIN_DIRECTIVE_FHANDLE_NOT_CLOSED);
				}

				token_ptr = strtok( (char*)NULL, " \t\n\r\0" );
				sprintf(block_name,"%s",token_ptr);
				mht.read_block = 1;
				first_line = 1;
				last_line = 0;
			}

			else if ( QUICK_STRCMP(strlwr(token_ptr),"#end")==0 ) {
				/* A block cannot be closed if there wasn't a #begin directive before */
				if (mht.read_block==0) {
					mht_register_error_macros(mht_error_str[MHT_ERR_END_DIRECTIVE_OUTSIDE_BLOCK],line,MHT_ERR_END_DIRECTIVE_OUTSIDE_BLOCK);
					return (MHT_ERR_END_DIRECTIVE_OUTSIDE_BLOCK);
				}

				token_ptr = strtok( (char*)NULL, " \t\n\r\0" );

				/* the blocknames of the #begin and #end directive do not match */
				if (QUICK_STRCMP(block_name,token_ptr)!=0) {
					mht_register_error_macros(mht_error_str[MHT_ERR_END_DIRECTIVE_BLOCKNAME_WRONG],line,MHT_ERR_END_DIRECTIVE_BLOCKNAME_WRONG);
					return (MHT_ERR_END_DIRECTIVE_BLOCKNAME_WRONG);
				}

				/* Before reading a block is finished, a possible open filehandle must be closed */
				if (mht.write_to_file==1) {
					mht_register_error_macros(mht_error_str[MHT_ERR_END_DIRECTIVE_FHANDLE_NOT_CLOSED],line,MHT_ERR_END_DIRECTIVE_FHANDLE_NOT_CLOSED);
					return (MHT_ERR_END_DIRECTIVE_FHANDLE_NOT_CLOSED);
				}

				mht_register_block(block_name,(LINE_BUFFER*)current_mht_block);
				mht.read_block = 0;
				block_name[0] = '\0';
				current_line = (LINE_BUFFER*)NULL;
				last_line = 1;
			}
		}

		if (mht.read_block==1) {
			/* The lines of a block are stored in a simple linked list */
			if (first_line==0) {
				if( current_line!=(LINE_BUFFER*)NULL ) {
					current_line->next = (LINE_BUFFER*)_malloc(sizeof(LINE_BUFFER));
					current_line = current_line->next;
				}
				else {
					current_mht_block = current_line = (LINE_BUFFER*)_malloc(sizeof(LINE_BUFFER));
				}

				current_line->content = (char*)_calloc( (size_t)_str_len(line)+1, sizeof(char) );
				strncpy(current_line->content,line,_str_len(line));
				current_line->content[_str_len(current_line->content)] = '\0';
				current_line->next = (LINE_BUFFER*)NULL;
			}
			else {
				first_line = 0;
			}
		}
		else if (last_line==0) {
			/*
				If we do not read the lines of a block currently, go ahead
				and process the line directly.
			*/
			mht_err = mht_process_line(line);

			if (mht_err!=MHT_OK) {
				fclose(fptr);
				mht_register_error_macros(mht_error_str[mht_err],line,mht_err);
				return (mht_err);
			}
		}

		line[0] = '\0';
	}

	fclose(fptr);
	mht.recursive_file_inclusion--;

	i=GET_IF_LEVEL;
	if (i>0) {
		mht_register_macro("mht_err_msg",mht_error_str[MHT_ERR_IF_COUNT_ENDIF_IS_MISSING]);
		return (MHT_ERR_IF_COUNT_ENDIF_IS_MISSING);
	}

	/*
		Reset the if-stack of the current if-context
		back to default values, and decrement the
		if-context level to process further the
		previous if-context.
	*/
	mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].was_true = 0;
	mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].is_true = 0;
	mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].there_is_an_if = 0;
	DEC_IF_CONTEXT;

	return (MHT_OK);
}


/*
	Process the lines of a block with optional arguments.
*/
int mht_process_with_params( FILE *out, char *blockname, char **block_params, int block_param_count ) {
	int
		i = 0,
		mht_err = MHT_OK;

	char
		block_param[MACRO_LEN];


	/*
		Pls. refer to mht_process to read why this is done here...!
	*/
	if (GET_IF_CONTEXT==0 && GET_IF_LEVEL==0) {
		INC_IF_CONTEXT;
		mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].is_true = 1;
	}

	if (block_params==(char**)NULL) {
		/* The block was called without any parameter, process the block... */
		mht_err = mht_process(out,blockname);
		return (mht_err);
	}

	/*
		If the block was called with parameters, go and register
		each parameter as its own macro.
	*/
	if (block_param_count>0) {
		for (i=1; i<block_param_count; i++) {
			if (block_params[i]!=(char*)NULL) {
				/* Block parameters are registered under the name "block.%n" */
				block_param[0] = '\0';
				sprintf(block_param,"%s.%%%d",blockname,i);
				mht_register_block_param(block_param,block_params[i]);
			}
		}
	}

	/* Process the block... */
	mht_err = mht_process(mht.out,block_params[0]);

	/*
		If the block was called with parameters, go and undefine
		all previously registered block parameters.
	*/
	if (block_param_count>0) {
		for (i=1; i<block_param_count; i++) {
			if (block_params[i]!=(char*)NULL) {
				/* Block parameters are registered under the name "block.%n" */
				block_param[0] = '\0';
				sprintf(block_param,"%s.%%%d",blockname,i);
				mht_undef_block_param(block_param);
			}
		}
	}

	if (GET_IF_CONTEXT==0 && GET_IF_LEVEL==0) {
		mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].was_true = 0;
		mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].is_true = 0;
		mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].there_is_an_if = 0;
		DEC_IF_CONTEXT;
	}

	return (mht_err);
}


/*
	Process the lines of a block with optional arguments.
*/
int mht_loop( FILE *out, char *blockname, char **block_params, int block_param_count ) {
	int
		strlen_block_param2 = 0,
		strlen_block_param3 = 0,
		i = 0,
		start = 0,
		end = 0,
		mht_err = MHT_OK;

	char index_str[16];


	if ( (block_params==(char**)NULL) || (block_param_count<4) ) {
		/* The loop was called without any parameter... */
		return (MHT_ERR_LOOP_WITHOUT_ENOUGH_PARAMETERS);
	}

	/* Check whether the start/end indices are decimal numbers: */
	for (i=0,strlen_block_param2=(int)_str_len(block_params[2]);i<strlen_block_param2;i++) {
		if (!isdigit(block_params[2][i])) {
			return (MHT_ERR_LOOP_NO_INTEGER_PARAMETERS);
		}
	}

	for (i=0,strlen_block_param3=(int)_str_len(block_params[3]);i<strlen_block_param3;i++) {
		if (!isdigit(block_params[3][i])) {
			return (MHT_ERR_LOOP_NO_INTEGER_PARAMETERS);
		}
	}

	/* Get the start/end indices for the for-loop: */
	start = (int)strtol( block_params[2], (char **)NULL, 10 );
	end = (int)strtol( block_params[3], (char **)NULL, 10 );

	index_str[0] = '\0';

	/* Loop the block: */
	for (i=start;i<=end;i++) {
		sprintf( index_str, "%d", i );
		mht_register_macro( block_params[1], index_str );
		mht_err = mht_process( out, block_params[0] );

		if (mht_err!=MHT_OK) {
			return (mht_err);
		}
	}

	return (mht_err);
}


/*
	Process the lines of a MHT block.
*/
int mht_process( FILE *out, char *block_name ) {
	int
		mht_err = MHT_OK;

	LINE_BUFFER
		*line_of_block = (LINE_BUFFER*)NULL;

	/*
		Every MHT template has its own if-context (recursion!).
		So if quickopen reads in a new template, it swithes to
		a new if-context which is TRUE per default. After the
		template is read in, the if-context is decremented again,
		and so always FALSE in case we want to process a block from
		a CGI program!
		So if the if-context and if-level are both 0, it is assumed
		that a block from within a template is processed out of a
		CGI program. In this case, we will do the same as in quickopen:
		increment the if-context, and afterwards decerement it again.
	*/
	if (GET_IF_CONTEXT==0 && GET_IF_LEVEL==0) {
		INC_IF_CONTEXT;
		mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].is_true = 1;
	}

	mht.out = out;
	line_of_block = mht_search_block(block_name);

	if (line_of_block==(LINE_BUFFER*)NULL) {
		mht_register_error_macros(mht_error_str[MHT_ERR_PROCESS_BLOCK_NOT_FOUND],"",MHT_ERR_PROCESS_BLOCK_NOT_FOUND);
		return (MHT_ERR_PROCESS_BLOCK_NOT_FOUND);
	}

	while (line_of_block!=(LINE_BUFFER*)NULL) {
		mht_err = mht_process_line(line_of_block->content);

		if (mht_err!=MHT_OK) {
			mht_register_error_macros(mht_error_str[mht_err],line_of_block->content,mht_err);
			return (mht_err);
		}

		line_of_block = line_of_block->next;
	}

	if (GET_IF_CONTEXT==0 && GET_IF_LEVEL==0) {
		mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].was_true = 0;
		mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].is_true = 0;
		mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].there_is_an_if = 0;
		DEC_IF_CONTEXT;
	}

	return (MHT_OK);
}

void mht_register_error_macros( char *err_msg, char *err_line, int err_code ) {
	if (!mht.error_macros_registered) {
		mht_register_macro("mht_err_msg",err_msg);
		mht_register_macro("mht_err_line",err_line);
		sprintf(tmp_str,"%d",err_code);
		mht_register_macro("mht_err_code",tmp_str);
		mht.error_macros_registered = 1;
	}
}

int keyw_strcmp( const void *el1, const void *el2 ) {
  return QUICK_STRCMP( (char*)el1, (char*)el2 );
}


char *is_mht_keyword( char *pos ) {
  char test_id[MAX_MHT_KEYW_LEN];
  register unsigned int i = 0;

  test_id[0]='\0';
  for (i=0;i<MAX_MHT_KEYW_LEN && isalnum(*pos);test_id[i++]=*(pos++));
  test_id[i]='\0';

  return bsearch(test_id,mht_keyw,MAX_MHT_KEYW_COUNT,sizeof(char[MAX_MHT_KEYW_LEN]),keyw_strcmp);
}


/*
	Process the directives in a single MHT line.
*/
int mht_process_line( char *line ) {
	int
		result = 0,
		param_start = 0,
		fence = 0,
		mht_err = MHT_OK,
		block_param_count = 0;

	char
		*mht_keyw = (char*)NULL,
		tmp_line[MAX_LEN],
		*tmp = (char*)NULL,
		*token_ptr = (char*)NULL,
		token1[MACRO_LEN], token2[MAX_LEN], token3[MAX_LEN],
		*block_params[MAX_ARG_COUNT];


	strcpy(tmp_line,line);
	tmp_line[_str_len(line)] = '\0';
	tmp = tmp_line;
	token1[0] = '\0';
	token2[0] = '\0';
	token3[0] = '\0';

	while (isspace(*tmp)) {
		tmp++;
	}

	if (*tmp=='#') {
		/* Check if it is delayed directive (fence>1) */
		while (*tmp=='#') {
			fence++;
			tmp++;
		}

		if ((token_ptr=strtok(tmp," \t\n\r\0"))==(char*)NULL) return (MHT_OK);

		strlwr(token_ptr);
		if ((mht_keyw=is_mht_keyword(token_ptr))==(char*)NULL) {
			/* The token with a leading '#' is NOT a MHT directive/keyword! */
			return (MHT_OK);
		}

		if (fence>1) {
			/* Ok, it is a delayed directive/keyword, do it from the start again! */
			if (mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].is_true==1) {
				strcpy(tmp_line,line);
				tmp = tmp_line;

				while (isspace(*tmp)) {
					tmp++;
				}

				memmove(tmp,tmp+1,_str_len(tmp));
				tmp_line[_str_len(tmp_line)] = '\0';

				mht_expand(tmp_line);

				if (mht.conv_umlauts==1) {
					mht_replace_umlauts(tmp_line);
				}

				if (mht.killspace==1) {
					mht_killspace(tmp_line);
				}

				if (mht.writeoutput==1) {
					mht_print_line(tmp_line);
				}
			}
			return (MHT_OK);
		}

		/* #if, #elif, #else or #endif directive. */
		if (QUICK_STRCMP(mht_keyw,"if")==0
			|| QUICK_STRCMP(mht_keyw,"elif")==0
			|| QUICK_STRCMP(mht_keyw,"else")==0
			|| QUICK_STRCMP(mht_keyw,"endif")==0) {

			/*
				That is a bit too difficult to do it all in here!
				Just set the if_count correct and then we will see
				if we have to continue right here or return back.
			*/
			sprintf(token1,"%s",mht_keyw);
			token_ptr = strtok((char*)NULL," \t\n\r\0");

			if (token_ptr!=(char*)NULL) {
				sprintf(token2,"%s",token_ptr);
				mht_expand(token2);
			}
			else {
				token2[0] = '\0';
			}

			mht_err = mht_set_if_count(token1,token2);
			return (mht_err);
		}


		if (mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].is_true==1) {
			/* macro definition */
			if (QUICK_STRCMP(mht_keyw,"def")==0) {
				token_ptr = strtok((char*)NULL," \t\n\r\0");

				if (token_ptr==(char*)NULL) {
					return (MHT_ERR_DEF_DIRECTIVE_WITHOUT_ARGS);
				}

				sprintf(token1,"%s",token_ptr);
				mht_expand(token1);

				token_ptr = strtok((char*)NULL,"\t\n\r\0");

				if (token_ptr!=(char*)NULL) {
					sprintf(token2,"%s",token_ptr);
					result = mht_register_macro(token1,token2);
				}
				else {
					return (MHT_ERR_DEF_DIRECTIVE_WITHOUT_DEFINITION);
				}

				return (MHT_OK);
			}


			/* macro definition, but with already expanded_ptr definition! */
			else if (QUICK_STRCMP(mht_keyw,"defex")==0) {
				token_ptr = strtok((char*)NULL," \t\n\r\0");

				if (token_ptr==(char*)NULL) {
					return (MHT_ERR_DEFEX_DIRECTIVE_WITHOUT_ARGS);
				}

				sprintf(token1,"%s",token_ptr);
				mht_expand(token1);

				token_ptr = strtok((char*)NULL,"\t\n\r\0");

				if (token_ptr!=(char*)NULL) {
					sprintf(token2,"%s",token_ptr);
					mht_register_macro(token1, mht_expand(token2));
				}
				else {
					return (MHT_ERR_DEFEX_DIRECTIVE_WITHOUT_DEFINITION);
				}

				return (MHT_OK);
			}


			/* macro undefinition */
			else if (QUICK_STRCMP(mht_keyw,"undef")==0) {
				token_ptr = strtok((char*)NULL," \t\n\r\0");

				if (token_ptr==(char*)NULL) {
					return (MHT_ERR_UNDEF_DIRECTIVE_WITHOUT_ARGS);
				}

				sprintf(token1,"%s",token_ptr);
				mht_expand(token1);

				/* the macro might be a block parameter or a "usual" macro */
				if ((strstr(token1,".%"))!=(char*)NULL) {
					mht_undef_block_param(token1);
				}
				else {
					mht_undef_macro(token1);
				}

				return (MHT_OK);
			}


			/* free a MHT block */
			else if (QUICK_STRCMP(mht_keyw,"undefblock")==0) {
				token_ptr = strtok((char*)NULL," \t\n\r\0");

				if (token_ptr==(char*)NULL) {
					return (MHT_ERR_UNDEF_BLOCK_DIRECTIVE_WITHOUT_ARGS);
				}

				sprintf(token1,"%s",token_ptr);
				mht_expand(token1);
				mht_undef_block(token1);

				return (MHT_OK);
			}


			/* process a block */
			else if (QUICK_STRCMP(mht_keyw,"process")==0) {
				token_ptr = strtok((char*)NULL,"\n\r\0");

				if (token_ptr==(char*)NULL) {
					return (MHT_ERR_PROCESS_DIRECTIVE_WITHOUT_ARGS);
				}

				/*
					Expand the name of the block to be processed and all
					possible block parameters...
				*/
				sprintf(token1,"%s",token_ptr);
				mht_expand(token1);
				mht_trim(token1);

				param_start = strcspn(token1,"|:");

				if (*(token1 + param_start)=='|') {
					/* New form: block|param1|param2|... */
					block_param_count = strsplit(token1,block_params,'|',MAX_ARG_COUNT);
					/* Process the block with optional arguments */
					mht_err = mht_process_with_params(mht.out,block_params[0],block_params,block_param_count);
				}
				else if (*(token1 + param_start)==':') {
					/* Old form: block : param1 param2 ... */
					block_param_count = mht_get_block_params(token1,block_params);
					/* Process the block with optional arguments */
					mht_err = mht_process_with_params(mht.out,block_params[0],block_params,block_param_count);
				}
				else {
					/* The block is called without any parameters: */
					mht_err = mht_process(mht.out,token1);
				}

				return (mht_err);
			}


			/* include another MHT file */
			else if (QUICK_STRCMP(mht_keyw,"include")==0) {
				token_ptr = strtok((char*)NULL," \t\n\r\0");

				if (token_ptr==(char*)NULL) {
					return (MHT_ERR_INCLUDE_DIRECTIVE_WITHOUT_ARGS);
				}

				sprintf(token1,"%s",token_ptr);
				mht_expand(token1);

				mht_err = mht_quickopen(mht.out,token1);
				return (mht_err);
			}


			/* set a MHT var */
			else if (QUICK_STRCMP(mht_keyw,"mhtvar")==0) {
				token_ptr = strtok((char*)NULL," \t\n\r\0");

				if (token_ptr==(char*)NULL) {
					return (MHT_ERR_MHTVAR_DIRECTIVE_WITHOUT_ARGS);
				}

				sprintf(token1,"%s",token_ptr);
				token_ptr = strtok((char*)NULL,"\t\n\r\0");

				if (token_ptr==(char*)NULL) {
					return (MHT_ERR_MHTVAR_DIRECTIVE_WITHOUT_ARGS);
				}

				sprintf(token2,"%s",token_ptr);
				return (mht_setvar(token1,token2));
			}


			/* exit */
			else if (QUICK_STRCMP(mht_keyw,"mhtexit")==0) {
				mht_exit();
				exit(0);
			}


			/* set to which file handle(s) the output should be printed */
			else if (QUICK_STRCMP(mht_keyw,"file")==0) {
				token_ptr = strtok((char*)NULL,"\t\n\r\0");

				if (token_ptr==(char*)NULL) {
					return (MHT_ERR_SETFILE_IO_FHANDLE_MISSING);
				}

				sprintf(token1,"%s",token_ptr);
				mht_expand(token1);

				return (mht_setfile_io(mht_keyw,token1,(char*)NULL));
			}

			/* set MHT file I/O */
			else if (QUICK_STRCMP(mht_keyw,"mhtfile")==0) {
				token_ptr = strtok((char*)NULL," \t\n\r\0");

				if (token_ptr==(char*)NULL) {
					return (MHT_ERR_SETFILE_IO_OPERATION_MISSING);
				}

				sprintf(token1,"%s",token_ptr);

				token_ptr = strtok((char*)NULL," \t\n\r\0");
				if (token_ptr==(char*)NULL) {
					/* #mhtfile close doesn't need the file name as a 2nd/3rd parameter */
					return (mht_setfile_io(token1,(char*)NULL,(char*)NULL));
				}

				sprintf(token2,"%s",token_ptr);
				mht_expand(token2);

				token_ptr = strtok((char*)NULL,"\t\n\r\0");
				if (token_ptr==(char*)NULL) {
					/* #mhtfile type doesn't need a 3rd parameter */
					return (mht_setfile_io(token1,token2,(char*)NULL));
				}

				sprintf(token3,"%s",token_ptr);
				mht_expand(token3);

				/* Finally, it could only be #mhtfile open */
				return (mht_setfile_io(token1,token2,token3));
			}


			/* Echo a expanded line to stdout */
			else if (QUICK_STRCMP(mht_keyw,"echo")==0) {
				token_ptr = strtok((char*)NULL,"\n\r\0");

				if (token_ptr!=(char*)NULL) {
					sprintf(tmp_str1,"%s",token_ptr);
					fprintf(stdout,"%s",mht_expand(tmp_str1));
				}

				return (MHT_OK);
			}


			/* Echo a expanded line to stdout with a trailing newline */
			else if (QUICK_STRCMP(mht_keyw,"echoln")==0) {
				token_ptr = strtok((char*)NULL,"\r\0");

				/*
				if (token_ptr!=(char*)NULL) {
					sprintf(tmp_str1,"%s",token_ptr);
					fprintf(stdout,"%s\n",mht_expand(tmp_str1));
				}
				else {
					fprintf(stdout,"\n");
				}
				*/

				if (token_ptr!=(char*)NULL) {
					sprintf(tmp_str1,"%s",token_ptr);
					mht_expand(tmp_str1);

					if (mht.killspace==1) {
						fprintf(stdout,"%s",mht_killspace(tmp_str1));
					}
					else {
						fprintf(stdout,"%s",tmp_str1);
					}
					/*fprintf(stdout,"\n");*/
				}
				else /*if (mht.killspace==0)*/ {
					fprintf(stdout,"\n");
				}

				return (MHT_OK);
			}


			/*
				Write an expanded line to the current MHT output stream,
				even outside a block! The difference to #echo is, that
				#echo writes to stdout, whereas #write can also write
				to a file, if it was opened via #mhtfile before.
			*/
			else if (QUICK_STRCMP(mht_keyw,"write")==0) {
				token_ptr = strtok((char*)NULL,"\n\r\0");

				if (token_ptr!=(char*)NULL) {
					sprintf(tmp_str1,"%s",token_ptr);
					mht_expand(tmp_str1);

					if (mht.killspace==1) {
						mht_killspace(tmp_str1);
					}

					mht_print_line(tmp_str1);
				}

				return (MHT_OK);
			}


			/*
				Write a expanded_ptr line to the current MHT output stream
				with a trailing newline.
			*/
			else if (QUICK_STRCMP(mht_keyw,"writeln")==0) {
				token_ptr = strtok((char*)NULL,"\r\0");

				if (token_ptr!=(char*)NULL) {
					sprintf(tmp_str1,"%s",token_ptr);
					mht_expand(tmp_str1);

					if (mht.killspace==1) {
						mht_print_line(mht_killspace(tmp_str1));
					}
					else {
						mht_print_line(tmp_str1);
					}
				}
				else if (mht.killspace==0) {
					mht_print_line("\n");
				}

				return (MHT_OK);
			}


			/*
				Interrupt the MHT processing until a key is pressed.
			*/
			else if (QUICK_STRCMP(mht_keyw,"pause")==0) {
				fprintf(stdout,"\nMHT paused: press return to continue...\n");
				fgetc(stdin);
				return (MHT_OK);
			}


			/*
				Call a block n-times (looping)
			*/
			else if (QUICK_STRCMP(mht_keyw,"loop")==0) {
				token_ptr = strtok((char*)NULL,"\t\n\r\0");

				if (token_ptr==(char*)NULL) {
					return (MHT_ERR_LOOP_DIRECTIVE_WITHOUT_ARGS);
				}

				/*
					Expand the name of the block to be processed and all
					possible block parameters...
				*/
				sprintf(token1,"%s",token_ptr);
				mht_expand(token1);

				param_start = strcspn(token1,"|:");

				if (*(token1 + param_start)=='|') {
					/* New form: block|param1|param2|... */
					block_param_count = strsplit(token1,block_params,'|',MAX_ARG_COUNT);
					/* Process the block with optional arguments */
					mht_err = mht_loop(mht.out,block_params[0],block_params,block_param_count);
				}
				else if (*(token1 + param_start)==':') {
					/* Old form: block : param1 param2 ... */
					block_param_count = mht_get_block_params(token1,block_params);
					/* Process the block with optional arguments */
					mht_err = mht_loop(mht.out,block_params[0],block_params,block_param_count);
				}

				return (mht_err);
			}
		}
	}
	else if ( (*tmp_line!='\0' && tmp_line!=(char*)NULL) && (mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].is_true==1) ) {
		/*
			The line doesn't start with a MHT directive! If we are
			inside a MHT block, expand all macros, replace the umlauts
			and print it to the current MHT output stream.
		*/
		mht_expand(tmp_line);

		if (mht.conv_umlauts==1) {
			mht_replace_umlauts(tmp_line);
		}

		if (mht.killspace==1) {
			mht_killspace(tmp_line);
		}

		if (mht.writeoutput==1) {
			mht_print_line(tmp_line);
		}
	}

	return (MHT_OK);
}


/*
	After a line is processed, print it to the output sink(s).
*/
void mht_print_line( char *line ) {
	unsigned int i = 0;

	if (mht.active_fhandle>0) {
		/* Print to a specific file handle */
		if (mht.fhandle_fptr[mht.active_fhandle]!=(FILE*)NULL) {
			fprintf(mht.fhandle_fptr[mht.active_fhandle],"%s",line);
		}
	}
	else if (mht.active_fhandle==0) {
		/* Print to all open file handles */
		for (i=1;i<=mht.fhandles;i++) {
			if (mht.fhandle_fptr[i]!=(FILE*)NULL) {
				fprintf(mht.fhandle_fptr[i],"%s",line);
			}
		}
	}
	else if (mht.active_fhandle<0) {
		/*
			No file handle registered, print to the output
			sink, which is stdout unless other specified.
		*/
		if (mht.out!=(FILE*)NULL) {
			fprintf(mht.out,"%s",line);
		}
	}
}


/*
	Register a environment variable as a MHT macro.
*/
int mht_register_env( char *env_var, char *mht_macro ) {
	char
		*env_val = (char*)NULL;


	env_val = getenv(env_var);

	if ( (env_val!=(char*)NULL) && (env_var!=(char*)NULL) ) {
		return (mht_register_macro(mht_macro,env_val));
	}

	return (0);
}


/*
	Split the arguments of a #process directive of the form
	block : param1 param2 param3 ...
	and store the tokens in args.
*/
int mht_get_block_params( char *str, char **args ) {
	unsigned int
		arg_count = 0;

	char
		*token_ptr = (char*)NULL;


	token_ptr = strtok(str," :\t\n\r\0");

	while (token_ptr!=(char*)NULL) {
		if (QUICK_STRCMP(token_ptr,":")!=0) {
			args[arg_count++] = strdup(token_ptr);
		}
		token_ptr = strtok((char*)NULL," \t\n\r\0");
	}

	return (arg_count);
}


/*
	Process a line with a #if, #elif, #else or #endif
	directive and set the if_count correct. The argument
	for the directive is already expanded_ptr!
*/
int mht_set_if_count( char *if_directive, char *if_arg ) {
	if (GET_IF_LEVEL==MAX_IF_COUNT-1) {
		/* We have too many if levels (128 cascaded #if's should be enough!) */
		return (MHT_ERR_IF_COUNT_TOO_MANY_LEVELS);
	}


	/* A if block is splitted here */
	if (QUICK_STRCMP(if_directive,"else")==0) {
		/* An #else without an #if before is not allowed! */
		if (mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].there_is_an_if==0) {
			return (MHT_ERR_IF_COUNT_IF_IS_MISSING);
		}

		if ( (mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].was_true==0) && (mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL-1 ].is_true==1) ) {
			mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].is_true = 1;
			mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].was_true = 1;
		}
		else {
			mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].is_true = 0;
		}

		return (MHT_OK);
	}


	/* Go down in the if stack */
	else if (QUICK_STRCMP(if_directive,"endif")==0) {
		/* An #endif without an #if before is not allowed! */
		if (mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].there_is_an_if==0) {
			return (MHT_ERR_IF_COUNT_IF_IS_MISSING);
		}

		mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].is_true = 0;
		mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].was_true = 0;
		mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].there_is_an_if = 0;
		DEC_IF_LEVEL;

		/* Oops, there was one #endif too much! */
		if (GET_IF_LEVEL<0) {
			return (MHT_ERR_IF_COUNT_TOO_MANY_ENDIF);
		}

		return (MHT_OK);
	}


	/* Set everything for "#if if_arg" or "#elif if_arg" */
	else if ( (if_arg!=(char*)NULL) && ((QUICK_STRCMP(if_directive,"if")==0) || (QUICK_STRCMP(if_directive,"elif")==0)) ) {
		strlwr(if_arg);

		if (QUICK_STRCMP(if_directive,"if")==0) {
			INC_IF_LEVEL;
			mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].there_is_an_if = 1;

			if (mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL-1 ].is_true==1) {
				if ( (QUICK_STRCMP(if_arg,"true")==0) || (QUICK_STRCMP(if_arg,"1")==0) ) {
					mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].is_true = 1;
					mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].was_true = 1;
				}
				else if ( (QUICK_STRCMP(if_arg,"false")==0) || ((QUICK_STRCMP(if_arg,"0")==0)) ) {
					mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].is_true = 0;
					mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].was_true = 0;
				}
				else {
					return (MHT_ERR_IF_COUNT_ARGUMENT_WRONG_ARG);
				}
			}
		}
		else if (QUICK_STRCMP(if_directive,"elif")==0) {
			/* An #elif without an #if before is not allowed! */
			if (mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].there_is_an_if==0) {
				return (MHT_ERR_IF_COUNT_IF_IS_MISSING);
			}

			if (mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL-1 ].is_true==1) {
				if ( (QUICK_STRCMP(if_arg,"true")==0) || (QUICK_STRCMP(if_arg,"1")==0) ) {
					if (mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].was_true==0) {
						mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].is_true = 1;
						mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].was_true = 1;
					}
					else if (mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].was_true==1) {
						mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].is_true = 0;
					}
				}
				else if ( (QUICK_STRCMP(if_arg,"false")==0) || (QUICK_STRCMP(if_arg,"0")==0) ) {
					mht.if_context[ GET_IF_CONTEXT ].if_stack[ GET_IF_LEVEL ].is_true = 0;
				}
				else {
					return (MHT_ERR_IF_COUNT_ARGUMENT_WRONG_ARG);
				}
			}
		}
	}
	else {
		/* if_arg was missing */
		return (MHT_ERR_IF_COUNT_IF_OR_ELIF_WITHOUT_ARG);
	}

	return (MHT_OK);
}


/*
	Set a MHT var.
*/
int mht_setvar( char *mhtvar, char *value ) {
	/* Check if a mhtvar and a value is specified */
	if (mhtvar==(char*)NULL || value==(char*)NULL) {
		return (MHT_ERR_SETVAR_PARAM_MISSING);
	}

	strlwr(mhtvar);
	strlwr(value);

	/* Switch converting German umlauts on/off */
	if (QUICK_STRCMP(mhtvar,"convumlauts")==0) {
		if ( (QUICK_STRCMP(value,"true")==0) || (QUICK_STRCMP(value,"1")==0) ) {
			mht.conv_umlauts = 1;
			return (MHT_OK);
		}
		else if ( (QUICK_STRCMP(value,"false")==0) || (QUICK_STRCMP(value,"0")==0) ) {
			mht.conv_umlauts = 0;
			return (MHT_OK);
		}
		else {
			return (MHT_ERR_SETVAR_PARAM_UNRECOGNIZED);
		}
	}

	/* Switch killing leading and ending white spaces, and also \n\r */
	else if (QUICK_STRCMP(mhtvar,"killspace")==0) {
		if ( (QUICK_STRCMP(value,"true")==0) || (QUICK_STRCMP(value,"1")==0) ) {
			mht.killspace = 1;
			return (MHT_OK);
		}
		else if ( (QUICK_STRCMP(value,"false")==0) || (QUICK_STRCMP(value,"0")==0) ) {
			mht.killspace = 0;
			return (MHT_OK);
		}
		else {
			return (MHT_ERR_SETVAR_PARAM_UNRECOGNIZED);
		}
	}

	/* Switch whether the expanded output should be written to the outpu stream(s) or not */
	else if (QUICK_STRCMP(mhtvar,"writeoutput")==0) {
		if ( (QUICK_STRCMP(value,"true")==0) || (QUICK_STRCMP(value,"1")==0) ) {
			mht.writeoutput = 1;
			return (MHT_OK);
		}
		else if ( (QUICK_STRCMP(value,"false")==0) || (QUICK_STRCMP(value,"0")==0) ) {
			mht.writeoutput = 0;
			return (MHT_OK);
		}
		else {
			return (MHT_ERR_SETVAR_PARAM_UNRECOGNIZED);
		}
	}

	return (MHT_ERR_SETVAR_VAR_UNRECOGNIZED);
}


/*
	Set MHT file I/O.
*/
int mht_setfile_io( char *action, char *type, char *fname ) {
	unsigned int
		type_exists = 0,
		i = 0;


	/* Check if a file action is specified. */
	if (action==(char*)NULL) {
		return (MHT_ERR_SETFILE_IO_OPERATION_MISSING);
	}

	/* If type is NULL, and action is close, then close all file handles */
	if (QUICK_STRCMP(action,"close")==0) {
		for (i=1;i<=mht.fhandles;i++) {
			if (mht.fhandle_fptr[i]!=(FILE*)NULL) {
				fclose(mht.fhandle_fptr[i]);
				mht.fhandle_fptr[i] = (FILE*)NULL;
			}
		}

		mht.write_to_file = 0;
		mht.active_fhandle = -1;

		return (MHT_OK);
	}

	/* Check whether we know the given type. */
	if (QUICK_STRCMP(type,"all")!=0) {
		type_exists = 0;
		for (i=0;i<=mht.fhandles;i++) {
			if (QUICK_STRCMP(mht.fhandle_type[i],type)==0) {
				type_exists = i;
			}
		}

		/*
			Check whether an undefined file type is
			used in a #file or #mhtfile open directive:
		*/
		if ((QUICK_STRCMP(action,"open")==0 || QUICK_STRCMP(action,"file")==0) && type_exists==0) {
			return (MHT_ERR_SETFILE_IO_UNKNOWN_FILETYPE);
		}
	}

	/* No, it's a new file type */
	if ( (type_exists==0) && (QUICK_STRCMP(type,"all")!=0) ) {
		if (QUICK_STRCMP(action,"type")==0) {
			/* Register the new type */
			mht.fhandles++;
			mht.fhandle_type[mht.fhandles] = strdup(type);
		}
		else {
			/* Unknow action in this context */
			return (MHT_ERR_SETFILE_IO_UNKNOWN_ACTION);
		}
	}
	else {
		if (QUICK_STRCMP(action,"open")==0) {
			/* Check if a filename was specified */
			if (fname==(char*)NULL) {
				return (MHT_ERR_SETFILE_IO_FNAME_MISSING);
			}

			/* Open a new file handle */
			mht_trim(fname);
			mht.fhandle_fptr[type_exists] = fopen(fname,"w");
			if (mht.fhandle_fptr[type_exists]==(FILE*)NULL) {
				/* Get the type of I/O error that occured while opening the file */
				switch (errno) {
					case -34:
						return (MHT_ERR_SETFILE_IO_DRIVE_FULL);
					case -44:
						return (MHT_ERR_SETFILE_IO_DRIVE_WRITE_PROTECTED);
					default:
						return (MHT_ERR_SETFILE_IO_DRIVE_GENERAL_IO_ERR);
				}
			}

			mht.write_to_file = 1;
			return (MHT_OK);
		}
		else if (QUICK_STRCMP(action,"file")==0) {
			/* Print to file handle(s) */
			if (QUICK_STRCMP(type,"all")==0) {
				/* Print to all file handles */
				mht.active_fhandle = 0;
			}
			else {
				/* Print to a specific file handle only */
				mht.active_fhandle = type_exists;
			}
		}
		else {
			/* Unknow action in this context */
			return (MHT_ERR_SETFILE_IO_UNKNOWN_ACTION);
		}
	}

	return (MHT_OK);
}

/*
	Expand all MHT macros in a string by recursion.
*/
char *mht_expand( char *input ) {
	int
		is_delayed_macro = 0,
		macro_found = 0,
		bracket = -1,
		macro_len = 0,
		macro_arg_count = 0;

	register unsigned int
		i = 0;

	char
		*str_ptr = (char*)NULL,
		*macro_start_ptr = (char*)NULL,
		*macro_end_ptr = (char*)NULL,
		tmp_macro[MAX_LEN],
		*expanded_ptr = (char*)NULL,
		expanded_macro[MAX_LEN],
		*macro_args[MAX_ARG_COUNT],
		*dummy_ptr = (char*)NULL;

	LINE_BUFFER *block = (LINE_BUFFER*)NULL;


	str_ptr = input;

	tmp_macro[0] = '\0';
	expanded_macro[0] = '\0';

	/* Return if we have nothing else than en empty string */
	if (input==(char*)NULL) {
		return ((char*)NULL);
	}

	for (i=0; i<MAX_ARG_COUNT; i++) {
		macro_args[i] = (char*)NULL;
	}

	do {
		is_delayed_macro = 0;

		/* Find the start of a MHT macro "<#..." */
		if( (macro_start_ptr=strstr(str_ptr,"<#"))!=(char*)NULL ) {
			macro_found = 1;
		}
		else {
			macro_found = 0;
			break;
		}

		/* Find the closing bracket "...>" of a macro */
		for (bracket=0,macro_len=0,macro_end_ptr=macro_start_ptr; *macro_end_ptr!='\0'; *macro_end_ptr++,macro_len++) {
			if (*macro_end_ptr=='<') bracket++;
			if (*macro_end_ptr=='>') bracket--;
			if (bracket==0) break;
		}

		if (bracket>0) {
			/*
				This is an error case: open macro "<#macro" found!
				Return the input string as is untouched...
			*/
			return (input);
		}

		*macro_end_ptr++;
		macro_len++;

		/* an error occurred: "<#macro" without closing bracket ">" */
		if (bracket!=0) {
			return ((char*)NULL);
		}


		/* Copy the macro without the brackets "<#" and ">" into tmp_macro */
		tmp_macro[0] = '\0';
		strncpy(tmp_macro,macro_start_ptr+2,macro_len-3);
		tmp_macro[macro_len-3] = '\0';

		/*
			Expand all inner macros in possible parameters and split it into
			its arguments
		*/
		mht_expand(tmp_macro);

		if (tmp_macro[0]=='#') {
			is_delayed_macro = 1;
			expanded_ptr = tmp_macro;
		}
		else {
			is_delayed_macro = 0;
		}

		macro_arg_count = strsplit(tmp_macro,macro_args,'|',MAX_ARG_COUNT);

		/* check whether it is a "standard" MHT macros */
		if (QUICK_STRCMP("ifequal",macro_args[0])==0) {
			mht_replace_unexpanded_params( macro_arg_count, macro_args );

			/* <#ifequal|str1|str2|TRUE|FALSE> */
			/* str1 AND str2 are NULL, <#null>, empty or undefined */
			if ( (macro_args[1]==(char*)NULL && macro_args[2]==(char*)NULL) || ((_str_len(macro_args[1]))==0 && (_str_len(macro_args[2])==0)) ) {
				if (macro_arg_count<=3) {
					expanded_ptr = (char*)NULL;
				}
				else {
					expanded_ptr = macro_args[3];
				}
			}

			/* str1 OR str2 are NULL, <#null>, empty or undefined */
			else if (macro_args[1]==(char*)NULL || macro_args[2]==(char*)NULL || (_str_len(macro_args[1]))==0 || (_str_len(macro_args[2])==0)) {
				if (macro_arg_count<=4) {
					expanded_ptr = (char*)NULL;
				}
				else {
					expanded_ptr = macro_args[4];
				}
			}

			/* str1==str2 */
			else if (QUICK_STRCMP(macro_args[1],macro_args[2])==0) {
				if (macro_arg_count<=3) {
					expanded_ptr = (char*)NULL;
				}
				else {
					expanded_ptr = macro_args[3];
				}
			}
			else {
				/* str1!=str2 */
				if (macro_arg_count<=4) {
					expanded_ptr = (char*)NULL;
				}
				else {
					expanded_ptr = macro_args[4];
				}
			}

			/*
				If expanded_ptr is here NULL, the macro would remain unexpanded
				in the output string. This shouldn't be the case, #ifequal should
				alsways be expanded at least to an empty string!
			*/
			if (expanded_ptr==(char*)NULL) {
				expanded_macro[0] = '\0';
				expanded_ptr = expanded_macro;
			}
		}
		else if (QUICK_STRCMP("ifdef",macro_args[0])==0) {
			/* <#ifdef|str1|TRUE|FALSE> */

			/* str1 is NULL or empty (bit stupid) */
			if (macro_args[1]==(char*)NULL) {
				expanded_ptr = (char*)NULL;
			}
			else {
				mht_search_macro(macro_args[1],&dummy_ptr);

				/*
					If we did not find a definition for that macro,
					it might be a block parameter!
				*/
				if (dummy_ptr==(char*)NULL) {
					mht_search_block_param(macro_args[1],&dummy_ptr);
				}

				if (dummy_ptr!=(char*)NULL) {
					/* str1 IS defined as a macro */
					if (macro_arg_count<=2) {
						expanded_ptr = (char*)NULL;
					}
					else {
						expanded_ptr = macro_args[2];
					}
				}
				else {
					/* str1 is NOT defined as a macro */
					if (macro_arg_count<=3) {
						expanded_ptr = (char*)NULL;
					}
					else {
						expanded_ptr = macro_args[3];
					}
				}
			}

			/*
				If expanded_ptr is here NULL, the macro would remain unexpanded
				in the output string. This shouldn't be the case, #ifdef should
				alsways be expanded at least to an empty string!
			*/
			if (expanded_ptr==(char*)NULL) {
				expanded_macro[0] = '\0';
				expanded_ptr = expanded_macro;
			}
		}
		else if (QUICK_STRCMP("isin",macro_args[0])==0) {
			/* <#isin|str1|str2|TRUE|FALSE> */

			/* str1 AND str2 are NULL, <#null>, empty or undefined */
			if (macro_args[1]==(char*)NULL && macro_args[2]==(char*)NULL) {
				if (macro_arg_count<=3) {
					expanded_ptr = (char*)NULL;
				}
				else {
					expanded_ptr = macro_args[3];
				}
			}

			/* str1 OR str2 are NULL, <#null>, empty or undefined */
			else if (macro_args[1]==(char*)NULL || macro_args[2]==(char*)NULL) {
				if (macro_arg_count<=4) {
					expanded_ptr = (char*)NULL;
				}
				else {
					expanded_ptr = macro_args[4];
				}
			}

			/* str1 IS in str2 */
			else if (strstr(macro_args[2],macro_args[1])!=(char*)NULL) {
				if (macro_arg_count<=3) {
					expanded_ptr = (char*)NULL;
				}
				else {
					expanded_ptr = macro_args[3];
				}
			}
			else {
				/* str1 is NOT in str2 */
				if (macro_arg_count<=4) {
					expanded_ptr = (char*)NULL;
				}
				else {
					expanded_ptr = macro_args[4];
				}
			}

			/*
				If expanded_ptr is here NULL, the macro would remain unexpanded
				in the output string. This shouldn't be the case, #isin should
				alsways be expanded at least to an empty string!
			*/
			if (expanded_ptr==(char*)NULL) {
				expanded_macro[0] = '\0';
				expanded_ptr = expanded_macro;
			}
		}
		else if (QUICK_STRCMP("ifblock",macro_args[0])==0) {
			/* <#checkblock|str1|TRUE|FALSE> */

			/* str1 is NULL or empty (bit stupid) */
			if (macro_args[1]==(char*)NULL) {
				/* str1 is NOT a existing block */
				if (macro_arg_count<=3) {
					expanded_ptr = (char*)NULL;
				}
				else {
					expanded_ptr = macro_args[3];
				}
			}
			else {
				block = mht_search_block(macro_args[1]);

				if (block!=(LINE_BUFFER*)NULL) {
					/* str1 IS a existing block */
					if (macro_arg_count<=2) {
						expanded_ptr = (char*)NULL;
					}
					else {
						expanded_ptr = macro_args[2];
					}
				}
				else {
					/* str1 is NOT a existing block */
					if (macro_arg_count<=3) {
						expanded_ptr = (char*)NULL;
					}
					else {
						expanded_ptr = macro_args[3];
					}
				}
			}

			/*
				If expanded_ptr is here NULL, the macro would remain unexpanded
				in the output string. This shouldn't be the case, #checkblock
				should alsways be expanded at least to an empty string!
			*/
			if (expanded_ptr==(char*)NULL) {
				expanded_macro[0] = '\0';
				expanded_ptr = expanded_macro;
			}
		}
		else if (is_delayed_macro==0) {
			/* It should be a macro defined via #def by the user... */
			if (mht_search_macro(macro_args[0],&expanded_ptr)==1) {
				/* Get the definition for the macro... */
				sprintf(expanded_macro,"%s",expanded_ptr);

				/*
					...if the macro has any params, replace them by their values...
					("blabla <#.%1> blabla <#.%2> ...")
				*/
				if (macro_arg_count>1) {
					expanded_ptr = mht_replace_macro_params( macro_arg_count, macro_args, expanded_macro );
				}

				/*
					...expand the new string again...
					("blabla <#macro1> blabla <#macro2> ...")
				*/
				expanded_ptr = mht_expand(expanded_macro);
			}
		}


		/*
			If we still did not find a definition for that macro, it might be
			a block parameter. So go and check all current block parameters...
		*/
		if (expanded_ptr==(char*)NULL) {
			if ((mht_search_block_param(macro_args[0],&expanded_ptr))==1) {
				mht_expand(expanded_ptr);
			}
		}

		/* Everything "expandable" was expanded...! */

		if (expanded_ptr!=(char*)NULL) {
			/* If the macro is defined, replace it by its definition */
			if (is_delayed_macro==0) {
				strinsert(str_ptr,expanded_ptr,macro_len,macro_start_ptr);
			}
			else {
				sprintf(expanded_macro,"<%s>",expanded_ptr);
				strinsert(str_ptr,expanded_macro,macro_len,macro_start_ptr);
			}
			str_ptr = macro_start_ptr+(int)_str_len(expanded_ptr);
		}
		else {
			/*
				The macro is not defined, so leave the macro unexpanded as "<#...>"
				We really do have to insert the "unexpanded" macro back again,
				because even this unexpanded macro might be the result of one or
				more expanded inner macros...!
			*/
			strinsert(str_ptr,tmp_macro,macro_len-3,macro_start_ptr+2);
			str_ptr = macro_end_ptr;
		}


		/* Reset the macro arguments, every macro has its own arguments! */
		for (i=0; i<MAX_ARG_COUNT; i++) {
			macro_args[i] = (char*)NULL;
		}


	} while (macro_found==1);

	return (input);
}


/*
	The following problem:
	somebody wants to check via ifequal, whether a macro is defined or not,
	empty or <#null>.
	Any unexpanded parameters in ifequal have to be treated as <#null> or
	empty strings internally, so that the string comparison in ifequal still
	works correct even with unexpanded parameters as in the example above.
	This function replaces therefor each unexpanded parameter of ifequal
	with an empüty string- et voila!
*/
void mht_replace_unexpanded_params( int macro_arg_count, char **macro_args ) {
	int
		bracket = 0,
		param_len = 0,
		i = 0;
	char
		*str_ptr = (char*)NULL,
		*param_start_ptr = (char*)NULL,
		*param_end_ptr = (char*)NULL;

	for (i=0; i<macro_arg_count; i++) {
		str_ptr = macro_args[i];

		if (str_ptr==(char*)NULL) continue;

		if( (param_start_ptr=strstr(str_ptr,"<#."))!=(char*)NULL ) {
			/* Find the closing bracket "...>" of a macro */
			for (bracket=0,param_len=0,param_end_ptr=param_start_ptr; param_end_ptr!=(char*)NULL; *param_end_ptr++,param_len++) {
				if (*param_end_ptr=='<') bracket++;
				if (*param_end_ptr=='>') bracket--;
				if (bracket==0) break;
			}
			*param_end_ptr++;
			param_len++;

			/* Remove the unexpanded parameter <#.%n> */
			strinsert(str_ptr,"",param_len,param_start_ptr);
			str_ptr = param_start_ptr+param_len;
		}
	}
}


/*
	Expand all parameters of a macro.
*/
char *mht_replace_macro_params( int macro_arg_count, char **macro_args, char *expanded_macro ) {
	int
		param_found = 0,
		bracket = 0,
		param_len = 0,
		param = 0;

	char
		*str_ptr = (char*)NULL,
		*param_start_ptr = (char*)NULL,
		*param_end_ptr = (char*)NULL,
		tmp_param[MACRO_LEN];


	str_ptr = expanded_macro;
	tmp_param[0] = '\0';

	if (expanded_macro==(char*)NULL) {
		return ((char*)NULL);
	}


	do {
		/* Find the start of a MHT macro parameter "<#." */
		if( (param_start_ptr=strstr(str_ptr,"<#."))!=(char*)NULL ) {
			param_found = 1;
		}
		else {
			param_found = 0;
			break;
		}

		/* Find the closing bracket "...>" of a macro */
		for (bracket=0,param_len=0,param_end_ptr=param_start_ptr; param_end_ptr!=(char*)NULL; *param_end_ptr++,param_len++) {
			if (*param_end_ptr=='<') bracket++;
			if (*param_end_ptr=='>') bracket--;
			if (bracket==0) break;
		}
		*param_end_ptr++;
		param_len++;

		/* an error occurred: "<#macro" without closing bracket ">" */
		if (bracket!=0) {
			return ((char*)NULL);
		}

		/* Copy the parameter without the brackets "<#.%" and ">" into tmp_param */
		tmp_param[0] = '\0';
		strncpy(tmp_param,param_start_ptr+4,param_len-5);
		tmp_param[param_len-5] = '\0';
		param = atoi(tmp_param);

		/* If we have that macro parameter, go and insert it! */
		if ( (param<=macro_arg_count) && (macro_args[param]!=(char*)NULL) ) {
			strinsert(str_ptr,macro_args[param],param_len,param_start_ptr);
			str_ptr = param_start_ptr+(int)_str_len(macro_args[param]);
		}
		else {
			/* Leave the undefined parameter inside the string as is. */
			str_ptr = param_end_ptr;
		}
	} while (param_found==1);

	return (expanded_macro);
}


/*
	Replace any german umlauts by its HTML replacement.
*/
char *mht_replace_umlauts( char *str ) {
	register unsigned int
		i = 0;

	char
		*search_ptr = (char*)NULL,
		*replace_char = (char*)NULL;


	for (i=0;i<MHT_UMLAUT_COUNT;i++) {
		search_ptr = str;
		while ((replace_char=strchr(search_ptr,mht_html_replace_chars[i]))!=(char)NULL) {
			strinsert(str,mht_html_umlauts[i],1,replace_char);
			search_ptr = replace_char + _str_len(mht_html_umlauts[i]);
		}
	}

	return (str);
}


/*
	Cut off all leading and trailing white spaces and new lines.
*/
char *mht_killspace( char *line ) {

	int
		len = 0,
		end = 0;

	char *str = (char*)NULL;


	str = line;

	if (str!=(char*)NULL) {
		/* Increment str till an alnum char is found */
		while(isspace(str[0])) {
			str++;
		}

		/* Cut off trailing spaces */
		end = _str_len(str)-1;
		while ((end>0) && (isspace(str[end]))) {
			str[end] = '\0';
			end--;
		}

		/* Copy the remaining str to the front of the line */
		len = _str_len(str);
		memmove(line,str,len);
		line[len] = '\0';
	}

	if (_str_len(str)>0) {
		strcat(line," ");
	}

	return(line);
}


/*
	Works equivalent to mht_killspace in the sense that all beginning
	and trailing whitespaces are removed, but without a ending whitespace.
*/
char *mht_trim( char *line ) {

	int
		len = 0,
		end = 0;

	char *str = (char*)NULL;


	str = line;

	if (str!=(char*)NULL) {
		/* Increment str till an alnum char is found */
		while(isspace(str[0])) {
			str++;
		}

		/* Cut off trailing spaces */
		end = _str_len(str)-1;
		while ((end>0) && (isspace(str[end]))) {
			str[end] = '\0';
			end--;
		}

		/* Copy the remaining str to the front of the line */
		len = _str_len(str);
		memmove(line,str,len);
		line[len] = '\0';
	}

	return(line);
}


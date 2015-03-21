/* Copyright (C) 2003 Thomas Weckert */

/* Initialize MHT data structures */
void mht_init(void);

/* Destroy MHT data structure */
void mht_exit(void);

/* Register a new macro */
int mht_register_macro( char *name, char *definition );

/* Search for a macro and return its definition */
int mht_search_macro( char *name, char **result );

/* "Un-"register a macro */
int mht_undef_macro( char *name );

/* "Un-"register a block */
int mht_undef_block( char *block_param );

/* Open, read and process a text file */
int mht_quickopen( FILE *out, char *fname );

/* Process a MHT block from a previously read text file */
int mht_process( FILE *out, char *block_name );

/* Process a MHT block with parameters from a previously read text file */
int mht_process_with_params( FILE *out, char *blockname, char **block_params, int block_param_count );

/* Register a environment variable as a MHT macro */
int mht_register_env( char *env_var, char *mht_macro );

/* Expand all MHT macros in a string by recursion. */
char *mht_expand( char *input );

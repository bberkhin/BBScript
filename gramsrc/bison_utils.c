#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "config.h"
#include "grammartypes.h"
#include "value.h"
#include "grammary.h"
#include "interpreter.h"
#include "utils.h"
#include "variable.h"
#include "bison_utils.h"


#define FREEONEXIT 1

void* yy_scan_string(const char*);
void clean_buffer(void *buf);


static int syntaxerrors = 0;
int runtimewarnings = 0;		/* Nr of calls to script's warning() function or internal rtwarning() */
int runtimeerrors = 0;		/* Nr of calls to script's error() function */
double *global_offs[MAXAXES];	/* Offset set at command-line */
double relative_offs[MAXAXES];	/* Offset set with relocate() function */
double *global_pos[MAXAXES];	/* Current turtle position */
int naxes = 6;			/* Default enable 6 axes */
int cl_decimals = 8;		/* Number of significant decimals to print in output */
int cl_pedantic = 0;		/* Enable pendatic warnings when set */

int yyparse(void);

void print_gram_error(const char *err_pfx, const char *fmt, va_list va);

static int errorprint(const node_t *n, const char *fmt, const char *pfx, va_list va)
{
	int l = linenr;
	int c = charnr;
	if(n) {
		l = n->linenr;
		c = n->charnr;
	}
    int size = snprintf(0, 0, "%s:%d:%d: %s: ", n ? n->filename : filename, l, c, pfx);
	char *err_pfx = malloc(size+1);
	snprintf(err_pfx, size, "%s:%d:%d: %s: ", n ? n->filename : filename, l, c, pfx);
	print_gram_error(err_pfx, fmt, va);	
	free(err_pfx);
	return 0;
}


/*
static int errorprint(const node_t *n, const char *fmt, const char *pfx, va_list va)
{
	int l = linenr;
	int c = charnr;
	if(n) {
		l = n->linenr;
		c = n->charnr;
	}
	fprintf(stderr, "%s:%d:%d: %s: ", n ? n->filename : filename, l, c, pfx);
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\n");
	return 0;
}
*/
int yyerror(const char *fmt, ...)
{
	syntaxerrors++;
	/* The parser always emits "syntax error" before going into error-recovery */
	if(!strcmp(fmt, "syntax error"))
		return 0;
	va_list va;
	va_start(va, fmt);
	errorprint(NULL, fmt, "error", va);
	va_end(va);
	return 0;
}

int yyfatal(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	errorprint(NULL, fmt, "fatal", va);
	va_end(va);
	//exit(1);
}

int rtinternal(const node_t *n, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	errorprint(n, fmt, "internal error", va);
	va_end(va);
	//exit(1);
    return 1;
}

int rterror(const node_t *n, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	errorprint(n, fmt, "error", va);
	va_end(va);
	//exit(1);
    return 1;
}

int rtwarning(const node_t *n, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	errorprint(n, fmt, "warning", va);
	va_end(va);
	runtimewarnings++;
	return 0;
}

int rtdeprecated(const node_t *n, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	errorprint(n, fmt, "deprecated", va);
	va_end(va);
	return 0;
}

int rtinfo(const node_t *n, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	errorprint(n, fmt, "info", va);
	va_end(va);
	return 0;
}

//only one include path: current directory
const char *includepath_get(int idx)
{
    static char *inc_curpath = ".";
	if(idx < 0)
		rtinternal(NULL, "Negative index in index search path retrieval");
	if(idx >= 1)
		return NULL;
	return inc_curpath;
}

void create_global_var()
{
    value_t *v,*w;
    double offsets[MAXAXES] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
	
	/* Setup reference to global offset and position track variable */
	v = value_new(VAL_VECTOR);
	for(int i = 0; i < naxes; i++) {
		w = value_new_flt(offsets[i], UNIT_RAD);
		value_vector_add(v, w);
		global_offs[i] = &w->d;
		relative_offs[i] = 0.0;		/* Also init relocate() position to zero */
	}
	variable_set(L"__global_offset", v);

	v = value_new(VAL_VECTOR);
	w = value_new_flt(0.0, UNIT_RAD); value_vector_add(v, w); global_pos[0] = &w->d;
	w = value_new_flt(0.0, UNIT_RAD); value_vector_add(v, w); global_pos[1] = &w->d;
	w = value_new_flt(0.0, UNIT_RAD); value_vector_add(v, w); global_pos[2] = &w->d;
	w = value_new_flt(0.0, UNIT_RAD); value_vector_add(v, w); global_pos[3] = &w->d;
	w = value_new_flt(0.0, UNIT_RAD); value_vector_add(v, w); global_pos[4] = &w->d;
	w = value_new_flt(0.0, UNIT_RAD); value_vector_add(v, w); global_pos[5] = &w->d;
    w = value_new_flt(0.0, UNIT_RAD); value_vector_add(v, w); global_pos[6] = &w->d;
	w = value_new_flt(0.0, UNIT_RAD); value_vector_add(v, w); global_pos[7] = &w->d;
	w = value_new_flt(0.0, UNIT_RAD); value_vector_add(v, w); global_pos[8] = &w->d;
	
	variable_set(L"__global_position", v);
}


int parser_intit()
{    
    create_global_var();
    return 0;
}
int parser_parse(const char *input_buf)
{
    syntaxerrors = 0;
    void* buf = yy_scan_string(input_buf);  // подключаем буфер к Flex
    if ( yyparse() )
        return -1;
    return syntaxerrors;
}   

int parser_execute()
{
    int retval = 0;
    value_t *v;
    if((v = execute(scripthead, NULL))) {
        retval = value_to_int(v);
        value_delete(v);
    }
    return retval;
}

void parser_clean()
{
	variable_purge();
	value_purge();
	node_delete(scripthead);
	parser_cleanup();
	lexer_cleanup();
	utils_cleanup();
	interpreter_cleanup();
}

int test_main() 
{
    char input[256];// = "2+2";
    create_global_var();
    while(true) {       
        syntaxerrors = 0;
        value_t *v;
        printf( "\nEnter new expression: ");
        fgets(input, sizeof(input), stdin  );
        printf("You entered: %s\n", input);
        void* buf = yy_scan_string(input);  // подключаем буфер к Flex
        if(yyparse())
    		yyerror("Syntax error");
        if ( syntaxerrors )
            continue;
        
        if((v = execute(scripthead, NULL))) {
	    	int retval = value_to_int(v);
		    value_delete(v);
	    }
        //clean_buffer(buf);        
    }
#ifdef FREEONEXIT
	variable_purge();
	value_purge();
	node_delete(scripthead);
	parser_cleanup();
	lexer_cleanup();
	utils_cleanup();
	interpreter_cleanup();
#endif
    return 0;
}

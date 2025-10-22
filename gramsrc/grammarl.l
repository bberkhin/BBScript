/*
 * G-code meta compiler
 *
 * Copyright (C) 2013  B. Stultiens
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
%option stack
%option noinput nounput noyy_top_state noyywrap
%option 8bit never-interactive

/* Exclusive comment eating */
%x comment
/* Exclusive string collection */
%x string

%{
#include "config.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#else
#define YY_NO_UNISTD_H
#endif
#include <ctype.h>
#include <assert.h>

#include "grammartypes.h"
#include "grammary.h"
#include "utils.h"

 
/* This is here to cast the result to ssize_t so that the "< 0" comparison works */
/* The rest is a copy of the generated scanner code */

int linenr = 1;
int charnr = 1;
int prevlinenr = 1;
int prevcharnr = 1;
const char *filename;

typedef struct __incstack_t {
	YY_BUFFER_STATE	bufferstate;	/* Previous flex context */
	FILE		*fp;		/* Previous input file */
	int		linenr;		/* Position of the include() statement */
	int		charnr;
	int		prevlinenr;
	int		prevcharnr;
	const char	*filename;	/* Name of file we were handling */
} incstack_t;

static incstack_t	*incstack;
static int		nincstack;
static int		naincstack;

static void str_init(void);
static void str_addc(char c);
static wchar_t *str_get(void);
static wchar_t *utf8_to_ucs32(const char *s, int len);

/* Always update the current character position within a line */
#define YY_USER_ACTION	do { prevlinenr = linenr; prevcharnr = charnr; charnr += yyleng; } while(0);

%}

/* Some shortcut definitions */
ws	[ \f\t\r]
cident	[a-zA-Z_][0-9a-zA-Z_]*
n	[0-9]
h	[0-9a-fA-F]

%%

<INITIAL>"/*"		yy_push_state(comment); /* Skip block comments */
<comment>[^*\n]+	; /* Skip junk */
<comment>"*"+[^/\n]	; /* Skip * */
<comment>\n		linenr++; charnr = 1;
<comment>"*"+"/"	yy_pop_state();
<comment>"*"		; /* Skip * */

<INITIAL>"//".*		; /* Skip line comments */

<INITIAL>\"		yy_push_state(string); str_init();
<string>\"{ws}+	|
<string>\"		{
				yy_pop_state();
				yylval.str = str_get();
				return STRING;
			}
<string>\\[0-7]{1,3}	{ /* octal escape sequence */
				int result;
				result = strtol(yytext+1, 0, 8);
				if(result > 0xff)
					yyerror("Character constant out of range");
				if(!result)
					yyerror("Embedded '\\0' character not allowed");
				str_addc((char)result);
			}
<string>\\x[0-9a-fA-F]{2} { /* hex escape sequence */
				int result;
				result = strtol(yytext+2, 0, 16);
				if(!result)
					yyerror("Embedded '\\0' character not allowed");
				str_addc((char)result);
			}
<string>\\x[0-9a-fA-F]	{  yyerror("Invalid hex escape sequence '%s'", yytext); }

<string>\\[0-9]+	yyerror("Bad escape sequence");
<string>\\\n{ws}*	linenr++; charnr = 1; /* backslash at EOL continues string after leading whitespace on next line */
<string>\\a		str_addc('\a');
<string>\\b		str_addc('\b');
<string>\\f		str_addc('\f');
<string>\\n		str_addc('\n');
<string>\\r		str_addc('\r');
<string>\\t		str_addc('\t');
<string>\\v		str_addc('\v');
<string>\\\"		str_addc('"');
<string>\\.		str_addc(yytext[1]);
<string>\\\r\n		str_addc(yytext[2]); linenr++; charnr = 1;
<string>[^\\\n\"]+	{
				char *yptr = yytext;
				while(*yptr)
					str_addc(*yptr++);
			}
<string>\"{ws}+\"	;		/* "bla" "bla" -> "blabla" */
<string>\n		yyerror("Unterminated string");

<INITIAL>{n}+	{
			yylval.i = strtol(yytext, NULL, 10);
			return NUMBER;
		}
<INITIAL>0x{h}+	{
			yylval.i = strtol(yytext, NULL, 16);
			return NUMBER;
		}
<INITIAL>0x[^0-9a-fA-F\n]*	yyerror("Bad hexadecimal number '%s'", yytext);

<INITIAL>(({n}+\.{n}*)|({n}*\.{n}+)|({n}+))([eE][+-]?{n}+)?	{
			yylval.d = strtod(yytext, NULL);
			return FLOATV;
		}
<INITIAL>(({n}+\.{n}*)|({n}*\.{n}+)|({n}+))([eE][+-]?)?	{
			yyerror("Bad floating point number '%s', missing exponent magnitude", yytext);
		}

<INITIAL>break		return BREAK;
<INITIAL>continue	return CONTINUE;
<INITIAL>const		return CONST;
<INITIAL>do		return DO;
<INITIAL>deg		return DEG;
<INITIAL>elif		return ELIF;
<INITIAL>else		return ELSE;
<INITIAL>foreach	return FOREACH;
<INITIAL>for		return FOR;
<INITIAL>function	return FUNCTION;
<INITIAL>if		return IF;
<INITIAL>in		return IN;
<INITIAL>include	return INCLUDE;
<INITIAL>local		return LOCAL;
<INITIAL>mil		return MIL;
<INITIAL>mm		return MM;
<INITIAL>rad		return RAD;
<INITIAL>repeat		return REPEAT;
<INITIAL>return		return RETURN;
<INITIAL>while		return WHILE;

<INITIAL>{cident}	{
			yylval.str = utf8_to_ucs32(yytext, yyleng);
			return IDENT;
		}

<INITIAL>"+|"		return ADDOR;
<INITIAL>"++"		return INC;
<INITIAL>"-|"		return SUBOR;
<INITIAL>"--"		return DEC;
<INITIAL>"||"		return LOR;
<INITIAL>"&&"		return LAND;
<INITIAL>"=="		return EQ;
<INITIAL>"!="		return NE;
<INITIAL>"<="		return LE;
<INITIAL>">="		return GE;
<INITIAL>"+="		return ADDASSIGN;
<INITIAL>"+|="		return ADDORASSIGN;
<INITIAL>"-="		return SUBASSIGN;
<INITIAL>"-|="		return SUBORASSIGN;
<INITIAL>"*="		return MULASSIGN;
<INITIAL>"/="		return DIVASSIGN;
<INITIAL>"%="		return MODASSIGN;
<INITIAL>"<<="		return SHLASSIGN;
<INITIAL>">>="		return SHRASSIGN;
<INITIAL>"|="		return BORASSIGN;
<INITIAL>"&="		return BANDASSIGN;
<INITIAL>"^="		return BXORASSIGN;
<INITIAL>"<<"		return SHL;
<INITIAL>">>"		return SHR;
<INITIAL>"<"		return LT;
<INITIAL>">"		return GT;
<INITIAL>"{"		return TOPEN;
<INITIAL>"}"		return TCLOSE;

<INITIAL>[=,;[\]()+*/!%&?:|~^.-]	return *yytext;

<INITIAL>{ws}*		; /* Skip whitespace */

<INITIAL>\n		linenr++; charnr = 1;
<*>.|\n			{
				/* Catch all rule to find any unmatched text */
				if(*yytext == '\n') {
					linenr++;
					charnr = 1;
				}
				yyerror("Illegal character in text '%c' (0x%02x)", isprint((unsigned char)*yytext) ? *yytext : '.', *yytext);
				/*yyfatal("internal: Unmatched text '%c' (0x%02x) YY_START=%d", isprint((unsigned char)*yytext) ? *yytext : '.', *yytext, YY_START);*/
			}
<<EOF>>			{
				/* Pop the include stack */
				YY_BUFFER_STATE bs = YY_CURRENT_BUFFER;

				if(YY_START != INITIAL)
					yyfatal("Unexpected EOF");

				if(nincstack) {
					/* Freeing the filename invalidates the node_t structure reference! */
					/*free((void *)filename);*/
					nincstack--;
					if(yyin)
						fclose(yyin);
					yy_switch_to_buffer(incstack[nincstack].bufferstate);
					linenr = incstack[nincstack].linenr;
					charnr = incstack[nincstack].charnr;
					prevlinenr = incstack[nincstack].prevlinenr;
					prevcharnr = incstack[nincstack].prevcharnr;
					filename = incstack[nincstack].filename;
					yy_delete_buffer(bs);
				} else {
					fclose(yyin);
					yylex_destroy();
					yyterminate();
				}
			}
%%

static char *str;
static int nstr;
static int nastr;

static void str_init(void)
{
	if(!str) {
		nastr = 32;
		str = calloc(nastr, sizeof(*str));
	}
	nstr = 0;
}

static void str_addc(char c)
{
	if(nstr >= nastr) {
		str = realloc(str, 2*nastr*sizeof(*str));
		assert(str != NULL);
		nastr *= 2;
	}
	str[nstr] = c;
	nstr++;
}

static wchar_t *str_get(void)
{
	wchar_t *s;
	assert(str != NULL);
	s = utf8_to_ucs32(str, nstr);
	nstr = 0;
	return s;
}

/*
 * UTF-8 to UCS-32 conversion
 *
 * It includes 1, 2, 3, 4, 5 and 6 byte sequences. The 5 and 6 byte sequences
 * are deprecated by RFC 3629, but they are possible encodings for specials if
 * we ever wanted to (we are not doing the internet here).
 */
static wchar_t *utf8_to_ucs32(const char *s, int len)
{
	int ns = len;
	int nucs = 0;
	int naucs = 0;
	wchar_t *ucs = NULL;

	assert(s != NULL);

	while(len > 0) {
		uint32_t val = 0;

		if((*s & 0xff) <= 0x7f) {
			/* 1-byte sequence */
			val = *s & 0x7f;
			len--;
			s++;
		} else if((*s & 0xfe) == 0xfc) {
			/* 6-byte sequence */
			if(len < 6) {
				yyerror("Invalid 6-byte UTF-8 sequence at %d/%d, missing bytes", ns-len, nucs);
				goto errout;
			}
			if((s[1] & 0xc0) != 0x80) {
				yyerror("Invalid 6-byte UTF-8 sequence at %d/%d, invalid second byte", ns-len, nucs);
				goto errout;
			}
			if((s[2] & 0xc0) != 0x80) {
				yyerror("Invalid 6-byte UTF-8 sequence at %d/%d, invalid third byte", ns-len, nucs);
				goto errout;
			}
			if((s[3] & 0xc0) != 0x80) {
				yyerror("Invalid 6-byte UTF-8 sequence at %d/%d, invalid fourth byte", ns-len, nucs);
				goto errout;
			}
			if((s[4] & 0xc0) != 0x80) {
				yyerror("Invalid 6-byte UTF-8 sequence at %d/%d, invalid fifth byte", ns-len, nucs);
				goto errout;
			}
			if((s[5] & 0xc0) != 0x80) {
				yyerror("Invalid 6-byte UTF-8 sequence at %d/%d, invalid sixth byte", ns-len, nucs);
				goto errout;
			}
			val = ((s[0] & 0x01) << 30) | ((s[1] & 0x3f) << 24) | ((s[2] & 0x3f) << 18) | ((s[3] & 0x3f) << 12) | ((s[4] & 0x3f) << 6) | (s[5] & 0x3f);
			if(val < 0x4000000)
				rtwarning(NULL, "Degenerate 6-byte UTF-8 encoding at %d/%d", ns-len, nucs);
			len -= 6;
			s += 6;
		} else if((*s & 0xfc) == 0xf8) {
			/* 5-byte sequence */
			if(len < 5) {
				yyerror("Invalid 5-byte UTF-8 sequence at %d/%d, missing bytes", ns-len, nucs);
				goto errout;
			}
			if((s[1] & 0xc0) != 0x80) {
				yyerror("Invalid 5-byte UTF-8 sequence at %d/%d, invalid second byte", ns-len, nucs);
				goto errout;
			}
			if((s[2] & 0xc0) != 0x80) {
				yyerror("Invalid 5-byte UTF-8 sequence at %d/%d, invalid third byte", ns-len, nucs);
				goto errout;
			}
			if((s[3] & 0xc0) != 0x80) {
				yyerror("Invalid 5-byte UTF-8 sequence at %d/%d, invalid fourth byte", ns-len, nucs);
				goto errout;
			}
			if((s[4] & 0xc0) != 0x80) {
				yyerror("Invalid 5-byte UTF-8 sequence at %d/%d, invalid fifth byte", ns-len, nucs);
				goto errout;
			}
			val = ((s[0] & 0x03) << 24) | ((s[1] & 0x3f) << 18) | ((s[2] & 0x3f) << 12) | ((s[3] & 0x3f) << 6) | (s[4] & 0x3f);
			if(val < 0x200000)
				rtwarning(NULL, "Degenerate 5-byte UTF-8 encoding at %d/%d", ns-len, nucs);
			len -= 5;
			s += 5;
		} else if((*s & 0xf8) == 0xf0) {
			/* 4-byte sequence */
			if(len < 4) {
				yyerror("Invalid 4-byte UTF-8 sequence at %d/%d, missing bytes", ns-len, nucs);
				goto errout;
			}
			if((s[1] & 0xc0) != 0x80) {
				yyerror("Invalid 4-byte UTF-8 sequence at %d/%d, invalid second byte", ns-len, nucs);
				goto errout;
			}
			if((s[2] & 0xc0) != 0x80) {
				yyerror("Invalid 4-byte UTF-8 sequence at %d/%d, invalid third byte", ns-len, nucs);
				goto errout;
			}
			if((s[3] & 0xc0) != 0x80) {
				yyerror("Invalid 4-byte UTF-8 sequence at %d/%d, invalid fourth byte", ns-len, nucs);
				goto errout;
			}
			val = ((s[0] & 0x07) << 18) | ((s[1] & 0x3f) << 12) | ((s[2] & 0x3f) << 6) | (s[3] & 0x3f);
			if(val < 0x10000)
				rtwarning(NULL, "Degenerate 4-byte UTF-8 encoding at %d/%d", ns-len, nucs);
			len -= 4;
			s += 4;
		} else if((*s & 0xf0) == 0xe0) {
			/* 3-byte sequence */
			if(len < 3) {
				yyerror("Invalid 3-byte UTF-8 sequence at %d/%d, missing bytes", ns-len, nucs);
				goto errout;
			}
			if((s[1] & 0xc0) != 0x80) {
				yyerror("Invalid 3-byte UTF-8 sequence at %d/%d, invalid second byte", ns-len, nucs);
				goto errout;
			}
			if((s[2] & 0xc0) != 0x80) {
				yyerror("Invalid 3-byte UTF-8 sequence at %d/%d, invalid third byte", ns-len, nucs);
				goto errout;
			}
			val = ((s[0] & 0x0f) << 12) | ((s[1] & 0x3f) << 6) | (s[2] & 0x3f);
			if(val < 0x800)
				rtwarning(NULL, "Degenerate 3-byte UTF-8 encoding at %d/%d", ns-len, nucs);
			len -= 3;
			s += 3;
		} else if((*s & 0xe0) == 0xc0) {
			/* 2-byte sequence */
			if(len < 2) {
				yyerror("Invalid 2-byte UTF-8 sequence at %d/%d, missing bytes", ns-len, nucs);
				goto errout;
			}
			if((s[1] & 0xc0) != 0x80) {
				yyerror("Invalid 2-byte UTF-8 sequence at %d/%d, invalid second byte", ns-len, nucs);
				goto errout;
			}
			val = ((s[0] & 0x1f) << 6) | (s[1] & 0x3f);
			if(val < 0x80)
				rtwarning(NULL, "Degenerate 2-byte UTF-8 encoding at %d/%d", ns-len, nucs);
			len -= 2;
			s += 2;
		} else {
			yyerror("Invalid UTF-8 character at %d/%d, out-of-sequence byte", ns-len, nucs);
			goto errout;
		}

		if(val) {
			if(sizeof(wchar_t) < sizeof(val) && val > 0xffff) {
				rtwarning(NULL, "wchar_t too small to hold character U+%08x, replacing with '?'", val);
				val = L'?';
			}
			testalloc((void **)&ucs, nucs, &naucs, sizeof(ucs[0]));
			ucs[nucs++] = (wchar_t)val;
		} else
			rtwarning(NULL, "Skipping embedded UTF-8 NUL embedding %d/%d", ns-len, nucs);
	}

errout:
	testalloc((void **)&ucs, nucs, &naucs, sizeof(ucs[0]));
	ucs[nucs] = 0;	/* Implicit zero terminate */

	return ucs;
}

void define_setup(char *buf, int bufsize, const char *name)
{
	if(!buf || !bufsize)
		return;

	testalloc((void **)&incstack, nincstack, &naincstack, sizeof(incstack[0]));
	/* Push the current state onto the include stack */
	/* We have to create the initial buffer because it does not exist yet. */
	if(!(incstack[nincstack].bufferstate = YY_CURRENT_BUFFER))
		incstack[nincstack].bufferstate = yy_create_buffer(yyin, YY_BUF_SIZE);
	incstack[nincstack].fp = yyin;
	incstack[nincstack].linenr = linenr;
	incstack[nincstack].charnr = charnr;
	incstack[nincstack].prevlinenr = prevlinenr;
	incstack[nincstack].prevcharnr = prevcharnr;
	incstack[nincstack].filename = filename;
	nincstack++;

	yyin = NULL;
	linenr = 0;
	charnr = 0;
	prevlinenr = 0;
	prevcharnr = 0;
	filename = strdup(name);
	yy_scan_bytes(buf, bufsize);
}

void handle_include(const wchar_t *wfname)
{
	int i;
	char *fname = strdup(ucs32_to_utf8(wfname, -1));

	testalloc((void **)&incstack, nincstack, &naincstack, sizeof(incstack[0]));
	/* Push the current state onto the include stack */
	incstack[nincstack].bufferstate = YY_CURRENT_BUFFER;
	incstack[nincstack].fp = yyin;
	incstack[nincstack].linenr = linenr;
	incstack[nincstack].charnr = charnr;
	incstack[nincstack].prevlinenr = prevlinenr;
	incstack[nincstack].prevcharnr = prevcharnr;
	incstack[nincstack].filename = filename;
	nincstack++;

	if(0 != access(fname, R_OK)) {
		for(i = 0; 1; i++) {
			char *p;
			const char *ip = includepath_get(i);
			if(!ip) {
				yyerror("Include file '%s' cannot be found", fname);
				nincstack--;
				free(fname);
				return;
			}
			p = malloc(strlen(ip) + strlen(fname) + 2);
			assert(p != NULL);
			strcpy(p, ip);
			strcat(p, "/");
			strcat(p, fname);
			if(0 == access(p, R_OK)) {
				filename = p;
				break;
			}
			free(p);
		}
	} else {
		filename = strdup(fname);
	}

	free(fname);

	yyin = fopen(filename, "r");
	if(!yyin) {
		yyerror("Include file '%s' cannot be opened", filename);
		nincstack--;
		return;
	}

	linenr = 1;
	charnr = 1;
	prevlinenr = 1;
	prevcharnr = 1;
	yy_switch_to_buffer(yy_create_buffer(yyin, YY_BUF_SIZE));
}

void lexer_cleanup(void)
{
	if(str)
		free(str);
	if(incstack)
		free(incstack);
}


void clean_buffer(void *buf)
{
    yy_delete_buffer((YY_BUFFER_STATE)(buf));  
}

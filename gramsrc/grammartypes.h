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
#ifndef __GCMC_GRAMMARTYPES_H
#define __GCMC_GRAMMARTYPES_H

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <wchar.h>

typedef enum {
	NT_INVALID,
	NT_EXPR,
	NT_EXPRLIST,
	NT_IF,
	NT_ELIF,
	NT_FOR,
	NT_FOREACH,
	NT_REPEAT,
	NT_WHILE,
	NT_DOWHILE,
	NT_FUNCTION,
	NT_ARGLIST,
	NT_BREAK,
	NT_CONTINUE,
	NT_RETURN,
	NT_LOCAL,
	NT_CONST,
} node_et;

typedef enum {
	OP_NULL,
	OP_ADD,
	OP_ADDOR,
	OP_SUB,
	OP_SUBOR,
	OP_MUL,
	OP_DIV,
	OP_MOD,
	OP_INDEX,
	OP_INDEXID,
	OP_LOR,
	OP_LAND,
	OP_EQ,
	OP_NE,
	OP_GT,
	OP_LT,
	OP_GE,
	OP_LE,
	OP_SHL,
	OP_SHR,
	OP_BAND,
	OP_BOR,
	OP_BXOR,
	OP_BNOT,
	OP_NOT,
	OP_INT,
	OP_FLOAT,
	OP_STRING,
	OP_VECTOR,
	OP_VECTORLIST,
	OP_ASSIGN,
	OP_ADDASSIGN,
	OP_ADDORASSIGN,
	OP_SUBASSIGN,
	OP_SUBORASSIGN,
	OP_MULASSIGN,
	OP_DIVASSIGN,
	OP_MODASSIGN,
	OP_SHLASSIGN,
	OP_SHRASSIGN,
	OP_BORASSIGN,
	OP_BANDASSIGN,
	OP_BXORASSIGN,
	OP_PREINC,
	OP_PREDEC,
	OP_POSTINC,
	OP_POSTDEC,
	OP_DEREF,
	OP_CONDEXPR,
	OP_CALL,
} op_et;

typedef enum {
	UNIT_NONE,
	UNIT_MM,
	UNIT_IN,
	UNIT_DEG,
	UNIT_RAD,
} unit_et;

#define _UNIT_MIL	(UNIT_IN*1000)

struct __node_t;

typedef struct __localvar_t {
	wchar_t		*id;
	struct __node_t	*init;
} localvar_t;

typedef struct __constvar_t {
	wchar_t		*id;
	struct __node_t	*init;
} constvar_t;

typedef struct __arg_t {
	wchar_t		*id;
	int		isref;
	struct __node_t	*expr;
} arg_t;

typedef struct __alist_t {
	int	n;
	int	na;
	arg_t	*args;
} alist_t;

typedef struct __nlist_t {
	int	n;
	int	na;
	struct __node_t	**nodes;
} nlist_t;

typedef struct __foreach_t {
	struct __node_t	*stmts;
	struct __node_t	*src;
	wchar_t		*dst;
} foreach_t;

typedef struct __for_t {
	struct __node_t	*stmts;
	struct __node_t	*init;
	struct __node_t	*cond;
	struct __node_t	*inc;
} for_t;

typedef struct __if_t {
	struct __node_t	*cond;
	struct __node_t	*ifclause;
	struct __node_t *elifclauses;
	struct __node_t	*elseclause;
} if_t;

typedef struct __function_t {
	wchar_t		*id;
	struct __node_t	*args;
	struct __node_t	*body;
} function_t;

typedef struct __string_t {
	int		n;	/* Length of the string */
	int		na;	/* Allocated length */
	wchar_t		*chs;	/* Actual string */
} string_t;

typedef struct __expr_t {
	struct __node_t	*left;
	struct __node_t	*right;
	union {
		struct __node_t	*cond;		/* Condition in ternary ops */
		struct __node_t	*args;		/* Call arguments */
	};
	op_et		op;		/* Operation */
	union {
		wchar_t		*id;		/* Ident ref */
		int		i;		/* integer const */
		double		d;		/* float const */
		string_t	str;		/* Strings */
		nlist_t		nlist;		/* Vector values */
	};
	unit_et		unit;		/* mm/mil unit */
	int		inparen;	/* Set if this expr in in () */
} expr_t;

typedef struct __node_t {
	int		linenr;
	int		charnr;
	const char	*filename;
	node_et		type;
	struct __node_t	*next;
	struct __node_t	*prev;
	union {
		nlist_t		nlist;
		alist_t		alist;
		localvar_t	lvar;
		constvar_t	cvar;
		expr_t		expr;
		struct __node_t	*eref;
		foreach_t	lfe;
		for_t		lfor;
		if_t		cond;
		function_t	func;
	};
} node_t;

/* From grammery.y */
void node_delete(node_t *n);
void parser_cleanup(void);
extern int yydebug;
extern node_t *scripthead;

/* From grammerl.l */
void handle_include(const wchar_t *wfname);
void define_setup(char *buf, int bufsize, const char *name);
void lexer_cleanup(void);
extern FILE *yyin;
extern int linenr;
extern int charnr;
extern int prevlinenr;
extern int prevcharnr;
extern const char *filename;

#endif

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
%{
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "utils.h"
#include "grammartypes.h"
#include "builtin.h"
#include "interpreter.h"

#define myassert(x)	do { \
				if(!(x)) { \
					yyfatal("Assertion failed:(%s:%d): %s", __FILE__, __LINE__, #x); \
				} \
			} while(0);
int yylex(void);

static wchar_t *funcname;

static node_t *newnode(int type);
static node_t *alist_new(wchar_t *id, int isref, node_t *e);
static node_t *alist_add(node_t *l, wchar_t *id, int isref, node_t *e);
static node_t *lvar_new(wchar_t *id, node_t *e);
static node_t *cvar_new(wchar_t *id, node_t *e);
static node_t *elist_new(node_t *e);
static node_t *elist_add(node_t *l, node_t *e);
static node_t *vlist_new(node_t *v);
static node_t *vlist_add(node_t *l, node_t *v);
static node_t *vec_new(node_t *e, int allownull);
static node_t *vec_add(node_t *v, node_t *e);
static node_t *node_new_assign(node_t *lv, int op, node_t *rv);
static node_t *node_new(int op, node_t *e);
static node_t *node_add(node_t *tailnode, node_t *newnode);
static node_t *expr_new(node_t *l, node_t *r, int op);
static node_t *expr_new_unary(node_t *id, int op);
static node_t *expr_new_tern(node_t *c, node_t *l, node_t *r, int op);
static node_t *expr_new_call(wchar_t *id, node_t *e);
static node_t *expr_new_id(wchar_t *id);
static node_t *expr_new_idx(node_t *d, node_t *e);
static node_t *expr_new_idxid(node_t *d, wchar_t *id);
static node_t *expr_new_int(int i, int unit);
static node_t *expr_new_flt(double d, int unit);
static node_t *expr_new_str(wchar_t *str);
static node_t *pushtag(node_t *n);
static node_t *poptag(void);
static node_t *gethead(node_t *n);
static void checkfuncname(const wchar_t *fn);
static void check_useless(const node_t *n);
static void check_const_expr(const node_t *n);
static void check_boolean_expr(const node_t *n);

node_t *scripthead;

%}

%union{
	wchar_t		*str;
	double		d;
	int		i;
	node_t		*node;
}

/* Explicit tokens */
%token FUNCTION FOR FOREACH DO WHILE IF ELIF ELSE BREAK CONTINUE RETURN INCLUDE LOCAL REPEAT CONST
%token TOPEN TCLOSE MM MIL IN DEG RAD
%token <str> IDENT STRING
%token <i> NUMBER
%token <d> FLOATV
%right '=' SUBASSIGN ADDASSIGN MULASSIGN DIVASSIGN MODASSIGN SHLASSIGN SHRASSIGN ADDORASSIGN SUBORASSIGN BORASSIGN BANDASSIGN BXORASSIGN
%right '?' ':'
%left LOR
%left LAND
%left '|'
%left '^'
%left '&'
%left EQ NE
%left LT GT LE GE
%left SHL SHR
%left '+' '-' ADDOR SUBOR
%left '*' '/' '%'
%left INC DEC
%right '!' '~' UPM
%right '[' '.' UID

%type <i> optunit optref
%type <str> function
%type <node> lines line compound optstmts stmt optstmt
%type <node> optelif elif
%type <node> arglist optv optarglist locals local optassgn
%type <node> optargs args
%type <node> expr boolexpr call anynum
%type <node> nums vector
%type <node> vlist veclist
%type <node> cvars cvar

%%
file	: /* Empty */		{ ; }
	| lines			{ scripthead = gethead($1); }
	| lines error		{ yyerror("Syntax error"); }
	;

lines	: line			{ $$ = $1; }
	| lines line		{ $$ = node_add($1, $2); }
	;

line	: compound			{ $$ = $1; }
	| INCLUDE '(' STRING ')' ';'	{
			/* Include files is a language construct */
			/* We should get here without a lookahead symbol */
			assert(yychar == YYEMPTY);
			handle_include($3);	/* Setup flex */
			free($3);
			$$ = NULL;
		}
	| INCLUDE '(' STRING ')' error	{ $$ = NULL; yyerror("';' expected"); }
	| INCLUDE '(' STRING error	{ $$ = NULL; yyerror("')' expected"); }
	| INCLUDE '(' error		{ $$ = NULL; yyerror("String with include filename expected"); }
	| INCLUDE error			{ $$ = NULL; yyerror("'(' expected"); }
	| stmt ';'			{ $$ = $1; }
	| stmt error			{ $$ = NULL; yyerror("';' expected"); }
	| LOCAL locals ';'		{ $$ = $2; }
	| CONST cvars ';'		{ $$ = $2; }
	| ';'				{ $$ = NULL; }
	;

cvars	: cvar				{ $$ = $1; }
	| cvars ',' cvar		{ $$ = node_add($1, $3); }
	;

cvar	: IDENT '=' expr		{ $$ = cvar_new($1, $3); }
	| IDENT '=' error		{ $$ = NULL; yyerror("Expression expected"); }
	| IDENT error		 	{ $$ = NULL; yyerror("'=' expected"); }
	| error			 	{ $$ = NULL; yyerror("Identifier expected"); }
	;

optarglist: /* Empty */			{ $$ = NULL; }
	| arglist			{ $$ = $1; }
	;

arglist	: optref IDENT optv		{ $$ = alist_new($2, $1, $3); }
	| arglist ',' optref IDENT optv	{ $$ = alist_add($1, $4, $3, $5); }
	| arglist ',' error		{ $$ = NULL; yyerror("Identifier expected"); }
	;

optv	: /* Empty */			{ $$ = NULL; }
	| '=' expr			{ $$ = $2; }
	| '=' error			{ $$ = NULL; yyerror("Constant expression expected"); }
	;

optref	: /* Empty */			{ $$ = 0; }
	| '&'				{ $$ = 1; }
	;

locals	: local				{ $$ = $1; }
	| locals ',' local		{ $$ = node_add($1, $3); }
	| locals ',' error		{ $$ = NULL; yyerror("Identifier expected"); }
	;

local	: IDENT optassgn		{ $$ = lvar_new($1, $2); }
	| IDENT error			{ $$ = NULL; yyerror("Assignment expected"); }
	;

optassgn: /* Empty */		{ $$ = NULL; }
	| '=' expr		{ $$ = $2; }
	;

optstmts: /* Empty */		{ $$ = NULL; }
	| lines			{ $$ = gethead($1); }
	;

stmt	: expr			{ $$ = $1; check_const_expr($1); }
	;

optstmt	: /* Empty */		{ $$ = NULL; }
	| stmt			{ $$ = $1; }
	;

foreach	: FOREACH		{ pushtag(newnode(NT_FOREACH)); }
	;

repeat	: REPEAT		{ pushtag(newnode(NT_REPEAT)); }
	;

for	: FOR			{ pushtag(newnode(NT_FOR)); }
	;

while	: WHILE			{ pushtag(newnode(NT_WHILE)); }
	;

do	: DO			{ pushtag(newnode(NT_DOWHILE)); }
	;

function: FUNCTION IDENT	{
			if(funcname)
				yyerror("Function in functions not supported");
			checkfuncname($2);
			$$ = funcname = $2;
		}
	| FUNCTION error	{ $$ = NULL; yyerror("Function name expected"); }
	;

boolexpr: expr			{ $$ = $1; check_boolean_expr($$); }
	;

compound: foreach '(' expr ';' IDENT ')' TOPEN optstmts TCLOSE		{
			$$ = poptag();
			$$->lfe.src = $3;
			$$->lfe.dst = $5;
			$$->lfe.stmts = $8;
		}
	| foreach '(' expr ';' IDENT ')' TOPEN optstmts error		{ $$ = NULL; yyerror("'}' expected"); }
	| foreach '(' expr ';' IDENT ')' error				{ $$ = NULL; yyerror("'{' expected"); }
	| foreach '(' expr ';' IDENT error				{ $$ = NULL; yyerror("')' expected"); }
	| foreach '(' expr ';' error					{ $$ = NULL; yyerror("Identifier expected"); }
	| foreach '(' expr error					{ $$ = NULL; yyerror("';' expected"); }
	| foreach '(' error						{ $$ = NULL; yyerror("Vectorlist expression expected"); }
	| foreach error							{ $$ = NULL; yyerror("'(' expected"); }
	| repeat '(' expr ';' IDENT ')' TOPEN optstmts TCLOSE		{
			$$ = poptag();
			$$->lfe.src = $3;
			$$->lfe.dst = $5;
			$$->lfe.stmts = $8;
		}
	| repeat '(' expr ')' TOPEN optstmts TCLOSE		{
			$$ = poptag();
			$$->lfe.src = $3;
			$$->lfe.dst = NULL;
			$$->lfe.stmts = $6;
		}
	| repeat '(' expr ';' IDENT ')' TOPEN optstmts error		{ $$ = NULL; yyerror("'}' expected"); }
	| repeat '(' expr ';' IDENT ')' error				{ $$ = NULL; yyerror("'{' expected"); }
	| repeat '(' expr ';' IDENT error				{ $$ = NULL; yyerror("')' expected"); }
	| repeat '(' expr ';' error					{ $$ = NULL; yyerror("Identifier expected"); }
	| repeat '(' expr error						{ $$ = NULL; yyerror("';' or ')' expected"); }
	| repeat '(' error						{ $$ = NULL; yyerror("Integer expression expected"); }
	| repeat error							{ $$ = NULL; yyerror("'(' expected"); }
	| for '(' optstmt ';' boolexpr ';' optstmt ')' TOPEN optstmts TCLOSE	{
			$$ = poptag();
			$$->lfor.init = $3;
			$$->lfor.cond = $5;
			$$->lfor.inc = $7;
			$$->lfor.stmts = $10;
		}
	| for '(' optstmt ';' boolexpr ';' optstmt ')' TOPEN optstmts error	{ $$ = NULL; yyerror("'}' expected"); }
	| for '(' optstmt ';' boolexpr ';' optstmt ')' error		{ $$ = NULL; yyerror("'{' expected"); }
	| for '(' optstmt ';' boolexpr ';' optstmt error		{ $$ = NULL; yyerror("')' expected"); }
	| for '(' optstmt ';' boolexpr error				{ $$ = NULL; yyerror("';' expected"); }
	| for '(' optstmt ';' error					{ $$ = NULL; yyerror("Conditional expression expected"); }
	| for '(' optstmt error						{ $$ = NULL; yyerror("';' expected"); }
	| for error							{ $$ = NULL; yyerror("'(' expected"); }
	| while '(' boolexpr ')' TOPEN optstmts TCLOSE			{
			$$ = poptag();
			$$->lfor.cond = $3;
			$$->lfor.stmts = $6;
		}
	| while '(' boolexpr ')' TOPEN optstmts error			{ $$ = NULL; yyerror("'}' expected"); }
	| while '(' boolexpr ')' error					{ $$ = NULL; yyerror("'{' expected"); }
	| while '(' boolexpr error					{ $$ = NULL; yyerror("')' expected"); }
	| while '(' error						{ $$ = NULL; yyerror("Conditional expression expected"); }
	| while error							{ $$ = NULL; yyerror("'(' expected"); }
	| do TOPEN optstmts TCLOSE WHILE '(' boolexpr ')' ';'		{
			$$ = poptag();
			$$->lfor.cond = $7;
			$$->lfor.stmts = $3;
		}
	| do TOPEN optstmts TCLOSE WHILE '(' boolexpr ')' error		{ $$ = NULL; yyerror("';' expected"); }
	| do TOPEN optstmts TCLOSE WHILE '(' boolexpr error		{ $$ = NULL; yyerror("')' expected"); }
	| do TOPEN optstmts TCLOSE WHILE '(' error			{ $$ = NULL; yyerror("Conditional expression expected"); }
	| do TOPEN optstmts TCLOSE WHILE error				{ $$ = NULL; yyerror("'(' expected"); }
	| do TOPEN optstmts TCLOSE error				{ $$ = NULL; yyerror("'while' expected"); }
	| do TOPEN optstmts error					{ $$ = NULL; yyerror("'}' expected"); }
	| do error							{ $$ = NULL; yyerror("'{' expected"); }
	| function '(' optarglist ')' TOPEN optstmts TCLOSE	{
			$$ = newnode(NT_FUNCTION);
			$$->func.id = $1;
			$$->func.args = $3;
			$$->func.body = $6;
			funcname = NULL;
		}
	| function '(' optarglist ')' TOPEN optstmts error		{ $$ = NULL; yyerror("'}' expected"); funcname = NULL; }
	| function '(' optarglist ')' error				{ $$ = NULL; yyerror("'{' expected"); funcname = NULL; }
	| function '(' optarglist error					{ $$ = NULL; yyerror("')' expected"); funcname = NULL; }
	| function error						{ $$ = NULL; yyerror("'(' expected"); funcname = NULL; }
	| IF '(' boolexpr ')' TOPEN optstmts TCLOSE optelif			{
			$$ = newnode(NT_IF);
			$$->cond.cond = $3;
			$$->cond.ifclause = $6;
			$$->cond.elifclauses = gethead($8);
		}
	| IF '(' boolexpr ')' TOPEN optstmts TCLOSE optelif ELSE TOPEN optstmts TCLOSE	{
			$$ = newnode(NT_IF);
			$$->cond.cond = $3;
			$$->cond.ifclause = $6;
			$$->cond.elifclauses = gethead($8);
			$$->cond.elseclause = $11;
		}
	| IF '(' boolexpr ')' TOPEN optstmts TCLOSE optelif ELSE TOPEN optstmts error	{ $$ = NULL; yyerror("'}' expected"); }
	| IF '(' boolexpr ')' TOPEN optstmts TCLOSE optelif ELSE error			{ $$ = NULL; yyerror("'{' expected"); }
	| IF '(' boolexpr ')' TOPEN optstmts error					{ $$ = NULL; yyerror("'}' expected"); }
	| IF '(' boolexpr ')' error	{ $$ = NULL; yyerror("'{' expected"); }
	| IF '(' boolexpr error		{ $$ = NULL; yyerror("')' expected"); }
	| IF '(' error			{ $$ = NULL; yyerror("Conditional expression expected"); }
	| IF error			{ $$ = NULL; yyerror("'(' expected"); }
	| RETURN expr ';'		{ $$ = node_new(NT_RETURN, $2); }
	| RETURN ';'			{ $$ = node_new(NT_RETURN, NULL); }
	| RETURN error			{ $$ = NULL; yyerror("Expression or ';' expected"); }
	| RETURN expr error		{ $$ = NULL; yyerror("';' expected"); }
	| BREAK ';'			{ $$ = node_new(NT_BREAK, NULL); }
	| BREAK error			{ $$ = NULL; yyerror("';' expected"); }
	| CONTINUE ';'			{ $$ = node_new(NT_CONTINUE, NULL); }
	| CONTINUE error		{ $$ = NULL; yyerror("';' expected"); }
	;

optelif	: /* Empty */		{ $$ = NULL; }
	| optelif elif		{ $$ = $2; if($1) { $1->next = $2; $2->prev = $1;} }
	;

elif	: ELIF '(' boolexpr ')' TOPEN optstmts TCLOSE	{ $$ = newnode(NT_ELIF); $$->cond.cond = $3; $$->cond.ifclause = $6; }
	| ELIF '(' boolexpr ')' TOPEN optstmts error	{ $$ = NULL; yyerror("'}' expected"); }
	| ELIF '(' boolexpr ')' error			{ $$ = NULL; yyerror("'{' expected"); }
	| ELIF '(' boolexpr error			{ $$ = NULL; yyerror("')' expected"); }
	| ELIF '(' error				{ $$ = NULL; yyerror("Conditional expression expected"); }
	| ELIF error					{ $$ = NULL; yyerror("'(' expected"); }
	;

call	: IDENT '(' optargs ')'	{ $$ = expr_new_call($1, $3); }
	;

optargs	: /* Empty */		{ $$ = NULL; }
	| args			{ $$ = $1; }
	;

args	: expr			{ $$ = elist_new($1); }
	| args ',' expr		{ $$ = elist_add($1, $3); }
	| args ',' error	{ $$ = $1; yyerror("Expression expected after ','"); }
	| args error		{ $$ = $1; yyerror("',' expected"); }
	;

expr	: '+' expr %prec UPM	{ $$ = $2; }
	| '-' expr %prec UPM	{ $$ = expr_new($2, expr_new_int(-1, UNIT_NONE), OP_MUL); }
	| '!' expr		{ $$ = expr_new($2, NULL, OP_NOT); }
	| '~' expr		{ $$ = expr_new($2, NULL, OP_BNOT); }
	| '(' expr ')'		{ $$ = $2; $$->expr.inparen = 1; }
	| expr '+' expr		{ $$ = expr_new($1, $3, OP_ADD); }
	| expr ADDOR expr	{ $$ = expr_new($1, $3, OP_ADDOR); }
	| expr '-' expr		{ $$ = expr_new($1, $3, OP_SUB); }
	| expr SUBOR expr	{ $$ = expr_new($1, $3, OP_SUBOR); }
	| expr '*' expr		{ $$ = expr_new($1, $3, OP_MUL); }
	| expr '/' expr		{ $$ = expr_new($1, $3, OP_DIV); }
	| expr '%' expr		{ $$ = expr_new($1, $3, OP_MOD); }
	| expr EQ expr		{ $$ = expr_new($1, $3, OP_EQ); }
	| expr NE expr		{ $$ = expr_new($1, $3, OP_NE); }
	| expr GT expr		{ $$ = expr_new($1, $3, OP_GT); }
	| expr LT expr		{ $$ = expr_new($1, $3, OP_LT); }
	| expr GE expr		{ $$ = expr_new($1, $3, OP_GE); }
	| expr LE expr		{ $$ = expr_new($1, $3, OP_LE); }
	| expr LAND expr	{ $$ = expr_new($1, $3, OP_LAND); }
	| expr LOR expr		{ $$ = expr_new($1, $3, OP_LOR); }
	| expr SHL expr		{ $$ = expr_new($1, $3, OP_SHL); }
	| expr SHR expr		{ $$ = expr_new($1, $3, OP_SHR); }
	| expr '&' expr		{ $$ = expr_new($1, $3, OP_BAND); }
	| expr '|' expr		{ $$ = expr_new($1, $3, OP_BOR); }
	| expr '^' expr		{ $$ = expr_new($1, $3, OP_BXOR); }
	| expr '?' expr ':' expr {$$ = expr_new_tern($1, $3, $5, OP_CONDEXPR); }
	| call			{ $$ = $1; }
	| expr '[' expr ']'	{ $$ = expr_new_idx($1, $3); }
	| expr '.' IDENT	{ $$ = expr_new_idxid($1, $3); }
	| IDENT			{ $$ = expr_new_id($1); }
	| NUMBER optunit	{ $$ = expr_new_int($1, $2); }
	| FLOATV optunit		{ $$ = expr_new_flt($1, $2); }
	| STRING		{ $$ = expr_new_str($1); }
	| vector		{ $$ = $1; }
	| veclist		{ $$ = $1; }
	| INC expr %prec UPM	{ $$ = expr_new_unary($2, OP_PREINC); }
	| DEC expr %prec UPM	{ $$ = expr_new_unary($2, OP_PREDEC); }
	| expr INC %prec UID	{ $$ = expr_new_unary($1, OP_POSTINC); check_useless($$); }
	| expr DEC %prec UID	{ $$ = expr_new_unary($1, OP_POSTDEC); check_useless($$); }
	| expr '=' expr		{ $$ = node_new_assign($1, OP_ASSIGN, $3); }
	| expr ADDASSIGN expr	{ $$ = node_new_assign($1, OP_ADDASSIGN, $3); }
	| expr ADDORASSIGN expr	{ $$ = node_new_assign($1, OP_ADDORASSIGN, $3); }
	| expr SUBASSIGN expr	{ $$ = node_new_assign($1, OP_SUBASSIGN, $3); }
	| expr SUBORASSIGN expr	{ $$ = node_new_assign($1, OP_SUBORASSIGN, $3); }
	| expr MULASSIGN expr	{ $$ = node_new_assign($1, OP_MULASSIGN, $3); }
	| expr DIVASSIGN expr	{ $$ = node_new_assign($1, OP_DIVASSIGN, $3); }
	| expr MODASSIGN expr	{ $$ = node_new_assign($1, OP_MODASSIGN, $3); }
	| expr SHLASSIGN expr	{ $$ = node_new_assign($1, OP_SHLASSIGN, $3); }
	| expr SHRASSIGN expr	{ $$ = node_new_assign($1, OP_SHRASSIGN, $3); }
	| expr BORASSIGN expr	{ $$ = node_new_assign($1, OP_BORASSIGN, $3); }
	| expr BANDASSIGN expr	{ $$ = node_new_assign($1, OP_BANDASSIGN, $3); }
	| expr BXORASSIGN expr	{ $$ = node_new_assign($1, OP_BXORASSIGN, $3); }
	| expr '=' error	{ $$ = $1; yyerror("Expression expected after '='"); }
	| expr ADDASSIGN error	{ $$ = $1; yyerror("Expression expected after '+='"); }
	| expr ADDORASSIGN error{ $$ = $1; yyerror("Expression expected after '+|='"); }
	| expr SUBASSIGN error	{ $$ = $1; yyerror("Expression expected after '-='"); }
	| expr SUBORASSIGN error{ $$ = $1; yyerror("Expression expected after '-|='"); }
	| expr MULASSIGN error	{ $$ = $1; yyerror("Expression expected after '*='"); }
	| expr DIVASSIGN error	{ $$ = $1; yyerror("Expression expected after '/='"); }
	| expr MODASSIGN error	{ $$ = $1; yyerror("Expression expected after '%%='"); }
	| expr SHLASSIGN error	{ $$ = $1; yyerror("Expression expected after '<<='"); }
	| expr SHRASSIGN error	{ $$ = $1; yyerror("Expression expected after '>>='"); }
	| expr BORASSIGN error	{ $$ = $1; yyerror("Expression expected after '|='"); }
	| expr BANDASSIGN error	{ $$ = $1; yyerror("Expression expected after '&='"); }
	| expr BXORASSIGN error	{ $$ = $1; yyerror("Expression expected after '^='"); }
	;

optunit	: /* Empty */		{ $$ = UNIT_NONE; }
	| MM			{ $$ = UNIT_MM; }
	| MIL			{ $$ = _UNIT_MIL; }
	| IN			{ $$ = UNIT_IN; }
	| DEG			{ $$ = UNIT_DEG; }
	| RAD			{ $$ = UNIT_RAD; }
	;

optcomma: /* Empty */
	| ','
	;

veclist	: TOPEN vlist optcomma TCLOSE	{ $$ = $2; }
	| TOPEN TCLOSE			{ $$ = vlist_new(NULL); }
	| TOPEN error			{ $$ = NULL; yyerror("'}' expected"); }
	| TOPEN vlist optcomma error	{ $$ = $2; yyerror("'}' expected"); }
	;

vlist	: expr			{ $$ = vlist_new($1); }
	| vlist ',' expr	{ $$ = vlist_add($1, $3); }
	;

vector	: '[' ']'		{ $$ = vec_new(NULL, 0); }
	| '[' nums ']'		{ $$ = $2; }
	| '[' nums error	{ $$ = $2; yyerror("']' expected"); }
	;

nums	: anynum		{ $$ = vec_new($1, 1); }
	| nums ',' anynum	{ $$ = vec_add($1, $3); }
	| nums ',' error	{ $$ = $1; yyerror("Scalar expression or '-' expected"); }
	;

anynum	: '-'			{ $$ = NULL; }
	| expr			{ $$ = $1; }
	;

%%
static node_t *newnode(int type)
{
	node_t *n = calloc(1, sizeof(node_t));
	myassert(n != NULL);
	n->type = type;
	n->linenr = prevlinenr;
	n->charnr = prevcharnr;
	n->filename = filename;
	return n;
}

static node_t *newnlist(node_t *n, int i)
{
	n->nlist.na = i;
	n->nlist.nodes = calloc(i, sizeof(n->nlist.nodes[0]));
	myassert(n->nlist.nodes != NULL);
	return n;
}

static node_t *newalist(node_t *n, int i)
{
	n->alist.na = i;
	n->alist.args = calloc(i, sizeof(n->alist.args[0]));
	myassert(n->alist.args != NULL);
	return n;
}

static node_t *alist_new(wchar_t *id, int isref, node_t *e)
{
	node_t *n = newnode(NT_ARGLIST);
	myassert(!e || (e && !isref));
	newalist(n, 4);
	n->alist.n = 1;
	n->alist.args[0].id = id;
	n->alist.args[0].isref = isref;
	n->alist.args[0].expr = e;
	return n;
}

static node_t *alist_add(node_t *l, wchar_t *id, int isref, node_t *e)
{
	int i;
	int havedef = 0;
	myassert(l->type == NT_ARGLIST);
	myassert(!e || (e && !isref));
	testalloc((void **)&l->alist.args, l->alist.n, &l->alist.na, sizeof(l->alist.args[0]));
	for(i = 0; i < l->alist.n; i++) {
		if(!wcscmp(id, l->alist.args[i].id)) {
			yyerror("Argument %d's name '%ls' already used in argument %d", l->alist.n+1, id, i+1);
			return l;
		}
		if(l->alist.args[i].expr)
			havedef = 1;
	}

	if(!e && havedef)
		yyerror("Argument %d (%ls) must include default value", l->alist.n+1, id);

	if(e && isref)
		yyerror("Argument %d (%ls) cannot be both a reference and have a default value", l->alist.n+1, id);

	l->alist.args[l->alist.n].id = id;
	l->alist.args[l->alist.n].isref = isref;
	l->alist.args[l->alist.n].expr = e;
	l->alist.n++;
	return l;
}

static node_t *lvar_new(wchar_t *id, node_t *e)
{
	node_t *n = newnode(NT_LOCAL);
	n->lvar.id = id;
	n->lvar.init = e;
	return n;
}

static node_t *cvar_new(wchar_t *id, node_t *e)
{
	node_t *n = newnode(NT_CONST);
	n->cvar.id = id;
	n->cvar.init = e;
	return n;
}

static node_t *elist_new(node_t *e)
{
	node_t *n = newnode(NT_EXPRLIST);
	newnlist(n, 4);
	n->nlist.n = 1;
	n->nlist.nodes[0] = e;
	return n;
}

static node_t *elist_add(node_t *l, node_t *e)
{
	myassert(e->type == NT_EXPR);
	myassert(l->type == NT_EXPRLIST);
	testalloc((void **)&l->nlist.nodes, l->nlist.n, &l->nlist.na, sizeof(l->nlist.nodes[0]));
	l->nlist.nodes[l->nlist.n] = e;
	l->nlist.n++;
	return l;
}

static node_t *vlist_new(node_t *v)
{
	myassert(v == NULL || v->type == NT_EXPR);
	node_t *n = newnode(NT_EXPR);
	n->expr.op = OP_VECTORLIST;
	n->expr.nlist.na = 4;
	n->expr.nlist.nodes = calloc(4, sizeof(n->expr.nlist.nodes[0]));
	myassert(n->expr.nlist.nodes != NULL);
	if(v) {
		n->expr.nlist.n = 1;
		n->expr.nlist.nodes[0] = v;
	}
	return n;
}

static node_t *vlist_add(node_t *l, node_t *v)
{
	myassert(l->type == NT_EXPR);
	myassert(l->expr.op == OP_VECTORLIST);
	myassert(v->type == NT_EXPR);
	testalloc((void **)&l->expr.nlist.nodes, l->expr.nlist.n, &l->expr.nlist.na, sizeof(l->expr.nlist.nodes[0]));
	l->expr.nlist.nodes[l->expr.nlist.n] = v;
	l->expr.nlist.n++;
	return l;
}

static node_t *vec_new(node_t *e, int allownull)
{
	node_t *n = newnode(NT_EXPR);
	n->expr.op = OP_VECTOR;
	n->expr.nlist.na = 4;
	n->expr.nlist.nodes = calloc(4, sizeof(n->expr.nlist.nodes[0]));
	myassert(n->expr.nlist.nodes != NULL);
	if(allownull || e) {
		n->expr.nlist.n = 1;
		n->expr.nlist.nodes[0] = e;
	}
	return n;
}

static node_t *vec_add(node_t *v, node_t *e)
{
	myassert(v->type == NT_EXPR);
	myassert(v->expr.op == OP_VECTOR);
	testalloc((void **)&v->expr.nlist.nodes, v->expr.nlist.n, &v->expr.nlist.na, sizeof(v->expr.nlist.nodes[0]));
	v->expr.nlist.nodes[v->expr.nlist.n] = e;
	v->expr.nlist.n++;
	return v;
}

static node_t *node_new_assign(node_t *lv, int op, node_t *rv)
{
	node_t *n = newnode(NT_EXPR);
	n->expr.op = op;
	n->expr.left = lv;
	n->expr.right = rv;
	check_useless(n);
	return n;
}

static node_t *node_new(int nt, node_t *e)
{
	node_t *n = newnode(nt);
	n->eref = e;
	return n;
}

static node_t *node_add(node_t *tailnode, node_t *newnode)
{
	node_t *nnhead = newnode;
	node_t *nntail = newnode;

	/* Find real head and tail of the new node(s) */
	if(newnode) {
		while(nnhead->prev)
			nnhead = nnhead->prev;
		while(nntail->next)
			nntail = nntail->next;
	}

	if(!tailnode)
		return nntail;	/* Always return the real tail */

	/* Find the tail's real tail */
	while(tailnode->next)
		tailnode = tailnode->next;

	if(!newnode)
		return tailnode;

	/* Crosslink to append new nodes */
	tailnode->next = nnhead;
	nnhead->prev = tailnode;
	return nntail;	/* Return the real tail of the combined list */
}

static node_t *expr_new(node_t *l, node_t *r, int op)
{
	node_t *n = newnode(NT_EXPR);
	myassert(l != NULL);
	n->expr.op = op;
	n->expr.left = l;
	n->expr.right = r;
	return n;
}

static node_t *expr_new_unary(node_t *id, int op)
{
	node_t *n = newnode(NT_EXPR);
	n->expr.op = op;
	n->expr.left = id;
	return n;
}

static node_t *expr_new_tern(node_t *c, node_t *l, node_t *r, int op)
{
	node_t *n = newnode(NT_EXPR);
	myassert(c != NULL);
	myassert(l != NULL);
	myassert(r != NULL);
	n->expr.op = op;
	n->expr.cond = c;
	n->expr.left = l;
	n->expr.right = r;
	return n;
}

static node_t *expr_new_call(wchar_t *id, node_t *e)
{
	node_t *n = newnode(NT_EXPR);
	myassert(e == NULL || e->type == NT_EXPRLIST);
	n->expr.id = id;
	n->expr.args = e;
	n->expr.op = OP_CALL;
	return n;
}

static node_t *expr_new_id(wchar_t *id)
{
	node_t *n = newnode(NT_EXPR);
	n->expr.op = OP_DEREF;
	n->expr.id = id;
	if(!wcscmp(L"__global_offset", n->expr.id) || !wcscmp(L"__global_position", n->expr.id))
		rtwarning(n, "Using internal variable '%ls' strongly discouraged", n->expr.id);
	return n;
}

static node_t *expr_new_idx(node_t *d, node_t *e)
{
	node_t *n = newnode(NT_EXPR);
	n->expr.op = OP_INDEX;
	n->expr.left = d;
	n->expr.right = e;
	return n;
}

static const wchar_t axisnames[] = L"xyzabcuvw";

static node_t *expr_new_idxid(node_t *d, wchar_t *id)
{
	node_t *n = newnode(NT_EXPR);
	assert(id != NULL);
	n->expr.op = OP_INDEXID;
	n->expr.left = d;
	wchar_t *cptr = wcschr(axisnames, id[0]);
	if(!cptr || 1 != wcslen(id))
		yyerror("Index can only be an axis name");
	n->expr.right = expr_new_int(cptr - axisnames, UNIT_NONE);
	free(id);
	return n;
}

static node_t *expr_new_int(int i, int unit)
{
	node_t *n = newnode(NT_EXPR);
	if(unit == _UNIT_MIL) {
		n->expr.op = OP_FLOAT;
		n->expr.d = (double)i / 1000.0;
		n->expr.unit = UNIT_IN;
	} else {
		n->expr.op = OP_INT;
		n->expr.i = i;
		n->expr.unit = unit;
	}
	return n;
}

static node_t *expr_new_flt(double d, int unit)
{
	node_t *n = newnode(NT_EXPR);
	n->expr.op = OP_FLOAT;
	if(unit == _UNIT_MIL) {
		d /= 1000.0;
		unit = UNIT_IN;
	}
	n->expr.d = d;
	n->expr.unit = unit;
	return n;
}

static node_t *expr_new_str(wchar_t *str)
{
	node_t *n = newnode(NT_EXPR);
	n->expr.op = OP_STRING;
	n->expr.str.chs = str;
	n->expr.str.n = wcslen(str);
	n->expr.str.na = n->expr.str.n + 1;
	return n;
}

static node_t **nodestack;
static int nnodestack;
static int nanodestack;

static node_t *pushtag(node_t *n)
{
	if(!nodestack) {
		nodestack = calloc(16, sizeof(*nodestack));
		assert(nodestack != NULL);
		nnodestack = 0;
		nanodestack = 16;
	} else if(nnodestack >= nanodestack) {
		nodestack = realloc(nodestack, nanodestack * 2 * sizeof(*nodestack));
		assert(nodestack != NULL);
		nanodestack *= 2;
	}
	nodestack[nnodestack] = n;
	nnodestack++;
	return n;
}

static node_t *poptag(void)
{
	assert(nnodestack > 0);
	nnodestack--;
	return nodestack[nnodestack];
}

static node_t *gethead(node_t *n)
{
	if(!n)
		return NULL;
	while(n->prev)
		n = n->prev;
	return n;
}

void node_delete(node_t *head)
{
	node_t *n, *next;
	int i;

	assert(nnodestack == 0);

	if(nodestack) {
		free(nodestack);
		nodestack = NULL;
	}

	if(!head)
		return;

	for(n = head; n; n = next) {
		next = n->next;
		switch(n->type) {
		case NT_EXPRLIST:
			for(i = 0; i < n->nlist.n; i++)
				node_delete(n->nlist.nodes[i]);
			free(n->nlist.nodes);
			break;
		case NT_ARGLIST:
			for(i = 0; i < n->alist.n; i++) {
				node_delete(n->alist.args[i].expr);
				free(n->alist.args[i].id);
			}
			free(n->alist.args);
			break;
		case NT_RETURN:
			node_delete(n->eref);
			break;
		case NT_LOCAL:
			node_delete(n->lvar.init);
			free(n->lvar.id);
			break;
		case NT_CONST:
			node_delete(n->cvar.init);
			free(n->cvar.id);
			break;
		case NT_BREAK:
		case NT_CONTINUE:
		case NT_INVALID:
			break;
		case NT_EXPR:
			node_delete(n->expr.left);
			node_delete(n->expr.right);
			switch(n->expr.op) {
			case OP_NULL:
				break;
			case OP_ADD:
			case OP_ADDOR:
			case OP_SUB:
			case OP_SUBOR:
			case OP_MUL:
			case OP_DIV:
			case OP_MOD:
			case OP_LOR:
			case OP_LAND:
			case OP_BOR:
			case OP_BXOR:
			case OP_BAND:
			case OP_BNOT:
			case OP_EQ:
			case OP_NE:
			case OP_GT:
			case OP_LT:
			case OP_GE:
			case OP_LE:
			case OP_SHL:
			case OP_SHR:
			case OP_NOT:
			case OP_INT:
			case OP_FLOAT:
				break;
			case OP_CONDEXPR:
				node_delete(n->expr.cond);
				break;
			case OP_CALL:
				node_delete(n->expr.args);
				free(n->expr.id);
				break;
			case OP_ASSIGN:
			case OP_ADDASSIGN:
			case OP_ADDORASSIGN:
			case OP_SUBASSIGN:
			case OP_SUBORASSIGN:
			case OP_MULASSIGN:
			case OP_DIVASSIGN:
			case OP_MODASSIGN:
			case OP_SHLASSIGN:
			case OP_SHRASSIGN:
			case OP_BORASSIGN:
			case OP_BANDASSIGN:
			case OP_BXORASSIGN:
			case OP_INDEX:
			case OP_INDEXID:
				break;
			case OP_STRING:
				free(n->expr.str.chs);
				break;
			case OP_DEREF:
				free(n->expr.id);
				break;
			case OP_PREINC:
			case OP_PREDEC:
			case OP_POSTINC:
			case OP_POSTDEC:
				break;
			case OP_VECTOR:
			case OP_VECTORLIST:
				for(i = 0; i < n->expr.nlist.n; i++)
					node_delete(n->expr.nlist.nodes[i]);
				free(n->expr.nlist.nodes);
				break;
			}
			break;
		case NT_IF:
			node_delete(n->cond.cond);
			node_delete(n->cond.ifclause);
			node_delete(n->cond.elifclauses);
			node_delete(n->cond.elseclause);
			break;
		case NT_ELIF:
			node_delete(n->cond.cond);
			node_delete(n->cond.ifclause);
			break;
		case NT_FOR:
		case NT_WHILE:
		case NT_DOWHILE:
			node_delete(n->lfor.stmts);
			node_delete(n->lfor.init);
			node_delete(n->lfor.cond);
			node_delete(n->lfor.inc);
			break;
		case NT_FOREACH:
		case NT_REPEAT:
			node_delete(n->lfe.stmts);
			node_delete(n->lfe.src);
			if(n->lfe.dst)
				free(n->lfe.dst);
			break;
		case NT_FUNCTION:
			node_delete(n->func.args);
			node_delete(n->func.body);
			free(n->func.id);
			break;
		}
		free(n);
	}
}

static const wchar_t **funcs;
static int nfuncs;
static int nafuncs;

const builtins_t *find_builtin(const wchar_t *s);

static void checkfuncname(const wchar_t *fn)
{
	int i;
	if(find_builtin(fn)) {
		yyerror("Function name '%ls' reserved as built-in function", fn);
		return;
	}
	for(i = 0; i < nfuncs; i++) {
		if(!wcscmp(fn , funcs[i])) {
			yyerror("Function name '%ls' is already defined", fn);
			return;
		}
	}
	testalloc((void **)&funcs, nfuncs, &nafuncs, sizeof(*funcs));
	funcs[nfuncs] = fn;
	nfuncs++;
}

void parser_cleanup(void)
{
	if(funcs) {
		free(funcs);
		funcs = NULL;
		nafuncs = nfuncs = 0;
	}
}

static int findderef(const node_t *n)
{
	const node_t *lv;
	/* Check if lvalue (left) is dereference-able */
	assert(n->expr.left != NULL);
	if(n->expr.inparen)
		return -1;
	for(lv = n->expr.left; lv; lv = lv->expr.left) {
		myassert(n->type == NT_EXPR);
		if(lv->expr.inparen)
			return -1;
		if(!lv->expr.left)
			break;
	}
	assert(lv != NULL);
	if(lv->expr.op == OP_DEREF) {
		if(!wcscmp(L"__global_offset", lv->expr.id) || !wcscmp(L"__global_position", lv->expr.id))
			yyerror("Assigning to read-only variable '%ls' not allowed", lv->expr.id);
		return 1;		/* Deref operation works on IDENT --> OK */
	}
	return -1;
}

static void check_useless(const node_t *n)
{
	assert(n != NULL);
	myassert(n->type == NT_EXPR);

	switch(n->expr.op) {
	case OP_CALL:
		/* Calls are fine */
		return;
	case OP_POSTINC:
	case OP_POSTDEC:
		/* Post inc/dec on a constant has no effect */
		if(findderef(n) >= 0)
			return;
		yyerror("Statement post-%s on a constant has no effect", n->expr.op == OP_POSTINC ? "increment" : "decrement");
		break;
	case OP_ASSIGN:
	case OP_ADDASSIGN:
	case OP_ADDORASSIGN:
	case OP_SUBASSIGN:
	case OP_SUBORASSIGN:
	case OP_MULASSIGN:
	case OP_DIVASSIGN:
	case OP_MODASSIGN:
	case OP_SHLASSIGN:
	case OP_SHRASSIGN:
	case OP_BORASSIGN:
	case OP_BANDASSIGN:
	case OP_BXORASSIGN:
		if(findderef(n) >= 0 && !n->expr.inparen)
			return;
		/* Fallthrough */
	default:
		yyerror("Lvalue not a variable, cannot be dereferenced");
		return;
	}
}

static void check_const_expr(const node_t *n)
{
	if(!n)
		return;

	myassert(n->type == NT_EXPR);

	switch(n->expr.op) {
	case OP_CALL:
		/* Calls are fine */
		return;
	case OP_ASSIGN:
	case OP_ADDASSIGN:
	case OP_ADDORASSIGN:
	case OP_SUBASSIGN:
	case OP_SUBORASSIGN:
	case OP_MULASSIGN:
	case OP_DIVASSIGN:
	case OP_MODASSIGN:
	case OP_SHLASSIGN:
	case OP_SHRASSIGN:
	case OP_BORASSIGN:
	case OP_BANDASSIGN:
	case OP_BXORASSIGN:
		/* We already check assignments */
		return;
	case OP_PREINC:
	case OP_PREDEC:
	case OP_POSTINC:
	case OP_POSTDEC:
		if(findderef(n) >= 0)
			return;
		/* Fallthrough */
	default:
		yyerror("Statement has no effect");
		return;
	}
}

static void check_boolean_expr(const node_t *n)
{
	assert(n != NULL);
	myassert(n->type == NT_EXPR);

	switch(n->expr.op) {
	case OP_CALL:
		return;
	case OP_ASSIGN:
	case OP_ADDASSIGN:
	case OP_ADDORASSIGN:
	case OP_SUBASSIGN:
	case OP_SUBORASSIGN:
	case OP_MULASSIGN:
	case OP_DIVASSIGN:
	case OP_MODASSIGN:
	case OP_SHLASSIGN:
	case OP_SHRASSIGN:
	case OP_BORASSIGN:
	case OP_BANDASSIGN:
	case OP_BXORASSIGN:
		if(!n->expr.inparen) {
			rtwarning(n, "Assignment in boolean expression may be an inadvertent error, use () to force");
			return;
		}
	default:
		if(n->expr.left)
			check_boolean_expr(n->expr.left);
		if(n->expr.right)
			check_boolean_expr(n->expr.right);
	}
}

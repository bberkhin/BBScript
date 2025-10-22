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
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "utils.h"
#include "variable.h"

#define NSORTVARS	16	/* More than these nr. of vars should use qsort/bsearch */

typedef struct __scope_t {
	variable_t **vars;
	int nvars;
	int navars;
} scope_t;

scope_t globalscope;

static scope_t *scopes;
static int nscopes;
static int nascopes;

#define ls_vars(n)	(scopes[nscopes-(n)-1].vars)
#define ls_nvars(n)	(scopes[nscopes-(n)-1].nvars)
#define ls_navars(n)	(scopes[nscopes-(n)-1].navars)

#define gs_vars		(globalscope.vars)
#define gs_nvars	(globalscope.nvars)
#define gs_navars	(globalscope.navars)

void printvar(const char *pfx, variable_t *v)
{
	printf("%s: %ls: ", pfx, v->id);
	switch(v->v->type) {
	case VAL_UNDEF:		printf("undef"); break;
	case VAL_INT:		printf("int(%d)", v->v->i); break;
	case VAL_FLOAT:		printf("float(%f)", v->v->d); break;
	case VAL_VECTOR:	printf("vector[%d]", v->v->v.n); break;
	case VAL_VECTORLIST:	printf("vectorlist{%d}", v->v->vl.n); break;
	default:		printf("<invalid variable value>"); break;
	}
	printf("\n");
}

void variable_purge(void)
{
	int i;
//printf("variable_purge\n");
	while(nscopes)
		variable_scope_pop();

	if(scopes)
		free(scopes);

	for(i = 0; i < gs_nvars; i++) {
		value_delete(gs_vars[i]->v);
		free(gs_vars[i]);
	}
	free(gs_vars);
}

static int var_cmp(const void *a, const void *b)
{
	return wcscmp((*(variable_t **)a)->id, (*(variable_t **)b)->id);
}

const variable_t *variable_lookup(const wchar_t *id, int onlyglobal, int scopeoffset)
{
	int i;
	if(!onlyglobal && nscopes && nscopes - scopeoffset > 0) {
		if(ls_nvars(scopeoffset) >= NSORTVARS) {
			variable_t var;
			variable_t *vp = &var;
			variable_t **vpp;
			var.id = id;
			vpp = bsearch(&vp, ls_vars(scopeoffset), ls_nvars(scopeoffset), sizeof(*ls_vars(scopeoffset)), var_cmp);
			if(vpp)
				return *vpp;
		} else {
			for(i = 0; i < ls_nvars(scopeoffset); i++) {
				if(!ls_vars(scopeoffset)[i])
					continue;
				if(!wcscmp(ls_vars(scopeoffset)[i]->id, id))
					return ls_vars(scopeoffset)[i];
			}
		}
	}

	if(gs_nvars >= NSORTVARS) {
		variable_t var;
		variable_t *vp = &var;
		variable_t **vpp;
		var.id = id;
		vpp = bsearch(&vp, gs_vars, gs_nvars, sizeof(*gs_vars), var_cmp);
		if(vpp)
			return *vpp;
	} else {
		for(i = 0; i < gs_nvars; i++) {
			if(!gs_vars[i])
				continue;
			if(!wcscmp(gs_vars[i]->id, id))
				return gs_vars[i];
		}
	}
	return NULL;
}

const variable_t *variable_lookup_deref(const wchar_t *id, int onlyglobal)
{
	const variable_t *var = variable_lookup(id, onlyglobal, 0);
//printf("variable_lookup_deref(%s, %d) -> %p\n", id, onlyglobal, var);
	if(!var)
		return NULL;
	while(var->ref)
		var = var->ref;
	return var;
}

void variable_scope_push(void)
{
//printf("variable_scope_push %d -> %d\n", nscopes, nscopes+1);
	testalloc((void **)&scopes, nscopes, &nascopes, sizeof(scopes[0]));
	scopes[nscopes].vars = NULL;
	scopes[nscopes].nvars = 0;
	scopes[nscopes].navars = 0;
	nscopes++;
	testalloc((void **)&ls_vars(0), ls_nvars(0), &ls_navars(0), sizeof(ls_vars(0)[0]));
}

void variable_scope_pop(void)
{
	int i;
	for(i = 0; i < ls_nvars(0); i++) {
		if(!ls_vars(0)[i])
			continue;
		if(ls_vars(0)[i]->refcount)
			ls_vars(0)[i]->refcount--;
//printf("variable_scope_pop %ls ref %d\n", ls_vars(0)[i]->id, ls_vars(0)[i]->refcount);
		if(!ls_vars(0)[i]->refcount) {
			if(ls_vars(0)[i]->v)
				value_delete(ls_vars(0)[i]->v);
			if(ls_vars(0)[i]->ref)
				ls_vars(0)[i]->ref->refcount--;
			free(ls_vars(0)[i]);
		}
	}
	free(ls_vars(0));
	nscopes--;
//printf("variable_scope_pop %d -> %d\n", nscopes+1, nscopes);
}

static variable_t *variable_addref(variable_t *v)
{
	if(nscopes) {
		testalloc((void **)&ls_vars(0), ls_nvars(0), &ls_navars(0), sizeof(ls_vars(0)[0]));
		ls_vars(0)[ls_nvars(0)] = v;
		ls_nvars(0)++;
		if(ls_nvars(0) >= NSORTVARS)
			qsort(ls_vars(0), ls_nvars(0), sizeof(*ls_vars(0)), var_cmp);
	} else {
		testalloc((void **)&gs_vars, gs_nvars, &gs_navars, sizeof(gs_vars[0]));
		gs_vars[gs_nvars] = v;
		gs_nvars++;
		if(gs_nvars >= NSORTVARS)
			qsort(gs_vars, gs_nvars, sizeof(*gs_vars), var_cmp);
	}
	return v;
}

static const variable_t *variable_new(const wchar_t *id, value_t *v, int isconst)
{
	variable_t *var = calloc(1, sizeof(*var));
	assert(var != NULL);
	assert(id !=  NULL);
	assert(v !=  NULL);
	var->id = id;
	var->refcount = 1;
	var->v = v;
	var->isconst = isconst;
	variable_addref(var);
//printvar("variable_new", var);
	return var;
}

const variable_t *variable_new_ref(const wchar_t *id, variable_t *v)
{
	variable_t *var = calloc(1, sizeof(*var));
	assert(var != NULL);
	assert(id !=  NULL);
	assert(v !=  NULL);
	var->id = id;
	var->refcount = 1;
	var->ref = v;
	variable_addref(var);
	return var;
}

const variable_t *variable_set(const wchar_t *id, value_t *v)
{
	variable_t *var;

	assert(v != NULL);
	assert(!isref(v));

	if(!(var = (variable_t *)variable_lookup(id, 0, 0)))
		return variable_new(id, v, 0);
	else {
		while(var->ref)		/* Dereference indirections */
			var = var->ref;
		value_delete(var->v);
		var->v = v;
	}
	return var;
}

const variable_t *variable_set_const(const node_t *n, const wchar_t *id, value_t *v)
{
	const variable_t *var;
	if(!(var = variable_lookup(id, 0, 0)))
		return variable_new(id, v, 1);
	else
		rterror(n, "Constant '%ls' already defined%s", id, var->isconst ? "" : " as variable");
	return var;
}

const variable_t *variable_set_local(const node_t *n, const wchar_t *id, value_t *v)
{
	int i;
	if(!nscopes) {
		rtwarning(n, "Locally scoped variable '%ls' setting without local scope, adding to global scope", id);
		return variable_set(id, v);
	}

	assert(v != NULL);
	assert(!isref(v));

	for(i = 0; i < ls_nvars(0); i++) {
		if(!ls_vars(0)[i])
			continue;
		if(!wcscmp(ls_vars(0)[i]->id, id))
			break;
	}
//printf("variable_set_local(%ls, <value>) %d\n", id, nscopes);
	if(i >= ls_nvars(0))
		return variable_new(id, v, 0);
	else {
		rtwarning(n, "Local scope variable '%ls' already available in local scope", id);
		return ls_vars(0)[i];
	}
}

const variable_t *variable_ref_local(const wchar_t *dstid, const wchar_t *srcid)
{
	variable_t *var;
	int i;
	if(!nscopes)
		rtinternal(NULL, "Referencing variable from '%ls' to '%ls' local scope without local scope", srcid, dstid);
	if(!(var = (variable_t *)variable_lookup(srcid, 0, 1)))
		return NULL;
	for(i = 0; i < ls_nvars(0); i++) {
		if(!ls_vars(0)[i])
			continue;
		if(!wcscmp(ls_vars(0)[i]->id, dstid))
			break;
	}
//printf("variable_ref_local(%ls, %ls) %d\n", dstid, srcid, nscopes);
	if(i >= ls_nvars(0)) {
		var->refcount++;
		return variable_new_ref(dstid, var);
	} else {
		rtwarning(NULL, "Local scope variable '%ls' already available in local scope", dstid);
		return ls_vars(0)[i];
	}
}

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
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <time.h>

#include "builtin_robot.h"
#include "config.h"
#include "utils.h"
#include "value.h"
#include "checkarg.h"
#include "script_bind.h"

static const char axes[MAXAXES] = "XYZABCUVW";
#pragma warning(disable:4996)

static int printscalarval(const value_t *v, char tag, int globalref, int relative, int force, int angular)
{
	int rv = 0;
	double coord;
	double offs = 0.0;
	double *gpref = NULL;
	if(globalref >= 0 && globalref < naxes) {
		offs = *global_offs[globalref] + relative_offs[globalref];
		gpref = global_pos[globalref];
	}
	switch(v->type) {
	default:
		return 0;
	case VAL_INT:
	case VAL_FLOAT:
		if(angular)
			coord = value_to_deg(v);
		else
			coord = value_to_dist(v);
		if(gpref) {
			if(relative) {
				coord += *gpref;
				/*
				 * The coordinate is present and must be output
				 * to prevent a second call to this function,
				 * which would cause the absolute position to
				 * move twice.
				 * This would happen on relative moves with a
				 * target coordinate within EPSILON of zero.
				 */
				force = 1;
			} else if(!relative)
				coord -= *gpref;
		}
		if(force || !relative || !gpref || fabs(coord + offs) >= EPSILON) {
			printf(" %c%.*f", tag, cl_decimals, coord + offs);
			rv = 1;
		}
		if(gpref) {
			*gpref = coord;
		}
		return rv;
	}
}

static void movegoto(const node_t *ref, const value_t *arg, int mv, const char *fn, int relative, double feed)
{
	int i;
	int nax = 0;
    printf("Do Goto(): ");
	
	for(i = 0; i < arg->v.n && i < naxes; i++) {
		if(i < 3 || i >= 6)
			check_arg_unit_distance(ref, fn, arg->v.vals[i], 0);
		else
			check_arg_unit_angle(ref, fn, arg->v.vals[i], 0);
		if(printscalarval(arg->v.vals[i], axes[i], i, relative, 0, i >= 3 && i < 6))
			nax++;
	}
	/* If we did not get any output on relative movement, force it */
	if(!nax && relative) {
		for(i = 0; i < arg->v.n && i < naxes; i++) {
			if(printscalarval(arg->v.vals[i], axes[i], i, relative, 1, i >= 3 && i < 6))
				nax++;
		}
	}
	
    if(!nax)
		rterror(ref, "All coordinates in %s() undefined", fn);
}

static value_t *robot_goto(const node_t *ref, int argc)
{
	value_t *p = bi_position(ref, 0);
	const value_t *arg;
	double feed = -1.0;
	int i;
	check_arg_one(ref, "move", argc);
	arg = value_get(argc, 0);
	if(argc > 1) {
		const value_t *ff = value_get(argc, 1);
		check_arg_scalar(ref, "move", ff, 1);
		feed = value_to_dist(ff);
	}
	if(isvectorlist(arg)) {
		if(arg->vl.n <= 0)
			rtwarning(ref, "move(): Vectorlist contains no vectors");
		for(i = 0; i < arg->vl.n; i++) {
			movegoto(ref, arg->vl.vecs[i], 1, "move", 0, feed);
		}
	} else if(isvector(arg))
		movegoto(ref, arg, 1, "move", 0, feed);
	else
		rterror(ref, "move(): First argument must be a vector or a vectorlist");
	return p;
}



static value_t *robot_move(const node_t *ref, int argc)
{
	value_t *p = bi_position(ref, 0);
	const value_t *arg;
	double feed = -1.0;
	int i;
	check_arg_one(ref, "move", argc);
	arg = value_get(argc, 0);
	if(argc > 1) {
		const value_t *ff = value_get(argc, 1);
		check_arg_scalar(ref, "move", ff, 1);
		feed = value_to_dist(ff);
	}
	if(isvectorlist(arg)) {
		if(arg->vl.n <= 0)
			rtwarning(ref, "move(): Vectorlist contains no vectors");
		for(i = 0; i < arg->vl.n; i++) {
			movegoto(ref, arg->vl.vecs[i], 1, "move", 0, feed);
		}
	} else if(isvector(arg))
		movegoto(ref, arg, 1, "move", 0, feed);
	else
		rterror(ref, "move(): First argument must be a vector or a vectorlist");
	return p;
}



static value_t *br_sleep(const node_t *ref, int argc)
{	
	const value_t *arg0;
	value_t *v;
	check_arg_one(ref, "sleep", argc);		
	arg0 = value_get(argc, 0);
	check_arg_scalar(ref, "sleep", arg0, 0);
	int id = value_to_int(arg0);
	mr_delay_ms(id);
	return value_new(VAL_UNDEF);
}



static value_t *br_getVoltage(const node_t *ref, int argc)
{
	const value_t *arg0;
	value_t *v;
	check_arg_one(ref, "getVoltage", argc);		
	arg0 = value_get(argc, 0);
	check_arg_scalar(ref, "getVoltage", arg0, 0);
	int id = value_to_int(arg0);
	v = value_new(VAL_FLOAT);
	v->d = mr_getvoltag(id);
	return v;
}
	
static value_t *br_getPos(const node_t *ref, int argc)
{
	const value_t *arg0;
	value_t *v;
	check_arg_one(ref, "getPos", argc);		
	arg0 = value_get(argc, 0);
	check_arg_scalar(ref, "getPos", arg0, 0);
	int id = value_to_int(arg0);
	v = value_new(VAL_FLOAT);
	v->d = mr_getpos(id);
	return v;
}


static value_t *br_getVel(const node_t *ref, int argc)
{
	const value_t *arg0;
	value_t *v;
	check_arg_one(ref, "getVel", argc);		
	arg0 = value_get(argc, 0);
	check_arg_scalar(ref, "getVel", arg0, 0);
	int id = value_to_int(arg0);
	v = value_new(VAL_FLOAT);
	v->d = mr_getvel(id);
	return v;
}

static value_t *br_closeLoop(const node_t *ref, int argc)
{
	const value_t *arg0;
	value_t *v;
	check_arg_one(ref, "closeLoop", argc);		
	arg0 = value_get(argc, 0);
	check_arg_scalar(ref, "closeLoop", arg0, 0);
	int id = value_to_int(arg0);	
	mr_closeloop(id);
	return value_new(VAL_UNDEF);
}


static value_t *br_calibrate(const node_t *ref, int argc)
{
	const value_t *arg0;
	value_t *v;
	check_arg_one(ref, "calibrate", argc);		
	arg0 = value_get(argc, 0);
	check_arg_scalar(ref, "calibrate", arg0, 0);
	int id = value_to_int(arg0);	
	mr_calibrate(id);
	return value_new(VAL_UNDEF);
}

static value_t *br_setIdle(const node_t *ref, int argc)
{
	const value_t *arg0;
	value_t *v;
	check_arg_one(ref, "setIdle", argc);		
	arg0 = value_get(argc, 0);
	check_arg_scalar(ref, "setIdle", arg0, 0);
	int id = value_to_int(arg0);	
	mr_setidle(id);
	return value_new(VAL_UNDEF);
}

static value_t *br_setModePos(const node_t *ref, int argc)
{
	const value_t *arg0;
	value_t *v;
	check_arg_one(ref, "setModePos", argc);		
	arg0 = value_get(argc, 0);
	check_arg_scalar(ref, "setModePos", arg0, 0);
	int id = value_to_int(arg0);	
	mr_setmodepos(id);
	return value_new(VAL_UNDEF);
}

static value_t *br_setModeVel(const node_t *ref, int argc)
{
	const value_t *arg0;
	value_t *v;
	check_arg_one(ref, "setModeVel", argc);		
	arg0 = value_get(argc, 0);
	check_arg_scalar(ref, "setModeVel", arg0, 0);
	int id = value_to_int(arg0);	
	mr_setmodevel(id);
	return value_new(VAL_UNDEF);
}


static value_t *br_setPos(const node_t *ref, int argc)
{
	const value_t *arg0,*arg1;
	value_t *v;	
	check_arg_two(ref, "setPos", argc);		
	arg0 = value_get(argc, 0);
	arg1 = value_get(argc, 1);
	check_arg_scalar(ref, "setPos", arg0, 0);
	check_arg_scalar(ref, "setPos", arg1, 1);
	int id = value_to_int(arg0);	
	double d = value_to_double(arg1);
	mr_setpos( id, d);
	return value_new(VAL_UNDEF);	
}

static value_t *br_setVel(const node_t *ref, int argc)
{
	const value_t *arg0,*arg1;
	value_t *v;
	check_arg_two(ref, "setVel", argc);		
	arg0 = value_get(argc, 0);
	arg1 = value_get(argc, 1);
	check_arg_scalar(ref, "setVel", arg0, 0);
	check_arg_scalar(ref, "setVel", arg1, 1);
	int id = value_to_int(arg0);	
	double d = value_to_double(arg1);
	mr_setvel( id, d);
	return value_new(VAL_UNDEF);	
}
	

static value_t *br_stop(const node_t *ref, int argc)
{
	check_arg_none(ref, "stop", argc);		
	mr_stop();
	return value_new(VAL_UNDEF);	
}
	

//should be sorted in alphabetical order
const builtins_t builtins_robot[] = {	
	
	{ L"calibrate",   br_calibrate },
	{ L"closeLoop",   br_closeLoop },
	{ L"getPos",      br_getPos },
	{ L"getVel",      br_getVel },
	{ L"getVoltage",  br_getVoltage },
	{ L"setIdle",     br_setIdle },
	{ L"setModePos",  br_setModePos },
	{ L"setModeVel",  br_setModeVel },
	{ L"setPos",      br_setPos },
	{ L"setVel",      br_setVel },
	{ L"sleep",       br_sleep },
	{ L"stop",        br_stop }
	
};

const int nbuiltins_robot = NELEM(builtins_robot);

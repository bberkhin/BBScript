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


float value_to_float(const value_t *v)
{
	if(!v)
		return 0.0;
	v = value_deref(v);
	switch(v->type) {
	case VAL_INT:	return (float)v->i;
	case VAL_FLOAT:	return (float)v->d;
	default:	return 0.0f;
	}
}

static void movej_(const node_t *ref, const value_t *arg, const char *fn, float feed)
{
	int i;
	int nax = 0;
	float out_pos[6];	
	//check arg
	for(i = 0; i < arg->v.n && i < naxes; i++) {
		check_arg_unit_angle(ref, fn, arg->v.vals[i], 0);
		const value_t *v = arg->v.vals[i];
		if ( v->type == VAL_INT || v->type == VAL_FLOAT )
		{
			out_pos[i] = value_to_float(v); // TODO coord = value_to_rad(v);
			nax++;
		}
		else
			out_pos[i] = NAN;
	}
	for( ; i< naxes; i++)
		out_pos[i] = NAN;
		
    if( !nax )
		rterror(ref, "All coordinates in %s() undefined", fn);
	mr_movej(out_pos, feed );
}

static value_t *br_movej(const node_t *ref, int argc)
{
	const value_t *arg;
	float feed = 0.0f;
	int i;
	check_arg_one(ref, "movej", argc);
	arg = value_get(argc, 0);
	if(argc > 1) {
		const value_t *ff = value_get(argc, 1);
		check_arg_scalar(ref, "movej", ff, 1);
		feed = value_to_float(ff);
	}
	else 
		feed = NAN;
	if(isvectorlist(arg)) {
		if(arg->vl.n <= 0)
			rtwarning(ref, "movej(): Vectorlist contains no vectors");
		for(i = 0; i < arg->vl.n; i++) {
			movej_(ref, arg->vl.vecs[i], "movej", feed);
		}
	} else if(isvector(arg))
		movej_(ref, arg, "movej", feed);
	else
		rterror(ref, "movej(): First argument must be a vector or a vectorlist");
	return value_new(VAL_UNDEF);

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
	float d = value_to_float(arg1);
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
	float d = value_to_float(arg1);
	mr_setvel( id, d);
	return value_new(VAL_UNDEF);	
}

static value_t *br_setZero(const node_t *ref, int argc)
{
	const value_t *arg0;
	value_t *v;
	check_arg_one(ref, "setZero", argc);		
	arg0 = value_get(argc, 0);
	check_arg_scalar(ref, "setZero", arg0, 0);
	int id = value_to_int(arg0);	
	mr_setzero(id);
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
	{ L"movej",  	  br_movej },
	{ L"setIdle",     br_setIdle },
	{ L"setModePos",  br_setModePos },
	{ L"setModeVel",  br_setModeVel },
	{ L"setPos",      br_setPos },
	{ L"setVel",      br_setVel },
	{ L"setZero",     br_setZero },	
	{ L"sleep",       br_sleep },
	{ L"stop",        br_stop }
	
};

const int nbuiltins_robot = NELEM(builtins_robot);

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
#if 1
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <time.h>
//#include <getopt.h>
#include <ctype.h>
#ifdef __WIN32__
#include <io.h>
#include <fcntl.h>
#endif

#include "utils.h"
#include "value.h"
#include "variable.h"
#include "interpreter.h"
#include "builtin.h"
#include "builtin_gcode.h"
#include "builtin_svg.h"
#include "builtin_dxf.h"

/*#define FREEONEXIT	1*/	/* For valgrind testing */
#pragma warning(disable:4996)

FILE *ofp;
double *global_offs[MAXAXES];	/* Offset set at command-line */
double relative_offs[MAXAXES];	/* Offset set with relocate() function */
double *global_pos[MAXAXES];	/* Current turtle position */
plane_et global_plane = PLANE_XY;
int naxes = 6;			/* Default enable 6 axes */
int cl_decimals = 8;		/* Number of significant decimals to print in output */
int cl_relative;		/* Use relative output code */
int cl_inch;			/* Set if output is to be in inch */
format_et cl_format;		/* Set to output format to use */
int cl_svg_movelayer = 1;	/* Set to output goto()s in separate SVG layer */
int cl_svg_flip = 1;		/* Set to flip the output (mirror Y) */
int cl_svg_grid = 1;		/* Set to show grid on SVG */
double cl_svg_toolwidth = -1.0;	/* Line-width in SVG output */
double cl_svg_opacity = 0.1;	/* Transparency in SVG ouput */
int cl_nom2 = 0;		/* Use % when generating subs instead of ending in M2 */
int cl_pedantic = 0;		/* Enable pendatic warnings when set */
int runtimeerrors;		/* Nr of calls to script's error() function */
int runtimewarnings;		/* Nr of calls to script's warning() function or internal rtwarning() */

static char *definebuf;		/* Holds command-line defined entries/code */
static int ndefinebuf;
static int nadefinebuf;

static const char **includepaths;
static int nincludepaths;
static int naincludepaths;

static int syntaxerrors;

#define STRVAL(x)	#x
#define STRINGIZE(x)	STRVAL(x)

static char gcmcconsts[] =
	/* Automatically generated version numbers */
	"const GCMC_VERSION_STR = \"" VERSION "\";\n"
	"const GCMC_VERSION = (" STRINGIZE(GCMC_VERSION_MAJOR) " * 1000 + " STRINGIZE(GCMC_VERSION_MINOR) ") * 1000 + " STRINGIZE(GCMC_VERSION_POINT) ";\n"
	"const GCMC_VERSION_MAJOR = " STRINGIZE(GCMC_VERSION_MAJOR) ";\n"
	"const GCMC_VERSION_MINOR = " STRINGIZE(GCMC_VERSION_MINOR) ";\n"
	"const GCMC_VERSION_POINT = " STRINGIZE(GCMC_VERSION_POINT) ";\n"

	/* plane() argument */
	"const PLANE_QUERY = undef();\n"
	"const PLANE_XY = 0;\n"
	"const PLANE_XZ = 1;\n"
	"const PLANE_YZ = 2;\n"

	/* coolant() argument */
	"const COOLANT_OFF = 0;\n"
	"const COOLANT_MIST = 1;\n"
	"const COOLANT_FLOOD = 2;\n"
	"const COOLANT_ALL = COOLANT_MIST | COOLANT_FLOOD;\n"
	"const COOLANT_MISTFLOOD = COOLANT_ALL;\n"

	/* feedmode() argument */
	"const FEEDMODE_INVERSE = 0;\n"	/* Inverse time mode */
	"const FEEDMODE_UPM = 1;\n"	/* Units per minute */
	"const FEEDMODE_UPR = 2;\n"	/* Units per revolution */

	/* typeset() argument - Font flags and faces */
	"const FONTF_BOLD    = 0x0100;\n"
	"const FONTF_ITALIC  = 0x0200;\n"

	"const FONT_HSANS_1    = 0x0001;\n"
	"const FONT_HSANS_2    = 0x0002;\n"
	"const FONT_HSCRIPT_1  = 0x0003;\n"
	"const FONT_HSCRIPT_2  = 0x0004;\n"
	"const FONT_HTIMES     = 0x0005;\n"
	"const FONT_HSANS_1_RS = 0x0006;\n"
	"const FONT_HTIMES_BOLD        = FONT_HTIMES | FONTF_BOLD;\n"
	"const FONT_HTIMES_ITALIC      = FONT_HTIMES | FONTF_ITALIC;\n"
	"const FONT_HTIMES_ITALIC_BOLD = FONT_HTIMES | FONTF_BOLD | FONTF_ITALIC;\n"
	;
static const int ngcmcconsts = NELEM(gcmcconsts) - 1;	/* minus one to remove the string terminating \0 */

int yyparse(void);

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
	exit(1);
}

int rtinternal(const node_t *n, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	errorprint(n, fmt, "internal error", va);
	va_end(va);
	exit(1);
}

int rterror(const node_t *n, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	errorprint(n, fmt, "error", va);
	va_end(va);
	exit(1);
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

#define BUFSIZE	16384
static void cpyfile(const char *src, FILE *fp)
{
	FILE *sp = fopen(src, "rb");
	char *buf;
	if(!sp) {
		rterror(NULL, "File '%s' cannot be opened", src);
		return;
	}
	buf = malloc(BUFSIZE);
	assert(buf != NULL);
#ifdef __WIN32__
	fflush(fp);
	if(-1 == setmode(fileno(fp), O_BINARY))
		rtinternal(NULL, "Failed to set binary mode on output file");
#endif
	while(1) {
		size_t n = fread(buf, 1, BUFSIZE, sp);
		if(!n)
			break;
		while(n) {
			size_t w = fwrite(buf, 1, n, fp);
			if(!n) {
				if(ferror(fp)) {
					rterror(NULL, "Error writing output file");
					goto err_out;
				}
			} else
				n -= w;
		}
	}
err_out:
	fclose(sp);
#ifdef __WIN32__
	fflush(fp);
	if(-1 == setmode(fileno(fp), O_TEXT))
		rtinternal(NULL, "Failed to set text mode on output file");
#endif
	free(buf);
}

static void prologue(const char *src, FILE *fp)
{
	if(src)
		cpyfile(src, fp);
	else {
		switch(cl_format) {
		case FMT_GCODE:	gcode_prologue(fp); break;
		case FMT_SVG:	svg_prologue(fp); break;
		case FMT_DXF:	dxf_prologue(fp); break;
		default:	rtinternal(NULL, "prologue(): Undefined backend ID (%d) requested", cl_format);
		}
	}
}

static void epilogue(const char *src, FILE *fp)
{
	if(src)
		cpyfile(src, fp);
	else {
		switch(cl_format) {
		case FMT_GCODE:	gcode_epilogue(fp); break;
		case FMT_SVG:	svg_epilogue(fp); break;
		case FMT_DXF:	dxf_epilogue(fp); break;
		default:	rtinternal(NULL, "epilogue(): Undefined backend ID (%d) requested", cl_format);
		}
	}
}

static void includepath_add(const char *path)
{
	assert(path != NULL);
	testalloc((void **)&includepaths, nincludepaths, &naincludepaths, sizeof(*includepaths));
	includepaths[nincludepaths] = path;
	nincludepaths++;
}

#ifdef FREEONEXIT
static void includepath_purge(void)
{
	int i;
	for(i = 0; i < nincludepaths; i++)
		free((void *)includepaths[i]);
	free(includepaths);
}
#endif

const char *includepath_get(int idx)
{
	if(idx < 0)
		rtinternal(NULL, "Negative index in index search path retrieval");
	if(idx >= nincludepaths)
		return NULL;
	return includepaths[idx];
}

void define_add(const char *def)
{
	int dl;
	assert(def != NULL);
	dl = strlen(def);
	if(nadefinebuf - ndefinebuf < dl + 1) {
		if(!definebuf) {
			definebuf = malloc(dl + 1);
			assert(definebuf != NULL);
			nadefinebuf = dl + 1;
		} else {
			while(nadefinebuf - ndefinebuf < dl + 1)
				nadefinebuf <<= 1;
			definebuf = realloc(definebuf, nadefinebuf);
			assert(definebuf != NULL);
		}
	}
	strcpy(definebuf + ndefinebuf, def);
	ndefinebuf += dl;
	definebuf[ndefinebuf++] = ';';		/* This overwrites the '\0', which is not used */
}

static void asfunc_head(FILE *fp, const char *func)
{
	if(cl_nom2)
		fprintf(fp, "%%\n");
	fprintf(fp, "(gcmc compiled function: %s, do not change)\n", func);
	fprintf(fp, "o<%s> sub\n", func);
}

static void asfunc_tail(FILE *fp, const char *func)
{
	fprintf(fp, "o<%s> endsub\n", func);
	if(cl_nom2)
		fprintf(fp, "%%\n");
	else
		fprintf(fp, "M2\n");
}

static const char usage_str[] =
	"gcmc [options] [inputfile]\n"
	"Options:\n"
	"  -a|--offset-a ofs       Set global A-offset\n"
	"  -b|--offset-b ofs       Set global B-offset\n"
	"  -c|--offset-c ofs       Set global C-offset\n"
	"  -d|--debug              Enable debug\n"
	"  -D|--define stmt        Define 'stmt' prior to executing the script\n"
	"     --dxf                Output DXF path image\n"
	"  -G|--prologue file      Use 'file' as prologue code\n"
	"  -g|--epilogue file      Use 'file' as epilogue code\n"
	"     --gcode              Output G-Code path (default)\n"
	"     --gcode-function fn  Generate a function of name 'fn'\n"
	"     --gcode-nom2         Use % instead of M2 in function generation\n"
	"  -h|--help               This message\n"
	"  -I|--include path       Add include search path\n"
	"  -i|--imperial           Use imperial output units (default mm)\n"
	"  -o|--output file        Output to 'file' (default stdout)\n"
	"     --pedantic           Be more pedantic reporting warnings\n"
	"  -P|--precision dec      Use 'dec' decimals in output\n"
	"  -p|--plane plane        Set initial cutting plane (0|XY, 1|XZ, 2|YZ)\n"
	"  -q|--bare               Do not output prologue/epilogue\n"
	"  -r|--relative           Use relative positioning\n"
	"     --svg                Output SVG path image\n"
	"     --svg-opacity w      Set the opacity of the cutpath output lines\n"
	"     --svg-no-flip        Do not flip Y axis and relocate origin\n"
	"     --svg-no-movelayer   Disable output of goto moves\n"
	"     --svg-toolwidth w    Set the linewidth in the cutpath output\n"
	"     --svg-no-grid        Disable grid in SVG output\n"
	"  -U|--uvw                Enable 9-axis base XYZABCUVW (default XYZABC)\n"
	"  -u|--offset-u ofs       Set global U-offset\n"
	"  -V|--version            Print version and exit\n"
	"  -v|--offset-v ofs       Set global V-offset\n"
	"  -W|--error              Exit non-zero if warnings are encountered\n"
	"  -w|--offset-w ofs       Set global W-offset\n"
	"  -x|--offset-x ofs       Set global X-offset\n"
	"  -y|--offset-y ofs       Set global Y-offset\n"
	"  -z|--offset-z ofs       Set global Z-offset\n"
	;

enum {
	CMD_GCODE = 1000,
	CMD_GCODE_ASFUNC,
	CMD_GCODE_NOM2,
	CMD_SVG,
	CMD_SVG_TOOLWIDTH,
	CMD_SVG_NO_MOVELAYER,
	CMD_SVG_NO_FLIP,
	CMD_SVG_NO_GRID,
	CMD_SVG_OPACITY,
	CMD_DXF,
	CMD_PEDANTIC,
};

static const struct option lopts[] = {
	{ "bare",		no_argument,		NULL,	'q' },
	{ "debug",		no_argument,		NULL,	'd' },
	{ "define",		required_argument,	NULL,	'D' },
	{ "dxf",		no_argument,		NULL,	CMD_DXF },
	{ "epilogue",		required_argument,	NULL,	'g' },
	{ "error",		no_argument,		NULL,	'W' },
	{ "gcode",		no_argument,		NULL,	CMD_GCODE },
	{ "gcode-function",	required_argument,	NULL,	CMD_GCODE_ASFUNC },
	{ "gcode-nom2",		no_argument,		NULL,	CMD_GCODE_NOM2 },
	{ "help",		no_argument,		NULL,	'h' },
	{ "imperial",		no_argument,		NULL,	'i' },
	{ "include",		required_argument,	NULL,	'I' },
	{ "offset-a",		required_argument,	NULL,	'a' },
	{ "offset-b",		required_argument,	NULL,	'b' },
	{ "offset-c",		required_argument,	NULL,	'c' },
	{ "offset-u",		required_argument,	NULL,	'u' },
	{ "offset-v",		required_argument,	NULL,	'x' },
	{ "offset-w",		required_argument,	NULL,	'x' },
	{ "offset-x",		required_argument,	NULL,	'x' },
	{ "offset-y",		required_argument,	NULL,	'y' },
	{ "offset-z",		required_argument,	NULL,	'z' },
	{ "output",		required_argument,	NULL,	'o' },
	{ "pedantic",		no_argument,		NULL,	CMD_PEDANTIC },
	{ "plane",		required_argument,	NULL,	'p' },
	{ "precision",		required_argument,	NULL,	'P' },
	{ "prologue",		required_argument,	NULL,	'G' },
	{ "relative",		no_argument,		NULL,	'r' },
	{ "svg",		no_argument,		NULL,	CMD_SVG },
	{ "svg-no-movelayer",	no_argument,		NULL,	CMD_SVG_NO_MOVELAYER },
	{ "svg-no-flip",	no_argument,		NULL,	CMD_SVG_NO_FLIP },
	{ "svg-no-grid",	no_argument,		NULL,	CMD_SVG_NO_GRID },
	{ "svg-toolwidth",	required_argument,	NULL,	CMD_SVG_TOOLWIDTH },
	{ "svg-opacity",	required_argument,	NULL,	CMD_SVG_OPACITY },
	{ "uvw",		no_argument,		NULL,	'U' },
	{ "version",		no_argument,		NULL,	'V' },
	{ NULL,			0,			NULL,	 0 },
};

#define MAXOFFSET	1e6	/* 1000000mm is one kilometer, should be enough for a mill... */
#define NOFFSETS	9	/* XYZABCUVW */

static int get_offset(double *offs, const char *str, char ch, unit_et *unit)
{
	char *cptr;
	assert(offs != NULL);
	assert(str != NULL);
	*unit = UNIT_NONE;
	*offs = strtod(str, &cptr);
	if(*cptr) {
		if(!strcmp(cptr, "mm\0")) {
			*unit = UNIT_MM;
		} else if(!strcmp(cptr , "in\0")) {
			*unit = UNIT_IN;
		} else if(!strcmp(cptr , "mil")) {
			*unit = UNIT_IN;
			*offs /= 1000.0;
		} else if(!strcmp(cptr , "deg")) {
			*unit = UNIT_DEG;
		} else if(!strcmp(cptr , "rad")) {
			*unit = UNIT_RAD;
		} else {
			fprintf(stderr, "Invalid global %c-offset\n", ch);
			return 0;
		}
		if(!(strchr("ABC", ch) && (*unit == UNIT_DEG || *unit == UNIT_RAD)) && !(strchr("XYZUVW", ch) && (*unit == UNIT_MM || *unit == UNIT_IN))) {
			fprintf(stderr, "Invalid units for global %c-offset\n", ch);
			return 0;
		}
	}
	if(fabs(*offs) > MAXOFFSET) {
		fprintf(stderr, "Global %c-offset out of range\n", ch);
		return 0;
	}
	return 1;
}

int main(int argc, char *argv[])
{
	int i;
	int unit;
	int anyoffs;
	int debug = 0;
	int lose = 0;
	int optc;
	int oidx = 0;
	char *cptr;
	const char *ofn = NULL;
	char *profn = NULL;
	char *epifn = NULL;
	int retval = 0;
	value_t *v, *w;
	double offsets[NOFFSETS] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
	unit_et offsetunits[NOFFSETS] = {UNIT_NONE, UNIT_NONE, UNIT_NONE, UNIT_NONE, UNIT_NONE, UNIT_NONE, UNIT_NONE, UNIT_NONE, UNIT_NONE};
	int proepi = 1;
	int warnerror = 0;
	char *asfuncname = NULL;

	while(EOF != (optc = getopt_long(argc, argv, "a:b:c:D:dG:g:hI:io:p:P:qrUVWu:v:w:x:y:z:", lopts, &oidx))) {
		switch(optc) {
		case 'a':
			if(!get_offset(&offsets[3], optarg, 'A', &offsetunits[3]))
				lose++;
			break;
		case 'b':
			if(!get_offset(&offsets[4], optarg, 'B', &offsetunits[4]))
				lose++;
			break;
		case 'c':
			if(!get_offset(&offsets[5], optarg, 'C', &offsetunits[5]))
				lose++;
			break;
		case 'D':
			define_add(optarg);
			break;
		case 'd':
			debug = 1;
			break;
		case 'h':
			fprintf(stderr, "%s", usage_str);
			exit(1);
			break;
		case 'G':
			profn = strdup(optarg);
			break;
		case 'g':
			epifn = strdup(optarg);
			break;
		case 'I':
			cptr = strdup(optarg);
			while(*cptr && (cptr[strlen(cptr)-1] == '/' || cptr[strlen(cptr)-1] == '\\'))
				cptr[strlen(cptr)-1] = 0;
			includepath_add(cptr);
			break;
		case 'i':
			cl_inch = 1;
			break;
		case 'o':
			ofn = optarg;
			break;
		case 'p':
			global_plane = strtoul(optarg, &cptr, 10);
			if(*cptr) {
				if(!strcasecmp(optarg, "xy"))
					global_plane = PLANE_XY;
				else if(!strcasecmp(optarg, "xz"))
					global_plane = PLANE_XZ;
				else if(!strcasecmp(optarg, "yz"))
					global_plane = PLANE_YZ;
				else
					goto plane_error;
			} else if(global_plane > 2) {
plane_error:
				fprintf(stderr, "Initial plane must be 0, 1, 2, XY, XZ or YZ\n");
				lose++;
			}
			break;
		case 'P':
			cl_decimals = strtoul(optarg, &cptr, 10);
			if(*cptr) {
				fprintf(stderr, "Invalid number of decimals\n");
				lose++;
			}
			if(cl_decimals < 1 || cl_decimals > 15) {
				fprintf(stderr, "Number of decimals out of range (1..15)\n");
				lose++;
			}
			break;
		case 'q':
			proepi = 0;
			break;
		case 'r':
			cl_relative = 1;
			break;
		case 'x':
			if(!get_offset(&offsets[0], optarg, 'X', &offsetunits[0]))
				lose++;
			break;
		case 'y':
			if(!get_offset(&offsets[1], optarg, 'Y', &offsetunits[1]))
				lose++;
			break;
		case 'z':
			if(!get_offset(&offsets[2], optarg, 'Z', &offsetunits[2]))
				lose++;
			break;
		case 'u':
			if(!get_offset(&offsets[3], optarg, 'U', &offsetunits[6]))
				lose++;
			break;
		case 'v':
			if(!get_offset(&offsets[4], optarg, 'V', &offsetunits[7]))
				lose++;
			break;
		case 'w':
			if(!get_offset(&offsets[5], optarg, 'W', &offsetunits[8]))
				lose++;
			break;
		case 'U':
			naxes = 9;
			break;
		case 'V':
			printf("G-Code Meta Compiler - gcmc version %s\n", PACKAGE_VERSION);
			printf("Distributed under GPLv3+\n");
			exit(0);
			break;
		case 'W':
			warnerror = 1;
			break;
		case CMD_GCODE_ASFUNC:
			asfuncname = strdup(optarg);
			for(i = strlen(optarg); i; i--) {
				int c = optarg[i-1] & 0xff;
				if(!isdigit(c) && !islower(c) && c != '-' && c != '_') {
					fprintf(stderr, "G-Code function name contains invalid characters (allowed: [a-z0-9_-])\n");
					lose++;
					break;
				}
			}
			break;
		case CMD_GCODE_NOM2:
			cl_nom2 = 1;
			break;
		case CMD_GCODE:
		case CMD_SVG:
		case CMD_DXF:
			if(cl_format != FMT_NONE) {
				fprintf(stderr, "Only one output format specifier allowed\n");
				lose++;
			}
			switch(optc) {
			case CMD_GCODE:	cl_format = FMT_GCODE; break;
			case CMD_SVG:	cl_format = FMT_SVG; break;
			case CMD_DXF:	cl_format = FMT_DXF; break;
			}
			break;
		case CMD_SVG_TOOLWIDTH:
			cl_svg_toolwidth = strtod(optarg, &cptr);
			if(*cptr) {
				fprintf(stderr, "SVG toolwidth is invalid\n");
				lose++;
			} else if(cl_svg_toolwidth < 0.001 || cl_svg_toolwidth > 1000.0) {
				fprintf(stderr, "SVG toolwidth should be >= 0.001 and <= 1000.0\n");
				lose++;
			}
			break;
		case CMD_SVG_NO_MOVELAYER:
			cl_svg_movelayer = 0;
			break;
		case CMD_SVG_NO_FLIP:
			cl_svg_flip = 0;
			break;
		case CMD_SVG_NO_GRID:
			cl_svg_grid = 0;
			break;
		case CMD_SVG_OPACITY:
			cl_svg_opacity = strtod(optarg, &cptr);
			if(*cptr) {
				fprintf(stderr, "SVG opacity is invalid\n");
				lose++;
			} else if(cl_svg_opacity < 0.001 || cl_svg_opacity > 1.0) {
				fprintf(stderr, "SVG opacity should be >= 0.001 and <= 1.0\n");
				lose++;
			}
			break;
		case CMD_PEDANTIC:
			cl_pedantic = 1;
			break;
		case '?':
			lose++;
			break;
		default:
			fprintf(stderr, "Unknown option character '%c' (%d) in option handling\n", isprint(optc) ? optc : ' ', optc);
			lose++;
			break;
		}
	}

	if(cl_pedantic && cl_decimals < 3)
		fprintf(stderr, "Number of decimals less than 3 severely limits accuracy\n");

	for(i = anyoffs = 0; i < NOFFSETS; i++)
		anyoffs |= offsets[i] != 0.0;
	if(cl_relative && anyoffs) {
		fprintf(stderr, "Offsets must be zero when using relative addressing\n");
		lose++;
	}

	/* Set SVG defaults */
	if(cl_svg_toolwidth == -1.0)
		cl_svg_toolwidth = cl_inch ? 0.125 : 3.0;
	if(cl_format == FMT_NONE)
		cl_format = FMT_GCODE;

	if(cl_format != FMT_GCODE && (profn || epifn || !proepi)) {
		fprintf(stderr, "Prologue/epilogue cannot be omitted or changed for non G-code backend\n");
		lose++;
	}

	if(cl_format != FMT_GCODE && asfuncname) {
		fprintf(stderr, "G-Code as function only possible for G-code backend\n");
		lose++;
	}

	if(asfuncname && (profn || epifn)) {
		fprintf(stderr, "Prologue/epilogue code cannot be customized when generating G-Code as function\n");
		lose++;
	}

	if((cl_format != FMT_GCODE || !asfuncname) && cl_nom2)
		fprintf(stderr, "Option --gcode-nom2 only has effect when generating gcode a function using --gcode-function\n");

	if(lose)
		exit(1);

	if(asfuncname)
		proepi = 0;	/* Disable std. prologue/epilogue with functions */

	if(debug) {
		setbuf(stdout, NULL);
		setbuf(stderr, NULL);
		/*yydebug = 1;*/
	}

	/* Last search-path entry is current directory */
	includepath_add(strdup(PKGDATADIR));
	includepath_add(strdup("."));

	if(optind >= argc) {
		yyin = stdin;
		filename = strdup("--stdin--");
	} else {
		if(NULL == (yyin = fopen(argv[optind], "r"))) {
			perror(argv[optind]);
			exit(1);
		}
		filename = strdup(argv[optind]);
	}

	if(!ofn) {
		ofp = stdout;
	} else {
		if(NULL == (ofp = fopen(ofn, "w"))) {
			perror(ofn);
			exit(1);
		}
	}

	assert(naxes == 6 || naxes == 9);

	unit = cl_inch ? UNIT_IN : UNIT_MM;

	/* Setup reference to global offset and position track variable */
	v = value_new(VAL_VECTOR);
	for(i = 0; i < naxes; i++) {
		unit_et u = offsetunits[i];
		if(u == UNIT_NONE) {
			if(i >= 3 && i < 6)
				u = UNIT_DEG;
			else
				u = cl_inch ? UNIT_IN : UNIT_MM;
		} else if(u == UNIT_MM && cl_inch) {
			offsets[i] /= 25.4;
			u = UNIT_IN;
		} else if(u == UNIT_IN && !cl_inch) {
			offsets[i] *= 25.4;
			u = UNIT_MM;
		} else if(u == UNIT_RAD) {
			/* Angular offset must be degrees (gcode units) */
			offsets[i] *= 180.0;
			offsets[i] /= M_PI;
			u = UNIT_DEG;
		}
		w = value_new_flt(offsets[i], u);
		value_vector_add(v, w);
		global_offs[i] = &w->d;

		relative_offs[i] = 0.0;		/* Also init relocate() position to zero */
	}
	variable_set(L"__global_offset", v);

	v = value_new(VAL_VECTOR);
	w = value_new_flt(0.0, unit); value_vector_add(v, w); global_pos[0] = &w->d;
	w = value_new_flt(0.0, unit); value_vector_add(v, w); global_pos[1] = &w->d;
	w = value_new_flt(0.0, unit); value_vector_add(v, w); global_pos[2] = &w->d;
	w = value_new_flt(0.0, UNIT_RAD); value_vector_add(v, w); global_pos[3] = &w->d;
	w = value_new_flt(0.0, UNIT_RAD); value_vector_add(v, w); global_pos[4] = &w->d;
	w = value_new_flt(0.0, UNIT_RAD); value_vector_add(v, w); global_pos[5] = &w->d;
	if(naxes > 6) {
		w = value_new_flt(0.0, unit); value_vector_add(v, w); global_pos[6] = &w->d;
		w = value_new_flt(0.0, unit); value_vector_add(v, w); global_pos[7] = &w->d;
		w = value_new_flt(0.0, unit); value_vector_add(v, w); global_pos[8] = &w->d;
	}
	variable_set(L"__global_position", v);

	/* Setup the command-line defines */
	define_setup(definebuf, ndefinebuf, "<cmd-line defines>");

	/* Setup the predefined constants (handled before command-line defines) */
	define_setup(gcmcconsts, ngcmcconsts, "<gcmc-internal constants>");

	/* Parse the input script */
	if(yyparse())
		yyerror("Syntax error");

	if(syntaxerrors)
		exit(1);

	/* Output the G-code */
	if(proepi || profn)
		prologue(profn, ofp);
	if(asfuncname)
		asfunc_head(ofp, asfuncname);
	if((v = execute(scripthead, NULL))) {
		retval = value_to_int(v);
		value_delete(v);
	}
	if(proepi || epifn)
		epilogue(epifn, ofp);
	if(asfuncname)
		asfunc_tail(ofp, asfuncname);

	if(warnerror && runtimewarnings)
		rterror(NULL, "Runtime warnings considered to be errors");

	if(runtimeerrors)
		retval = 1;

	/* Cleanup for testing purposes */
	assert(0 == value_stackdepth());

#ifdef FREEONEXIT
	variable_purge();
	value_purge();
	node_delete(scripthead);
	parser_cleanup();
	lexer_cleanup();
	utils_cleanup();
	interpreter_cleanup();
	includepath_purge();
	free((void *)filename);
#endif
	fclose(ofp);

	return retval;
}
#endif
/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_YY_C_PROJECTS_BBROBOT_BBSCRIPT_GRAMMAR_H_INCLUDED
# define YY_YY_C_PROJECTS_BBROBOT_BBSCRIPT_GRAMMAR_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    FUNCTION = 258,                /* FUNCTION  */
    FOR = 259,                     /* FOR  */
    FOREACH = 260,                 /* FOREACH  */
    DO = 261,                      /* DO  */
    WHILE = 262,                   /* WHILE  */
    IF = 263,                      /* IF  */
    ELIF = 264,                    /* ELIF  */
    ELSE = 265,                    /* ELSE  */
    BREAK = 266,                   /* BREAK  */
    CONTINUE = 267,                /* CONTINUE  */
    RETURN = 268,                  /* RETURN  */
    INCLUDE = 269,                 /* INCLUDE  */
    LOCAL = 270,                   /* LOCAL  */
    REPEAT = 271,                  /* REPEAT  */
    CONST = 272,                   /* CONST  */
    TOPEN = 273,                   /* TOPEN  */
    TCLOSE = 274,                  /* TCLOSE  */
    MM = 275,                      /* MM  */
    MIL = 276,                     /* MIL  */
    IN = 277,                      /* IN  */
    DEG = 278,                     /* DEG  */
    RAD = 279,                     /* RAD  */
    IDENT = 280,                   /* IDENT  */
    STRING = 281,                  /* STRING  */
    NUMBER = 282,                  /* NUMBER  */
    FLOATV = 283,                   /* FLOAT  */
    SUBASSIGN = 284,               /* SUBASSIGN  */
    ADDASSIGN = 285,               /* ADDASSIGN  */
    MULASSIGN = 286,               /* MULASSIGN  */
    DIVASSIGN = 287,               /* DIVASSIGN  */
    MODASSIGN = 288,               /* MODASSIGN  */
    SHLASSIGN = 289,               /* SHLASSIGN  */
    SHRASSIGN = 290,               /* SHRASSIGN  */
    ADDORASSIGN = 291,             /* ADDORASSIGN  */
    SUBORASSIGN = 292,             /* SUBORASSIGN  */
    BORASSIGN = 293,               /* BORASSIGN  */
    BANDASSIGN = 294,              /* BANDASSIGN  */
    BXORASSIGN = 295,              /* BXORASSIGN  */
    LOR = 296,                     /* LOR  */
    LAND = 297,                    /* LAND  */
    EQ = 298,                      /* EQ  */
    NE = 299,                      /* NE  */
    LT = 300,                      /* LT  */
    GT = 301,                      /* GT  */
    LE = 302,                      /* LE  */
    GE = 303,                      /* GE  */
    SHL = 304,                     /* SHL  */
    SHR = 305,                     /* SHR  */
    ADDOR = 306,                   /* ADDOR  */
    SUBOR = 307,                   /* SUBOR  */
    INC = 308,                     /* INC  */
    DEC = 309,                     /* DEC  */
    UPM = 310,                     /* UPM  */
    UID = 311                      /* UID  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 74 "grammary.y"

	wchar_t		*str;
	double		d;
	int		i;
	node_t		*node;

#line 127 "C:/Projects/BBRobot/BBScript/grammar.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;


int yyparse (void);


#endif /* !YY_YY_C_PROJECTS_BBROBOT_BBSCRIPT_GRAMMAR_H_INCLUDED  */

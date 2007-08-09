#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>
#include "parser.h"

void im_codegen (inst_t *);	/* Walk intermediate code tree */
void print_head ();		/* Program prolog */
void print_tail ();		/* Program epilog */
void print_incdec (char, int);	/* Increment/decrement cell */
void print_move (char, int);	/* Move pointer */
void print_input ();		/* Print input command */
void print_loop ();		/* Print loop beginning */
void print_end ();		/* Print loop ending */
void print_output ();		/* Print output command */
void print_indent ();		/* Print current indent level */
void print_ccpy (int, int);	/* Add one cell to another. */
void print_cclr ();		/* Cell clear */

extern int indent;		/* Indentation level */

/* Code strings */
extern char *bfstr_type;	/* Cell type */
extern char *bfstr_htype;	/* Thread type */
extern char *bfstr_ptr;		/* Cell pointer name */
extern char *bfstr_buffer;	/* Buffer name */
extern char *bfstr_get;		/* C code for . */
extern char *bfstr_put;		/* C code for , */
extern char *bfstr_loop;	/* C code for [ */
extern char *bfstr_end;		/* C code for ] */
extern char *bfstr_indent;	/* Indent */
extern char *bfstr_bsize;	/* Buffer size string */
extern char *bfstr_name;	/* Name for error reporting */
extern char *bfstr_memerr;	/* Malloc error string */
extern char *bfstr_bounderr;	/* Bound error string */

/* Options */
extern FILE *bfin;		/* Input stream */
extern FILE *bfout;		/* Output stream */
extern int compile_output;	/* Run compiler */
extern int dynamic_mem;		/* Dynamic memory */
extern int mem_grow_rate;	/* Memory grow rate */
extern int mem_size;		/* Starting memory size */
extern int check_bounds;	/* Runtime bounds checking */
extern int dump_core;		/* Dump memory core */
extern int bfbignum;		/* Bignum mode */
extern int optimize_c;		/* Run compiler optimizer */
extern int optimize;		/* Run optimization */
extern int pass_comments;	/* Pass comments to output */
extern int bfthreads;		/* Enable threading. */

#endif

#ifndef PARSER_H
#define PARSER_H

/* Intermediate code structure. */
typedef struct inst_t
{
  int inst;
  int dst;
  int src;
  int lineno;
  char *comment;
  struct inst_t *loop;
  struct inst_t *ret;
  struct inst_t *next;
} inst_t;

/* Intermediate instruction codes */
#define IM_NOP   0		/* No operation */
#define IM_CINC  1		/* Cell increment */
#define IM_CDEC  2		/* Cell decrement */
#define IM_IN    3		/* Input */
#define IM_OUT   4		/* Output */
#define IM_PRGHT 5		/* Move pointer right */
#define IM_PLEFT 6		/* Move pointer left */
#define IM_CADD  8		/* Cell adding */
#define IM_CCLR  9		/* Clear cell */

extern inst_t *head;		/* First instruction */
extern inst_t *tail;		/* Last instruction */

#include "codegen.h"

char bfscan ();
void bfparse (char c);

/* Optimization */
void im_opt (inst_t * head);
inst_t *loop_add_opt (inst_t * loopinst);

extern int lineno;

#endif

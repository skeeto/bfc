#include "common.h"
#include "parser.h"
#include "codegen.h"

#include <string.h>

int lineno;			/* Current scanner line number. */

inst_t *head = NULL;		/* First instruction */
inst_t *tail = NULL;		/* Last instruction */

char *com_buf;
char *com_ptr;
int com_buf_size = 0;

/* Create new instruction. */
inst_t *im_create (inst_t * inst);

/* Parser function */
void bfparse (char c)
{
  /* Mode is used to optimize the pointer operations +-<> */
  static char mode = 0;
  static int mode_count = 0;
  static int loopnow = 0;
  static int nextloop = 0;
  int no_inst;

  /* Initialize */
  if (head == NULL)
    {
      inst_t newhead;
      newhead.inst = IM_NOP;
      newhead.ret = NULL;
      head = im_create (&newhead);
      tail = head;
    }

  /* Modes */
  no_inst = 0;
  loopnow = nextloop;
  nextloop = 0;

  inst_t cinst;			/* Current instruction. */
  cinst.inst = IM_NOP;
  cinst.ret = tail->ret;

  if (c == mode)
    mode_count++;
  else
    {
      /* End current mode */
      if (mode)
	{
	  switch (mode)
	    {
	    case '+':
	      cinst.inst = IM_CINC;
	      cinst.src = mode_count;
	      cinst.ret = tail->ret;
	      break;
	    case '-':
	      cinst.inst = IM_CDEC;
	      cinst.src = mode_count;
	      cinst.ret = tail->ret;
	      break;
	    case '<':
	      cinst.inst = IM_PLEFT;
	      cinst.src = mode_count;
	      cinst.ret = tail->ret;
	      break;
	    case '>':
	      cinst.inst = IM_PRGHT;
	      cinst.src = mode_count;
	      cinst.ret = tail->ret;
	      break;
	    }
	  mode = -1;

	  /* Add this instruction. */
	  if (!loopnow)
	    {
	      tail->next = im_create (&cinst);
	      tail = tail->next;
	    }
	  else
	    {
	      inst_t *old_tail = tail;
	      tail->loop = im_create (&cinst);
	      tail = tail->loop;
	      tail->ret = old_tail;
	    }

	  /* Reset */
	  loopnow = 0;
	  nextloop = 0;
	  cinst.inst = IM_NOP;
	  cinst.ret = NULL;
	}

      /* Parse new character outside of mode */
      switch (c)
	{
	case '+':
	case '-':
	case '<':
	case '>':
	  mode = c;
	  mode_count = 1;
	  no_inst = 1;
	  break;
	case ',':
	  cinst.inst = IM_IN;
	  cinst.ret = tail->ret;
	  break;
	case '.':
	  cinst.inst = IM_OUT;
	  cinst.ret = tail->ret;
	  break;
	case '[':
	  cinst.inst = IM_NOP;
	  cinst.ret = tail->ret;
	  nextloop = 1;
	  indent++;
	  break;
	case ']':
	  tail = tail->ret;
	  no_inst = 1;
	  indent--;
	  break;
	default:
	  /* Control reaches here on last run */
	  no_inst = 1;
	  break;
	}
    }

  if (no_inst)
    {
      nextloop = loopnow;
      return;
    }

  if (!loopnow)
    {
      tail->next = im_create (&cinst);
      tail = tail->next;
    }
  else
    {
      inst_t *old_tail = tail;
      tail->loop = im_create (&cinst);
      tail = tail->loop;
      tail->ret = old_tail;
    }
}

/* Read in only valid BF characters +-<>,.[] */
char bfscan ()
{
  static int lineinc = 0;
  char c = 0;

  while (c == 0 && !feof (bfin))
    {
      c = getc (bfin);
      switch (c)
	{
	case '+':
	case '-':
	case '<':
	case '>':
	case '.':
	case ',':
	case '[':
	case ']':
	  lineno += lineinc;
	  lineinc = 0;
	  break;
	case 10:		/* newline */
	  lineinc = 1;
	default:
	  /* Build comment string */
	  if (com_ptr - com_buf != 0 || c > 32)
	    {
	      if (com_buf_size == 0)
		{
		  com_buf_size = 1024;
		  com_buf = (char *) bfmalloc (com_buf_size);
		  com_ptr = com_buf;
		}
	      if (com_ptr - com_buf + 2 >= com_buf_size)
		{
		  size_t offset = com_ptr - com_buf;
		  com_buf_size *= 2;
		  com_buf = (char *) realloc ((void *) com_buf, com_buf_size);
		  com_ptr = com_buf + offset;
		}
	      *com_ptr = c;
	      com_ptr++;
	      *com_ptr = 0;
	    }
	  c = 0;
	  break;
	}
    }
  return c;
}

inst_t *im_create (inst_t * inst)
{
  /* Copy it */
  inst_t *newinst = (inst_t *) bfmalloc (sizeof (inst_t));
  memcpy (newinst, inst, sizeof (inst_t));
  newinst->loop = NULL;
  newinst->next = NULL;
  newinst->lineno = lineno;
  if (com_ptr - com_buf > 0)
    {
      /* Filter comment */
      char *i;
      for (i = com_ptr; i > com_buf; i--)
	{
	  if (*i <= 32)
	    *i = 0;
	  else
	    break;
	}

      newinst->comment = strdup (com_buf);
      com_ptr = com_buf;
    }
  else
    newinst->comment = NULL;

  return newinst;
}

/* Optimize the intermediate code */
void im_opt (inst_t * head)
{
  inst_t *ii = head;

  while (ii != NULL)
    {
      if (ii->loop)
	{
	  /* Run optmization on loop. */
	  inst_t *rep;
	  rep = loop_add_opt (ii->loop);
	  if (rep)
	    {
	      ii->next = rep;
	      ii->loop = NULL;
	    }
	}

      ii = ii->next;
    }
}

/* Find special "copy" loops and unwrap them. */
inst_t *loop_add_opt (inst_t * loopinst)
{
  inst_t newinst;
  inst_t *head, *tail;
  head = (inst_t *) bfmalloc (sizeof (inst_t));
  head->next = NULL;
  tail = head;

  /* Is loop balanced? */
  int bal = 0, badloop = 0;
  inst_t *ii = loopinst;
  while (ii != NULL)
    {
      if (ii->loop != NULL)
	{
	  badloop = 1;

	  /* This is not the innermost loop. */
	  inst_t *optinst = loop_add_opt (ii->loop);
	  if (optinst != NULL)
	    {
	      ii->loop = NULL;
	      ii->next = optinst;
	    }
	}

      switch (ii->inst)
	{
	case IM_PRGHT:
	  bal += ii->src;
	  break;

	case IM_PLEFT:
	  bal -= ii->src;
	  break;

	case IM_CINC:
	  if (bal == 0)
	    badloop = 1;
	  else
	    {
	      int i;
	      for (i = 0; i < ii->src; i++)
		{
		  newinst.inst = IM_CADD;
		  newinst.ret = loopinst->ret->ret;
		  newinst.dst = bal;
		  newinst.src = 0;
		  lineno = ii->lineno;
		  tail->next = im_create (&newinst);
		  tail = tail->next;
		}
	    }
	  break;

	case IM_CDEC:
	  if (bal != 0)
	    badloop = 1;
	  break;

	case IM_NOP:
	  break;

	default:
	  /* Extra instruction type, loop no good. */
	  badloop = 1;
	  break;
	}

      ii = ii->next;
    }

  if (badloop)
    return NULL;

  if (bal != 0)
    {
      /* Unbalanced */
      return NULL;
    }

  /* Clear and link */
  newinst.inst = IM_CCLR;
  newinst.ret = loopinst->ret->ret;
  tail->next = im_create (&newinst);
  tail = tail->next;
  tail->next = loopinst->ret->next;
  if (tail->next == NULL)
    tail->next = loopinst->ret->ret;

  return head->next;
}

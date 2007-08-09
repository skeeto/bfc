#include <stdio.h>
#include "codegen.h"
#include "parser.h"

/* Options */
FILE *bfin;
FILE *bfout;
int compile_output = 0;
int dynamic_mem = 1;
int mem_grow_rate = 2;
int mem_size = 30000;
int check_bounds = 0;
int dump_core = 0;
int bfbignum = 0;
int optimize_c = 0;
int optimize = 1;
int pass_comments = 0;
int bfthreads = 0;

/* Code strings */
char *bfstr_type = "unsigned char";
char *bfstr_htype = NULL;
char *bfstr_ptr = "ptr";
char *bfstr_buffer = "bf_buffer";
char *bfstr_get = "*%s = (BFTYPE) getchar ();\n";
char *bfstr_put = "putchar ((char) *%s);\n";
char *bfstr_loop = "while (*%s) {\n";
char *bfstr_end = "}\n";
char *bfstr_indent = "  ";
char *bfstr_bsize = "bf_bsize";
char *bfstr_name = "brainfuck";
char *bfstr_memerr = "out of memory";
char *bfstr_bounderr = "pointer out of bounds";

int indent = 1;

/* Walk through intermediate tree and generate code. */
void im_codegen (inst_t * head)
{
  /* Handle threads */
  static int thread_cnt = 0;
  if (bfthreads)
    {
      fprintf (bfout, "void *bf%d (void *x) {\n", thread_cnt);
      fprintf (bfout, "  int ptri = %d;\n\n", thread_cnt);
      thread_cnt++;
    }

  indent = 1;

  int returned = 0;

  inst_t *inst = head;
  while (inst != NULL)
    {
      lineno = inst->lineno;
      if (inst->comment && pass_comments)
	{
	  fprintf (bfout, "/*\n %s \n*/\n", inst->comment);
	}

      switch (inst->inst * !returned)
	{
	case IM_CINC:		/* Cell increment */
	  print_incdec ('+', inst->src);
	  break;

	case IM_CDEC:		/* Cell decrement */
	  print_incdec ('-', inst->src);
	  break;

	case IM_PRGHT:		/* Move pointer right */
	  print_move ('>', inst->src);
	  break;

	case IM_PLEFT:		/* Move pointer left */
	  print_move ('<', inst->src);
	  break;

	case IM_IN:		/* Input */
	  print_input ();
	  break;

	case IM_OUT:		/* Output */
	  print_output ();
	  break;

	case IM_CADD:		/* Cell copy */
	  print_ccpy (inst->dst, inst->src);
	  break;

	case IM_CCLR:		/* Cell clear */
	  print_cclr ();
	  break;
	}

      /* Select next instruction. First from loop, then from next,
         then from ret. */
      if (inst->loop && !returned)
	{
	  print_loop ();
	  inst = inst->loop;
	  returned = 0;
	  indent++;
	}
      else if (inst->next)
	{
	  inst = inst->next;
	  returned = 0;
	}
      else if (inst->ret)
	{
	  print_end ();
	  inst = inst->ret;
	  returned = 1;
	  indent--;
	}
      else
	inst = NULL;
    }

  if (bfthreads)
    {
      fprintf (bfout, "\n");
      fprintf (bfout, "  pthread_exit (NULL);\n");
      fprintf (bfout, "} /* thread %d */\n\n", thread_cnt - 1);
    }
}

/* Print the top of the C file */
void print_head ()
{
  fprintf (bfout, "#include <stdio.h>\n");
  fprintf (bfout, "#include <stdlib.h>\n");
  fprintf (bfout, "#include <string.h>\n");
  if (dump_core)
    fprintf (bfout, "#include <signal.h>\n");
  if (bfbignum)
    fprintf (bfout, "#include <gmp.h>\n");
  if (bfthreads)
    fprintf (bfout, "#include <pthread.h>\n");
  fprintf (bfout, "\n");

  /* Type define */
  fprintf (bfout, "#define BFTYPE %s\n\n", bfstr_type);

  /* Thread struct */
  if (bfthreads)
    {
      fprintf (bfout, "typedef struct BFTYPE {\n");
      fprintf (bfout, "  %s val;\n", bfstr_htype);
      fprintf (bfout, "  pthread_mutex_t lock;\n");
      fprintf (bfout, "} BFTYPE;\n\n");
      fprintf (bfout, "BFTYPE *hptr[%d]; /* Thread pointers */\n\n",
	       bfthreads);
    }

  /* resize prototype */
  if (dynamic_mem)
    fprintf (bfout, "void bf_buffinc (BFTYPE **ptr);\n\n");

  /* Main memory */
  char *bfinit = " = { 0 }";
  if (bfbignum)
    bfinit = "";
  if (dynamic_mem)
    fprintf (bfout, "BFTYPE *%s;\n\n", bfstr_buffer);
  else
    fprintf (bfout, "BFTYPE %s[%d]%s;\n\n", bfstr_buffer, mem_size, bfinit);
  if (bfthreads && dynamic_mem)
    fprintf (bfout, "pthread_mutex_t mem_lock;\n\n");

  /* Track buffer size */
  if (dynamic_mem || check_bounds || dump_core)
    fprintf (bfout, "int %s = %d; /* Buffer size */\n\n",
	     bfstr_bsize, mem_size);

  /* bignum init */
  if (bfbignum)
    {
      char *thread_str;
      if (bfthreads)
	thread_str = ".val";
      else
	thread_str = "";
      fprintf (bfout, "/* Initialize bignum values */\n");
      fprintf (bfout, "void bignum_init (BFTYPE *buff, int n) {\n");
      fprintf (bfout, "  int i;\n");
      fprintf (bfout, "  for (i = 0; i < n; i++)\n");
      fprintf (bfout, "    mpz_init (buff[i]%s);\n", thread_str);
      fprintf (bfout, "}\n\n");
    }

  /* mutex */
  if (bfthreads)
    {
      /* init */
      fprintf (bfout, "/* Initialize mutex locks */\n");
      fprintf (bfout, "void mutex_init (BFTYPE *buff, int n) {\n");
      fprintf (bfout, "  int i;\n");
      fprintf (bfout, "  for (i = 0; i < n; i++) {\n");
      fprintf (bfout, "    pthread_mutex_init (&buff[i].lock, NULL);\n");
      if (bfbignum)
	fprintf (bfout, "    mpz_init (buff[i].val);\n");
      else
	fprintf (bfout, "    buff[i].val = 0;\n");
      fprintf (bfout, "  }\n");
      fprintf (bfout, "}\n\n");

      /* inc */
      fprintf (bfout, "void cell_inc (int i, int n) {\n");
      if (dynamic_mem)
	{
	  fprintf (bfout, "  pthread_mutex_lock (&mem_lock);\n");
	  fprintf (bfout, "  BFTYPE *ptr = hptr[i];\n");
	}
      else
	{
	  fprintf (bfout, "  BFTYPE *ptr = hptr[i];\n");
	  fprintf (bfout, "  pthread_mutex_lock (&ptr->lock);\n");
	}
      if (bfbignum)
	{
	  fprintf (bfout, "  mpz_add_ui (ptr->val, ptr->val, n);\n");
	}
      else
	{
	  fprintf (bfout, "  ptr->val += n;\n");
	}
      if (dynamic_mem)
	fprintf (bfout, "  pthread_mutex_unlock (&mem_lock);\n");
      else
	fprintf (bfout, "  pthread_mutex_unlock (&ptr->lock);\n");
      fprintf (bfout, "}\n\n");

      /* dec */
      fprintf (bfout, "void cell_dec (int i, int n) {\n");
      if (dynamic_mem)
	{
	  fprintf (bfout, "  pthread_mutex_lock (&mem_lock);\n");
	  fprintf (bfout, "  BFTYPE *ptr = hptr[i];\n");
	}
      else
	{
	  fprintf (bfout, "  BFTYPE *ptr = hptr[i];\n");
	  fprintf (bfout, "  pthread_mutex_lock (&ptr->lock);\n");
	}
      if (bfbignum)
	{
	  fprintf (bfout, "  mpz_sub_ui (ptr->val, ptr->val, n);\n");
	}
      else
	{
	  fprintf (bfout, "  ptr->val -= n;\n");
	}
      if (dynamic_mem)
	fprintf (bfout, "  pthread_mutex_unlock (&mem_lock);\n");
      else
	fprintf (bfout, "  pthread_mutex_unlock (&ptr->lock);\n");
      fprintf (bfout, "}\n\n");

      /* get conditional */
      fprintf (bfout, "int cell_cond (int i) {\n");
      if (dynamic_mem)
	{
	  fprintf (bfout, "  pthread_mutex_lock (&mem_lock);\n");
	  fprintf (bfout, "  BFTYPE *ptr = hptr[i];\n");
	}
      else
	{
	  fprintf (bfout, "  BFTYPE *ptr = hptr[i];\n");
	  fprintf (bfout, "  pthread_mutex_lock (&ptr->lock);\n");
	}
      if (bfbignum)
	{
	  fprintf (bfout, "  int out = !mpz_fits_sint_p (ptr->val) ||"
		   " mpz_get_ui (ptr->val) != 0;\n");
	}
      else
	{
	  fprintf (bfout, "  int out = ptr->val;\n");
	}
      if (dynamic_mem)
	fprintf (bfout, "  pthread_mutex_unlock (&mem_lock);\n");
      else
	fprintf (bfout, "  pthread_mutex_unlock (&ptr->lock);\n");
      fprintf (bfout, "  return out;\n");
      fprintf (bfout, "}\n\n");

      /* get */
      fprintf (bfout, "char cell_get (int i) {\n");
      if (dynamic_mem)
	{
	  fprintf (bfout, "  pthread_mutex_lock (&mem_lock);\n");
	  fprintf (bfout, "  BFTYPE *ptr = hptr[i];\n");
	}
      else
	{
	  fprintf (bfout, "  BFTYPE *ptr = hptr[i];\n");
	  fprintf (bfout, "  pthread_mutex_lock (&ptr->lock);\n");
	}
      if (bfbignum)
	fprintf (bfout, "  char out = (char) mpz_get_ui (ptr->val);\n");
      else
	fprintf (bfout, "  char out = (char) ptr->val;\n");
      if (dynamic_mem)
	fprintf (bfout, "  pthread_mutex_unlock (&mem_lock);\n");
      else
	fprintf (bfout, "  pthread_mutex_unlock (&ptr->lock);\n");
      fprintf (bfout, "  return out;\n");
      fprintf (bfout, "}\n\n");

      /* set */
      fprintf (bfout, "void cell_set (int i, char in) {\n");
      if (dynamic_mem)
	{
	  fprintf (bfout, "  pthread_mutex_lock (&mem_lock);\n");
	  fprintf (bfout, "  BFTYPE *ptr = hptr[i];\n");
	}
      else
	{
	  fprintf (bfout, "  BFTYPE *ptr = hptr[i];\n");
	  fprintf (bfout, "  pthread_mutex_lock (&ptr->lock);\n");
	}
      if (bfbignum)
	fprintf (bfout, "  mpz_set_ui "
		 "(ptr->val, (unsigned long int) in);\n");
      else
	fprintf (bfout, "  ptr->val = in;\n");
      if (dynamic_mem)
	fprintf (bfout, "  pthread_mutex_unlock (&mem_lock);\n");
      else
	fprintf (bfout, "  pthread_mutex_unlock (&ptr->lock);\n");
      fprintf (bfout, "}\n\n");

      /* move */
      fprintf (bfout, "void cell_move (int i, int m) {\n");
      if (dynamic_mem)
	{
	  fprintf (bfout, "  pthread_mutex_lock (&mem_lock);\n");
	  fprintf (bfout, "  BFTYPE *ptr = hptr[i];\n");
	}
      else
	{
	  fprintf (bfout, "  BFTYPE *ptr = hptr[i];\n");
	  fprintf (bfout, "  pthread_mutex_lock (&ptr->lock);\n");
	}
      fprintf (bfout, "  hptr[i] += m;\n");
      fprintf (bfout, "  int oob = hptr[i] - %s >= %s;\n",
	       bfstr_buffer, bfstr_bsize);
      if (dynamic_mem)
	fprintf (bfout, "  pthread_mutex_unlock (&mem_lock);\n");
      else
	fprintf (bfout, "  pthread_mutex_unlock (&ptr->lock);\n");
      fprintf (bfout, "  if (oob)\n");
      fprintf (bfout, "    bf_buffinc (&%s);\n", bfstr_ptr);
      fprintf (bfout, "}\n\n");

    }

  /* Thread function prototypes */
  if (bfthreads)
    {
      fprintf (bfout, "/* Thread functions */\n");
      int i;
      for (i = 0; i < bfthreads; i++)
	fprintf (bfout, "void *bf%d (void *);\n", i);
      fprintf (bfout, "\n");
    }

  /* core dumps */
  if (dump_core)
    {
      char *thread_str;
      if (bfthreads)
	thread_str = ".val";
      else
	thread_str = "";

      fprintf (bfout, "/* Dump the memory core */\n");
      fprintf (bfout, "void dump_core (BFTYPE *buff, int n) {\n");
      fprintf (bfout, "  FILE *fp = fopen (\"bf-core\", \"w\");\n");
      fprintf (bfout, "  if (fp == NULL)\n");
      fprintf (bfout, "    return;\n\n");
      fprintf (bfout, "  int i;\n");
      if (bfbignum)
	{
	  fprintf (bfout, "  char *numstr;\n");
	  fprintf (bfout, "  for (i = 0; i < n; i++) {\n");
	  fprintf (bfout, "    numstr = mpz_get_str(NULL, 10, buff[i]%s);\n",
		   thread_str);
	  fprintf (bfout, "    fprintf (fp, \"%%s\\n\", numstr);\n");
	  fprintf (bfout, "    free (numstr);\n");
	  fprintf (bfout, "  }\n");
	}
      else
	{
	  fprintf (bfout, "  for (i = 0; i < n; i++)\n");
	  fprintf (bfout,
		   "    fprintf (fp, \"%%d\\n\", (int) buff[i]%s);\n\n",
		   thread_str);
	}
      fprintf (bfout, "  fclose (fp);");
      fprintf (bfout, "}\n\n");

      /* Signal handler */
      fprintf (bfout, "void int_handle (int sig) {\n");
      fprintf (bfout, "  dump_core (%s, %s);\n", bfstr_buffer, bfstr_bsize);
      fprintf (bfout, "  exit (EXIT_SUCCESS);");
      fprintf (bfout, "}\n\n");
    }

  if (dynamic_mem)
    {
      /* Print memory resize function */
      fprintf (bfout, "/* Resize memory */\n");
      fprintf (bfout, "void bf_buffinc (BFTYPE **ptr) {\n");
      if (bfthreads)
	{
	  fprintf (bfout, "  pthread_mutex_lock (&mem_lock);\n");
	  fprintf (bfout, "  int offsets[%d];\n", bfthreads);
	  int i;
	  for (i = 0; i < bfthreads; i++)
	    fprintf (bfout, "  offsets[%d] = hptr[%d] - %s;\n",
		     i, i, bfstr_buffer);
	}
      fprintf (bfout, "  int offset = *ptr - %s;\n\n", bfstr_buffer);
      fprintf (bfout, "  int old_bsize = %s;\n", bfstr_bsize);
      fprintf (bfout, "  %s *= %d;\n", bfstr_bsize, mem_grow_rate);
      fprintf (bfout, "  if (offset > %s)\n", bfstr_bsize);
      fprintf (bfout, "    %s = offset;\n\n", bfstr_bsize);

      fprintf (bfout, "  %s = (BFTYPE *) realloc ((void *) %s, "
	       "%s * sizeof (BFTYPE));\n",
	       bfstr_buffer, bfstr_buffer, bfstr_bsize);
      fprintf (bfout, "  if (!%s) {\n", bfstr_buffer);
      fprintf (bfout, "    fprintf (stderr, \"%s:%d:%s\\n\");\n",
	       bfstr_name, lineno, bfstr_memerr);
      fprintf (bfout, "    abort ();\n  }\n\n");

      /* clear */
      if (bfbignum && !bfthreads)
	{
	  fprintf (bfout, "  bignum_init ((%s + old_bsize), "
		   "%s - old_bsize);\n", bfstr_buffer, bfstr_bsize);
	  fprintf (bfout, "  *ptr = %s + offset;\n", bfstr_buffer);
	}
      else if (bfthreads)
	{
	  fprintf (bfout, "  mutex_init ((%s + old_bsize), "
		   "%s - old_bsize);\n", bfstr_buffer, bfstr_bsize);
	  int i;
	  for (i = 0; i < bfthreads; i++)
	    fprintf (bfout, "  hptr[%d] = %s + offsets[%d];\n",
		     i, bfstr_buffer, i);
	  fprintf (bfout, "  pthread_mutex_unlock (&mem_lock);\n");
	}
      else
	{
	  fprintf (bfout, "  memset ((%s + old_bsize), 0, "
		   "(%s - old_bsize) * sizeof (BFTYPE));\n",
		   bfstr_buffer, bfstr_bsize);
	  fprintf (bfout, "  *ptr = %s + offset;\n", bfstr_buffer);
	}
      fprintf (bfout, "}\n\n");

      /* main */
      fprintf (bfout, "int main () {\n");
      fprintf (bfout, "  %s = calloc (%s * sizeof (BFTYPE), 1);\n\n",
	       bfstr_buffer, bfstr_bsize);
      if (!bfthreads)
	fprintf (bfout, "  BFTYPE *%s = %s;\n\n", bfstr_ptr, bfstr_buffer);
    }
  else
    {
      /* main */
      fprintf (bfout, "int main () {\n");
      if (!bfthreads)
	fprintf (bfout, "  BFTYPE *%s = %s;\n\n", bfstr_ptr, bfstr_buffer);
    }

  if (bfbignum && !bfthreads)
    {
      fprintf (bfout, "  bignum_init (%s, %d);\n\n", bfstr_buffer, mem_size);
    }

  if (dump_core)
    fprintf (bfout, "  signal (SIGINT, int_handle);\n\n");

  /* Spawn threads. */
  if (bfthreads)
    {
      if (dynamic_mem)
	fprintf (bfout, "  pthread_mutex_init (&mem_lock, NULL);\n");
      else
	fprintf (bfout, "  mutex_init (%s, %d);\n\n", bfstr_buffer, mem_size);

      /* Initialize pointers */
      int i;
      for (i = 0; i < bfthreads; i++)
	fprintf (bfout, "  hptr[%d] = %s;\n", i, bfstr_buffer);
      fprintf (bfout, "\n");


      /* Create */
      fprintf (bfout, "  pthread_t bfthread[%d];\n", bfthreads);
      for (i = 0; i < bfthreads; i++)
	fprintf (bfout, "  pthread_create "
		 "(&bfthread[%d], NULL, bf%d, NULL);\n", i, i);
      fprintf (bfout, "\n");

      /* Join */
      for (i = 0; i < bfthreads; i++)
	fprintf (bfout, "  pthread_join " "(bfthread[%d], NULL);\n", i);
      fprintf (bfout, "\n");

      print_tail ();
      fprintf (bfout, "\n");
    }
}

/* Print the bottom of the C file */
void print_tail ()
{
  /* Blank line */
  print_indent ();
  fprintf (bfout, "\n");

  if (dump_core)
    {
      print_indent ();
      fprintf (bfout, "dump_core (%s, %s);\n", bfstr_buffer, bfstr_bsize);
    }

  print_indent ();
  fprintf (bfout, "exit (EXIT_SUCCESS);\n");
  fprintf (bfout, "}\n");
}

/* Print increment instruction(s) */
void print_incdec (char c, int n)
{
  if (bfthreads)
    {
      print_indent ();
      if (c == '+')
	fprintf (bfout, "cell_inc (ptri, %d);\n", n);
      else
	fprintf (bfout, "cell_dec (ptri, %d);\n", n);
      return;
    }

  if (bfbignum)
    {
      char *addsub;
      if (c == '-')
	addsub = "sub";
      else
	addsub = "add";

      print_indent ();
      fprintf (bfout, "mpz_%s_ui (*%s, *%s, %d);\n",
	       addsub, bfstr_ptr, bfstr_ptr, n);
    }
  else
    {
      print_indent ();
      fprintf (bfout, "*%s %c= %d;\n", bfstr_ptr, c, n);
    }
}

/* Print pointer move instruction(s) */
void print_move (char c, int n)
{
  if (c == '>')
    c = '+';
  else
    c = '-';

  if (bfthreads)
    {
      print_indent ();
      fprintf (bfout, "cell_move (ptri, %c%d);\n", c, n);
      return;
    }

  if (dynamic_mem)
    {
      print_indent ();
      fprintf (bfout, "%s %c= %d;\n", bfstr_ptr, c, n);

      /* Check against upper bound */
      if (c == '+')
	{
	  print_indent ();
	  fprintf (bfout, "if (%s - %s >= %s)\n",
		   bfstr_ptr, bfstr_buffer, bfstr_bsize);
	  print_indent ();
	  fprintf (bfout, "  bf_buffinc (&%s);\n", bfstr_ptr);
	}
    }
  else
    {
      print_indent ();
      fprintf (bfout, "%s %c= %d;\n", bfstr_ptr, c, n);

      /* Check against upper bound */
      if (check_bounds && c == '+')
	{
	  print_indent ();
	  fprintf (bfout, "if (%s - %s >= %s) {\n",
		   bfstr_ptr, bfstr_buffer, bfstr_bsize);
	  print_indent ();
	  fprintf (bfout, "  fprintf (stderr, \"%s:%d:%s\\n\");",
		   bfstr_name, lineno, bfstr_bounderr);
	  print_indent ();
	  fprintf (bfout, "  abort ();");
	  print_indent ();
	  fprintf (bfout, "}");
	}
    }

  /* Check lower bounds */
  if (check_bounds && c == '-')
    {
      print_indent ();
      fprintf (bfout, "if (%s < %s) {\n", bfstr_ptr, bfstr_buffer);
      print_indent ();
      fprintf (bfout, "  fprintf (stderr, \"%s:%d:%s\\n\");\n",
	       bfstr_name, lineno, bfstr_bounderr);
      print_indent ();
      fprintf (bfout, "  abort ();");
      print_indent ();
      fprintf (bfout, "}");
    }
}

/* Print input code */
void print_input ()
{
  print_indent ();
  if (!bfthreads)
    fprintf (bfout, bfstr_get, bfstr_ptr);
  else
    fprintf (bfout, "cell_set (ptri, getchar ());\n");
}

/* Print output code */
void print_output ()
{
  print_indent ();
  if (!bfthreads)
    fprintf (bfout, bfstr_put, bfstr_ptr);
  else
    fprintf (bfout, "putchar (cell_get (ptri));\n");
}

/* Print loop beginning */
void print_loop ()
{
  print_indent ();
  if (bfbignum && !bfthreads)
    fprintf (bfout, bfstr_loop, bfstr_ptr, bfstr_ptr);
  else if (bfthreads)
    fprintf (bfout, "while (cell_cond (ptri)) {\n");
  else
    fprintf (bfout, bfstr_loop, bfstr_ptr);

  indent++;
}

/* Print loop ending */
void print_end ()
{
  print_indent ();
  fprintf (bfout, bfstr_end);
  indent--;
}

/* Print current indent level */
void print_indent ()
{
  int i;
  for (i = 0; i < indent; i++)
    {
      fprintf (bfout, bfstr_indent);
    }
}

void print_ccpy (int dst, int src)
{
  if (bfthreads)
    {
      if (dynamic_mem)
	fprintf (bfout, "  pthread_mutex_lock (&mem_lock);\n");
      else
	fprintf (bfout, "  pthread_mutex_lock (&ptr->lock);\n");
    }

  if (check_bounds)
    {
      /* Check lower bounds */
      if (dst < 0)
	{
	  print_indent ();
	  fprintf (bfout, "if (%s + %d < %s) {\n",
		   bfstr_ptr, dst, bfstr_buffer);
	  print_indent ();
	  fprintf (bfout, "  fprintf (stderr, \"%s:%d:%s\\n\");\n",
		   bfstr_name, lineno, bfstr_bounderr);
	  print_indent ();
	  fprintf (bfout, "  abort ();");
	  print_indent ();
	  fprintf (bfout, "}");
	}
      if (src < 0)
	{
	  print_indent ();
	  fprintf (bfout, "if (%s + %d < %s) {\n",
		   bfstr_ptr, src, bfstr_buffer);
	  print_indent ();
	  fprintf (bfout, "  fprintf (stderr, \"%s:%d:%s\\n\");\n",
		   bfstr_name, lineno, bfstr_bounderr);
	  print_indent ();
	  fprintf (bfout, "  abort ();");
	  print_indent ();
	  fprintf (bfout, "}");
	}

      /* Check upper bounds */
      if (dst > 0)
	{
	  print_indent ();
	  fprintf (bfout, "if (%s + %d > %s) {\n",
		   bfstr_ptr, dst, bfstr_buffer);
	  print_indent ();
	  fprintf (bfout, "  fprintf (stderr, \"%s:%d:%s\\n\");\n",
		   bfstr_name, lineno, bfstr_bounderr);
	  print_indent ();
	  fprintf (bfout, "  abort ();");
	  print_indent ();
	  fprintf (bfout, "}");
	}
      if (src > 0)
	{
	  print_indent ();
	  fprintf (bfout, "if (%s + %d > %s) {\n",
		   bfstr_ptr, src, bfstr_buffer);
	  print_indent ();
	  fprintf (bfout, "  fprintf (stderr, \"%s:%d:%s\\n\");\n",
		   bfstr_name, lineno, bfstr_bounderr);
	  print_indent ();
	  fprintf (bfout, "  abort ();");
	  print_indent ();
	  fprintf (bfout, "}");
	}
    }

  if (!bfbignum)
    {
      print_indent ();
      fprintf (bfout, "*(%s + %d) = *(%s + %d) + *(%s + %d);\n",
	       bfstr_ptr, dst, bfstr_ptr, dst, bfstr_ptr, src);
    }
  else
    {
      print_indent ();
      fprintf (bfout, "mpz_add (*(%s + %d), *(%s + %d), *(%s + %d));\n",
	       bfstr_ptr, dst, bfstr_ptr, dst, bfstr_ptr, src);
    }

  if (bfthreads)
    {
      if (dynamic_mem)
	fprintf (bfout, "  pthread_mutex_unlock (&mem_lock);\n");
      else
	fprintf (bfout, "  pthread_mutex_unlock (&ptr->lock);\n");
    }
}

void print_cclr ()
{
  print_indent ();
  if (bfbignum && !bfthreads)
    fprintf (bfout, "mpz_set_ui (*%s, 0);\n", bfstr_ptr);
  else if (bfthreads)
    fprintf (bfout, "cell_set (ptri, 0);\n");
  else
    fprintf (bfout, "*%s = 0;\n", bfstr_ptr);
}

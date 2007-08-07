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

/* Code strings */
char *bfstr_type = "unsigned char";
char *bfstr_ptr = "ptr";
char *bfstr_buffer = "bf_buffer";
char *bfstr_get = "*%s = (%s) getchar ();\n";
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
  fprintf (bfout, "\n");

  /* Main memory */
  char *bfinit = " = { 0 }";
  if (bfbignum)
    bfinit = "";
  if (dynamic_mem)
    fprintf (bfout, "  %s *%s;\n\n", bfstr_type, bfstr_buffer);
  else
    fprintf (bfout, "%s %s[%d]%s;\n\n",
	     bfstr_type, bfstr_buffer, mem_size, bfinit);

  /* Track buffer size */
  if (dynamic_mem || check_bounds || dump_core)
    fprintf (bfout, "int %s = %d; /* Buffer size */\n\n",
	     bfstr_bsize, mem_size);

  /* bignum init */
  if (bfbignum)
    {
      fprintf (bfout, "void bignum_init (mpz_t *buff, int n) {\n");
      fprintf (bfout, "  int i;\n");
      fprintf (bfout, "  for (i = 0; i < n; i++)\n");
      fprintf (bfout, "    mpz_init (buff[i]);\n");
      fprintf (bfout, "}\n\n");
    }

  /* bignum init */
  if (dump_core)
    {
      fprintf (bfout, "/* Dump the memory core */\n");
      fprintf (bfout, "void dump_core (%s *buff, int n) {\n", bfstr_type);
      fprintf (bfout, "  FILE *fp = fopen (\"bf-core\", \"w\");\n");
      fprintf (bfout, "  if (fp == NULL)\n");
      fprintf (bfout, "    return;\n\n");
      fprintf (bfout, "  int i;\n");
      if (!bfbignum)
	{
	  fprintf (bfout, "  for (i = 0; i < n; i++)\n");
	  fprintf (bfout, "    fprintf (fp, \"%%d\\n\", (int) buff[i]);\n\n");
	}
      else
	{
	  fprintf (bfout, "  char *numstr;\n");
	  fprintf (bfout, "  for (i = 0; i < n; i++) {\n");
	  fprintf (bfout, "    numstr = mpz_get_str(NULL, 10, buff[i]);\n");
	  fprintf (bfout, "    fprintf (fp, \"%%s\\n\", numstr);\n");
	  fprintf (bfout, "    free (numstr);\n");
	  fprintf (bfout, "  }\n");
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
      fprintf (bfout, "%s *bf_buffinc (%s *buff, %s **ptr) {\n",
	       bfstr_type, bfstr_type, bfstr_type);
      fprintf (bfout, "  int offset = *ptr - buff;\n");
      fprintf (bfout, "  int old_bsize = %s;\n", bfstr_bsize);
      fprintf (bfout, "  %s *= %d;\n", bfstr_bsize, mem_grow_rate);
      fprintf (bfout, "  if (offset > %s)\n", bfstr_bsize);
      fprintf (bfout, "    %s = offset;\n\n", bfstr_bsize);

      fprintf (bfout, "  buff = (%s *) realloc ((void *) buff, "
	       "%s * sizeof (%s));\n", bfstr_type, bfstr_bsize, bfstr_type);
      fprintf (bfout, "  if (!buff) {\n");
      fprintf (bfout, "    fprintf (stderr, \"%s:%d:%s\\n\");\n",
	       bfstr_name, lineno, bfstr_memerr);
      fprintf (bfout, "    abort ();\n  }\n\n");

      /* clear */
      if (!bfbignum)
	{
	  fprintf (bfout, "  memset ((buff + old_bsize), 0, "
		   "(%s - old_bsize) * sizeof (%s));\n",
		   bfstr_bsize, bfstr_type);
	}
      else
	{
	  fprintf (bfout, "  bignum_init ((buff + old_bsize), "
		   "%s - old_bsize);\n", bfstr_bsize);
	}

      fprintf (bfout, "  *ptr = buff + offset;\n");
      fprintf (bfout, "  return buff;\n}\n\n");

      /* main */
      fprintf (bfout, "int main () {\n");
      fprintf (bfout, "  %s = malloc (%d * sizeof (%s));\n\n",
	       bfstr_buffer, mem_size, bfstr_type);
      fprintf (bfout, "  %s *%s = %s;\n\n",
	       bfstr_type, bfstr_ptr, bfstr_buffer);
    }
  else
    {
      /* main */
      fprintf (bfout, "int main () {\n");
      fprintf (bfout, "  %s *%s = %s;\n\n",
	       bfstr_type, bfstr_ptr, bfstr_buffer);
    }

  if (bfbignum)
    {
      fprintf (bfout, "  bignum_init (%s, %d);\n\n", bfstr_buffer, mem_size);
    }

  if (dump_core)
    fprintf (bfout, "  signal (SIGINT, int_handle);\n\n");
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
	  fprintf (bfout, "  %s = bf_buffinc (%s, &%s);\n",
		   bfstr_buffer, bfstr_buffer, bfstr_ptr);
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
  fprintf (bfout, bfstr_get, bfstr_ptr, bfstr_type);
}

/* Print output code */
void print_output ()
{
  print_indent ();
  fprintf (bfout, bfstr_put, bfstr_ptr);
}

/* Print loop beginning */
void print_loop ()
{
  print_indent ();
  if (!bfbignum)
    fprintf (bfout, bfstr_loop, bfstr_ptr);
  else
    fprintf (bfout, bfstr_loop, bfstr_ptr, bfstr_ptr);
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
}

void print_cclr ()
{
  if (!bfbignum)
    {
      print_indent ();
      fprintf (bfout, "*%s = 0;\n", bfstr_ptr);
    }
  else
    {
      print_indent ();
      fprintf (bfout, "mpz_set_ui (*%s, 0);\n", bfstr_ptr);
    }
}

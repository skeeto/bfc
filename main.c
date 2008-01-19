/* Brainfuck to C converter */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <sys/wait.h>

#include "parser.h"		/* Code reading */
#include "codegen.h"		/* Code writing */
#include "common.h"		/* Needed by all */

char *progname = "";
char *version = "0.1-alpha";

int set_type (char *s);

/* Filenames */
char *outfile = "-";
char *midfile;

void print_version ()
{
  printf ("%s, version %s\n", PACKAGE_NAME, PACKAGE_VERSION);
  printf ("Copyright (C) 2007 Chris Wellons\n");
  printf ("This is free software; see the source code for "
	  "copying conditions.\n");
  printf ("There is ABSOLUTELY NO WARRANTY; not even for "
	  "MERCHANTIBILITY or\n");
  printf ("FITNESS FOR A PARTICULAR PURPOSE.\n");
}

void print_usage (int exit_stat)
{
  printf ("Usage: %s [OPTION ...] FILE\n\n", progname);
  printf ("Options:\n\n");
  printf ("  -b, --bounds-err      Error on pointer outside of boundies "
	  "at runtime\n");
  printf ("  -s, --static-mem      Memory size, number of cells (%d)\n",
	  mem_size);
  printf ("  -g, --mem-grow-rate   Dynamic memory grow rate (%d)\n",
	  mem_grow_rate);
  printf ("  -o, --output          Select output file\n");
  printf ("  -O, --optimize        Optimize compiled code (C compiler)\n");
  printf ("  -d, --dump            Dump memory core after run\n");
  printf ("  -C, --comments        Pass comments back out\n");
  printf ("  -H, --threads         Each supplied program gets a thread\n");
#ifdef EN_COMPILE
  printf ("  -c, --compile         Send output to C compiler\n");
#endif
  printf ("  -n, --no-optimize     Don't perform brainfuck optimization\n");
  printf ("  -t, --cell-type       Cell type (see below)\n");
  printf ("  -V, --version         Print program version\n");
  printf ("  -h, --help            Print this help information\n");
  printf ("\nCell Types:\n\n");
  printf ("  char         Unsigned char   (0 to 256)\n");
  printf ("  short        Unsigned short  (probably 0 to 65536)\n");
  printf ("  int          Unsigned int    (probably 0 to 4294967296)\n");
#if EN_BIGNUM
  printf ("  bignum       Multi-precision "
	  "(unlimited range, no wrapping, requires gmp)\n");
#endif

  exit (exit_stat);
}

int main (int argc, char **argv)
{
  progname = argv[0];

  while (1)
    {
      static struct option long_options[] = {
	/* *INDENT-OFF* */
	{"static-mem",    no_argument,       0, 's'},
	{"bounds-err",    no_argument,       0, 'b'},
	{"mem-size",      required_argument, 0, 'm'},
	{"mem-grow-rate", required_argument, 0, 'g'},
	{"cell-type",     required_argument, 0, 't'},
	{"output",        required_argument, 0, 'o'},
	{"optimize",      no_argument,       0, 'O'},
	{"no-optimize",   no_argument,       0, 'n'},
	{"threads",       no_argument,       0, 'H'},
	{"comments",      no_argument,       0, 'C'},
#ifdef EN_COMPILE
	{"compile",       no_argument,       0, 'c'},
#endif
	{"dump",          no_argument,       0, 'd'},
	{"version",       no_argument,       0, 'V'},
	{"help",          no_argument,       0, 'h'},
	{0, 0, 0, 0}
	/* *INDENT-ON* */
      };

      /* getopt_long stores the option index here. */
      int option_index = 0;
      char c;
      c = getopt_long (argc, argv, "sbm:g:t:o:OHncCdVh",
		       long_options, &option_index);

      /* Detect the end of the options. */
      if (c == -1)
	break;

      switch (c)
	{
	case 's':		/* static memory */
	  dynamic_mem = 0;
	  break;

	case 'b':		/* bounds checking */
	  check_bounds = 1;
	  break;

	case 'C':		/* comments */
	  pass_comments = 1;
	  break;

	case 'H':		/* threads */
	  bfthreads = 1;
	  break;

	case 'm':		/* memory size */
	  mem_size = atoi (optarg);
	  if (mem_size < 1)
	    {
	      fprintf (stderr,
		       "%s: --mem-size argument must be >= 1\n", progname);
	      exit (EXIT_FAILURE);
	    }
	  break;

	case 'g':		/* memory grow rate */
	  mem_grow_rate = atoi (optarg);
	  if (mem_grow_rate <= 1)
	    {
	      fprintf (stderr,
		       "%s: --mem-grow-rate argument must be > 1\n",
		       progname);
	      exit (EXIT_FAILURE);
	    }
	  break;

	case 'o':		/* output */
	  outfile = optarg;
	  break;

	case 't':		/* cell type */
	  if (set_type (optarg) != 0)
	    {
	      fprintf (stderr, "%s: bad cell type %s\n", progname, optarg);
	      exit (EXIT_FAILURE);
	    }
	  break;

#ifdef EN_COMPILE
	case 'c':		/* compile */
	  compile_output = 1;
	  break;
#endif

	case 'O':		/* optimize */
	  optimize_c = 1;
	  break;

	case 'n':		/* optimize */
	  optimize = 0;
	  break;

	case 'd':		/* compile */
	  dump_core = 1;
	  break;

	case 'V':		/* version */
	  print_version ();
	  exit (EXIT_SUCCESS);
	  break;

	case 'h':		/* help */
	  print_usage (EXIT_SUCCESS);
	  break;

	case '?':		/* error */
	  print_usage (EXIT_FAILURE);
	  break;

	default:
	  abort ();
	}
    }

  /* Set up for threads */
  if (bfthreads)
    {
      bfthreads = argc - optind;
      bfstr_htype = bfstr_type;
      bfstr_type = "bf_hcell";
    }

  /* No input files */
  if (argc - optind == 0)
    {
      fprintf (stderr, "%s: no input files\n", progname);
      exit (EXIT_FAILURE);
    }

  /* Output file */
#ifdef EN_COMPILE
  if (compile_output)
    {
      /* Create a temporary file */
      char *tmplate = "bftmp.XXXXXX";
      midfile = (char *) bfmalloc (strlen (tmplate) + 1);
      strcpy (midfile, tmplate);

      int fd;
      if ((fd = mkstemp (midfile)) == -1 ||
	  (bfout = fdopen (fd, "w+")) == NULL)
	{
	  fprintf (stderr, "%s: can't create temp file %s - %s\n",
		   progname, midfile, strerror (errno));
	  exit (EXIT_FAILURE);
	}
    }
  else
#endif
    {
      /* Output file is text */
      if (strcmp (outfile, "-") != 0)
	{
	  bfout = fopen (outfile, "w");
	  if (bfout == NULL)
	    {
	      fprintf (stderr, "%s: failed to open file %s - %s\n",
		       progname, outfile, strerror (errno));
	      exit (EXIT_FAILURE);
	    }
	}
      else
	{
	  bfout = stdout;
	}
    }

  /* Produce the code */
  print_head ();
  char c;
  for (; optind < argc; optind++)
    {
      /* Open next file and keep parsing */
      if (strcmp (argv[optind], "-") != 0)
	{
	  bfin = fopen (argv[optind], "r");
	  if (bfin == NULL)
	    {
	      fprintf (stderr, "%s: failed to open file %s - %s\n",
		       progname, argv[optind], strerror (errno));
	      break;
	    }
	}
      else
	{
	  bfin = stdin;
	}

      lineno = 1;
      while ((c = bfscan ()) != 0)
	{
	  bfparse (c);
	}

      /* Mismatched brackets (bad indentation level) */
      if (indent != 1)
	{
	  char *infile = argv[optind];
	  if (strcmp (infile, "-") == 0)
	    infile = "stdin";

	  fprintf (stderr, "%s: mismatched brackets in %s\n",
		   progname, infile);
	  exit (EXIT_FAILURE);
	}

      /* If threaded, start producing code */
      if (bfthreads)
	{
	  bfparse (0);
	  if (optimize)
	    im_opt (head);
	  im_codegen (head);
	  head = NULL;
	}
    }
  if (!bfthreads)
    {
      bfparse (0);
      if (optimize)
	im_opt (head);
      im_codegen (head);
      print_tail ();
    }

  /* Close output file */
  if (!strcmp (outfile, "-") || compile_output)
    fclose (bfout);

#ifdef EN_COMPILE
  if (compile_output)
    {
      /* Fork off a compiler to build this */
      pid_t pid = fork ();
      if (pid == -1)
	{
	  fprintf (stderr, "%s: fork error - %s", progname, strerror (errno));
	  exit (EXIT_FAILURE);
	}
      if (pid == 0)
	{
	  /* Build argv */
	  char *fargv[15];
	  fargv[0] = "gcc";
	  fargv[1] = "-x";
	  fargv[2] = "c";
	  int i = 3;

	  if (strcmp (outfile, "-") != 0)
	    {
	      fargv[i] = "-o";
	      fargv[i + 1] = outfile;
	      i += 2;
	    }

	  /* Link against gmp */
	  if (bfbignum)
	    {
	      fargv[i] = "-lgmp";
	      i++;
	    }

	  /* Link against pthreads */
	  if (bfthreads)
	    {
	      fargv[i] = "-lpthread";
	      i++;
	    }

	  /* Optimize */
	  if (optimize_c)
	    {
	      fargv[i] = "-O2";
	      i++;
	    }

	  fargv[i] = midfile;
	  fargv[i + 1] = NULL;

	  /* C compiler */
	  execvp ("gcc", fargv);

	  fprintf (stderr, "%s: execvp failed\n", progname);
	  exit (EXIT_FAILURE);
	}
      int s;
      wait (&s);

      unlink (midfile);
    }
#endif

  exit (EXIT_SUCCESS);
}

/* Determine cell type */
int set_type (char *s)
{
  if (strcmp (s, "char") == 0)
    {
      bfstr_type = "unsigned char";
    }
  else if (strcmp (s, "short") == 0)
    {
      bfstr_type = "unsigned short";
    }
  else if (strcmp (s, "int") == 0)
    {
      bfstr_type = "unsigned int";
    }
#if EN_BIGNUM
  else if (strcmp (s, "bignum") == 0)
    {
      bfstr_type = "mpz_t";
      bfbignum = 1;
      bfstr_get = "mpz_set_ui (*%s, (unsigned long int) getchar ());\n";
      bfstr_put = "putchar ((char) mpz_get_ui (*%s));\n";
      bfstr_loop =
	"while ( !mpz_fits_sint_p (*%s) || mpz_get_ui (*%s) != 0) {\n";
    }
#endif
  else
    {
      return 1;
    }

  return 0;
}

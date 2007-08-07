#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "common.h"

void *bfmalloc (size_t size)
{
  void *dat = malloc (size);
  if (dat == NULL)
    {
      fprintf (stderr, "%s: failed to malloc - %s\n",
	       progname, strerror (errno));
      abort ();
    }
  return dat;
}

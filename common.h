#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>

/* Get configuration options */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#  if HAVE_WORKING_FORK && HAVE_SYS_WAIT_H
#    define EN_COMPILE
#  endif
#  ifndef EN_BIGNUM
#    ifdef HAVE_GMP
#      define EN_BIGNUM 1
#    else
#      define EN_BIGNUM 0
#    endif
#  endif
#else /* Turn everything on */
#  define EN_COMPILE
#  define EN_BIGNUM
#  define PACKAGE_NAME "wbf2c"
#  define PACKAGE_VERSION ""
#endif

extern char *progname;

void *bfmalloc (size_t size);

#endif

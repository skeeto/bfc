#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT(wbf2c, 1.0, ccw129@psu.edu)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([main.c])
AC_CONFIG_HEADER([config.h])

AH_TEMPLATE([HAVE_GMP], [Notify us if the GNU multi-precision 
			library is available.])
AH_TEMPLATE([EN_BIGNUM], [Define user preference on bignum setting.])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB([gmp], [__gmpz_init], [AC_DEFINE(HAVE_GMP)])

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_CHECK_FUNCS([strerror])

# Options

# Bignum
EN_BIGNUM=false
USER_BIGNUM=false
AC_ARG_ENABLE(bignum,
  [AS_HELP_STRING([--enable-bignum],
     [Enable bignum cell types])],
  [USER_BIGNUM=true; if test "$enableval" = yes; then EN_BIGNUM=true; fi], [])
if $EN_BIGNUM && $USER_BIGNUM ; then
   AC_DEFINE(EN_BIGNUM)
else
   if $USER_BIGNUM ; then
      AC_DEFINE(EN_BIGNUM, [0])
   fi
fi

# Compile
EN_COMPILE=false
USER_BIGNUM=false
AC_ARG_ENABLE(bignum,
  [AS_HELP_STRING([--enable-bignum],
     [Enable bignum cell types])],
  [USER_BIGNUM=true; if test "$enableval" = yes; then EN_BIGNUM=true; fi], [])
if $EN_BIGNUM && $USER_BIGNUM ; then
   AC_DEFINE(EN_BIGNUM)
else
   if $USER_BIGNUM ; then
      AC_DEFINE(EN_BIGNUM, [0])
   fi
fi

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

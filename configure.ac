AC_PREREQ([2.69])
AC_INIT([libgrabkernel], m4_esyscmd([git rev-list --count HEAD | tr -d '\n']), [tihmstar@gmail.com])

AC_CANONICAL_SYSTEM

AC_CANONICAL_HOST
# Check for operating system
AC_MSG_CHECKING([whether we need platform-specific build settings])
case $host_os in
  darwin* )
        CXXFLAGS+=" -stdlib=libc++ "
        ;;
esac


AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])
AM_PROG_LIBTOOL
AM_INIT_AUTOMAKE


CPPFLAGS+=" -std=c++11 -g"

CPPFLAGS+=" -D VERSION_COMMIT_COUNT=\\\"$(git rev-list --count HEAD | tr -d '\n')\\\""
CPPFLAGS+=" -D VERSION_COMMIT_SHA=\\\"$(git rev-parse HEAD | tr -d '\n')\\\""
CPPFLAGS+=" -D EXPECTIONNAME=TKexception"


# Checks for programs.
AC_PROG_CXX
AC_PROG_CC


# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stdint.h stdlib.h string.h unistd.h])

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE

LT_INIT

AC_CONFIG_FILES([Makefile
                 include/Makefile
                 libtakeover/Makefile
		 libtakeover.pc
		 tools/Makefile])
AC_OUTPUT


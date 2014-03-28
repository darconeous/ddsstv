
AC_PREREQ(2.59)

AC_INIT([ddsstv],m4_esyscmd([([ -d .git ] && gitver=`git describe 2> /dev/null` && printf "$gitver") || printf  "0.1.0"]))

AC_CONFIG_AUX_DIR([m4])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE()
LT_INIT()

AC_LIBTOOL_DLOPEN_SELF
AC_PROG_LIBTOOL

AC_CONFIG_SRCDIR([src/ddsstv/ddsstv.h])
AC_CONFIG_HEADERS([src/config.h])

AM_MAINTAINER_MODE

# Checks for programs.
AC_PROG_CC

# Checks for header files.
AC_HEADER_STDC

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

AC_C_INLINE
AC_C_VOLATILE

AC_CONFIG_FILES([
	Makefile
	src/Makefile
	src/dddsp/Makefile
	src/ddsstv/Makefile
	src/app-ios/Makefile
	src/app-osx/Makefile
	res/Makefile
	res/app-ios/Makefile
	res/app-osx/Makefile
	doc/Makefile
])
AC_OUTPUT


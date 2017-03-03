AC_PREREQ([2.69])
AC_INIT([usb-tools], [1.0.0])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AM_SILENT_RULES([yes])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])
AC_PROG_CC
AC_USE_SYSTEM_EXTENSIONS

# Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_MAKE_SET

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h limits.h malloc.h stdint.h stdlib.h string.h \
			  sys/ioctl.h sys/mount.h sys/time.h termios.h \
			  unistd.h wchar.h])

# libusb-1.0
PKG_CHECK_MODULES([libusb], [libusb-1.0])

# libssl
PKG_CHECK_MODULES([ssl], [openssl])

# hidapi
PKG_CHECK_MODULES([hid], [hidapi-hidraw])

# libpthread
m4_include([m4/ax_pthread.m4])
ACX_PTHREAD

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_INT64_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_STRERROR_R
AC_CHECK_FUNCS([clock_gettime getpagesize gettimeofday memset strdup strerror strtol strtoul])

AC_GNU_SOURCE
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT

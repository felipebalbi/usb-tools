AC_INIT([usb-tools], [1.0.0])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AM_SILENT_RULES([yes])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_HEADERS([config.h])
AC_PROG_CC

# libusb-1.0
PKG_CHECK_MODULES([libusb], [libusb-1.0])

# libssl
PKG_CHECK_MODULES([ssl], [openssl])

# hidapi
PKG_CHECK_MODULES([hid], [hidapi-hidraw])

# libpthread
m4_include([m4/ax_pthread.m4])
ACX_PTHREAD

AC_GNU_SOURCE
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT

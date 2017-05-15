if DEBUG
	AM_CFLAGS = -O2 -Wall -Wextra
else
	AM_CFLAGS = -g -O0 -Wall -Wextra
endif

dist_bin_SCRIPTS = msc.sh scptest.sh test.sh usbpwrtest

bin_PROGRAMS =		\
	acmc		\
	acmd		\
	cleware		\
	companion-desc	\
	control		\
	device-reset	\
	msc		\
	serialc		\
	seriald		\
	switchbox	\
	testmode	\
	testusb		\
	trace		\
	uda		\
	uda-client

# These have no extra requirement
acmc_SOURCES = acmc.c
acmc_CFLAGS = $(AM_CFLAGS)
acmc_LDADD =

acmd_SOURCES = acmd.c
acmd_CFLAGS = $(AM_CFLAGS)
acmd_LDADD =

serialc_SOURCES = serialc.c
serialc_CFLAGS = $(AM_CFLAGS)
serialc_LDADD =

seriald_SOURCES = seriald.c
seriald_CFLAGS = $(AM_CFLAGS)
seriald_LDADD =

switchbox_SOURCES = switchbox.c
switchbox_CFLAGS = $(AM_CFLAGS)
switchbox_LDADD =

# This needs HIDAPI
cleware_SOURCES = cleware.c
cleware_CFLAGS = $(AM_CFLAGS) $(hid_CFLAGS)
cleware_LDADD = $(hid_LIBS)

# These need libusb-1.0
companion_desc_SOURCES = companion-desc.c
companion_desc_CFLAGS = $(AM_CFLAGS) $(libusb_CFLAGS)
companion_desc_LDADD = $(libusb_LIBS)

control_SOURCES = control.c
control_CFLAGS = $(AM_CFLAGS) $(libusb_CFLAGS)
control_LDADD = $(libusb_LIBS)

device_reset_SOURCES = device-reset.c
device_reset_CFLAGS = $(AM_CFLAGS) $(libusb_CFLAGS)
device_reset_LDADD = $(libusb_LIBS)

testmode_SOURCES = testmode.c
testmode_CFLAGS = $(AM_CFLAGS) $(libusb_CFLAGS)
testmode_LDADD = $(libusb_LIBS)

trace_SOURCES = trace.c
trace_CFLAGS = $(AM_CFLAGS) $(libusb_CFLAGS)
trace_LDADD = $(libusb_LIBS)

uda_SOURCES = uda.c
uda_CFLAGS = $(AM_CFLAGS) $(libusb_CFLAGS)
uda_LDADD = $(libusb_LIBS)

# This needs libssl
msc_SOURCES = msc.c
msc_CFLAGS = $(AM_CFLAGS) $(ssl_CFLAGS)
msc_LDADD = $(ssl_LIBS)

# These need libpthread
testusb_SOURCES = testusb.c
testusb_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS)
testusb_LDADD = $(PTHREAD_LIBS)

uda_client_SOURCES = uda-client.c
uda_client_CFLAGS = $(AM_CFLAGS) $(PTHREAD_CFLAGS)
uda_client_LDADD = $(PTHREAD_LIBS)



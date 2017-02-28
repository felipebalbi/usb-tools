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
	uda		\
	uda-client

acmc_SOURCES = acmc.c
acmd_SOURCES = acmd.c
cleware_SOURCES = cleware.c
companion_desc_SOURCES = companion-desc.c
control_SOURCES = control.c
device_reset_SOURCES = device-reset.c
msc_SOURCES = msc.c
serialc_SOURCES = serialc.c
seriald_SOURCES = seriald.c
switchbox_SOURCES = switchbox.c
testmode_SOURCES = testmode.c
testusb_SOURCES = testusb.c
uda_SOURCES = uda.c
uda_client_SOURCES = uda-client.c

AM_CFLAGS = -O2 -Wall -Wextra -Wpedantic $(libusb_CFLAGS) \
	$(ssl_CFLAGS) $(hid_CFLAGS) $(PTHREAD_CFLAGS)
LIBS += $(libusb_LIBS) $(ssl_LIBS) $(hid_LIBS) $(PTHREAD_LIBS)

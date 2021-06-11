

EXECUTABLE := chromecastmanager

SOURCE := chromecastmanager.c chromecastmanager_connectionupdate.c chromecastmanager_macro.c chromecastmanager_devicerequest.c chromecastmanager_deviceresponse.c chromecast_mdns.c chromecast_interface.c chromecast_interface_expandvars.c

LIB := libtools/libtools.a
DBGLIB := libtools/libtools-dbg.a
DOLIB := libdataobject/ldataobject.a
DODBGLIB := libdataobject/ldataobject-dbg.a
HTMLFSLIB := html/lhtmlfs.a
NETLIB := libnet/lnet.a
NETDBGLIB := libnet/lnet-dbg.a
CCDEBUG := -D CCDEBUG

#
#
#

std: submodules libs ${EXECUTABLE}

debug: submodules libs-dbg ${EXECUTABLE}-dbg

all: std debug

#
# sudo apt-get install gdb python3
# pip install gdbgui
#

rund: debug
	gdbgui --args ./chromecastmanager-dbg ../conf internal ./chromecastmanager-dbg

runn: debug
	nemiver ./chromecastmanager-dbg ../conf internal


runv: debug
	valgrind -s --leak-check=full --track-origins=yes ./chromecastmanager-dbg ../conf info

runvv: debug
	valgrind -s --leak-check=full --track-origins=yes ./chromecastmanager-dbg ../conf internal

submodules: libnet/Makefile libdataobject/Makefile libtools/Makefile

clean: 
	(cd libdataobject ; make clean)
	(cd libtools ; make clean)
	(cd html ; make clean)
	(cd libnet ; make clean)
	/bin/rm -f ${EXECUTABLE} ${EXECUTABLE}-dbg

pull:
	git pull --recurse-submodules

libs:
	(cd libdataobject ; make)
	(cd libtools ; make) 
	(cd html ; make)
	(cd libnet ; make)

libs-dbg:
	(cd libdataobject ; make debug)
	(cd libtools ; make debug)
	(cd html ; make)
	(cd libnet ; make debug)

${EXECUTABLE}: ${SOURCE} ${LIB} ${DOLIB} ${HTMLFSLIB} ${NETLIB}
	gcc -o $@ ${SOURCE} ${LIB} ${DOLIB} ${HTMLFSLIB} ${NETLIB} -lssl -lcrypto

${EXECUTABLE}-dbg: ${SOURCE} ${DBGLIB} ${DODBGLIB} ${HTMLFSLIB} ${NETDBGLIB}
	gcc -g ${CCDEBUG} -D DEBUG -o $@ ${SOURCE} ${DBGLIB} ${DODBGLIB} ${HTMLFSLIB} ${NETDBGLIB} -lssl -lcrypto

libtools/Makefile:
	git submodule update --init --recursive

libnet/Makefile:
	git submodule update --init --recursive

libdataobject/Makefile:
	git submodule update --init --recursive
%.c : %.h


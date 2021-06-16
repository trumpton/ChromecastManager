
EXEFILES := expandvars_test

DBGLIB := ../libtools/libtools-dbg.a
DODBGLIB := ../libdataobject/ldataobject-dbg.a
HTMLFSLIB := ../html/lhtmlfs.a
NETDBGLIB := ../libnet/lnet-dbg.a
CCDEBUG := -D CCDEBUG

#
#
#

all: libs-dbg expandvars_test

clean: 
	/bin/rm -f ${EXEFILES}

libs-dbg:
	(cd ../libdataobject ; make debug)
	(cd ../libtools ; make debug)
	(cd ../html ; make)
	(cd ../libnet ; make debug)

expandvars_test: expandvars_test.c ../chromecast_interface_expandvars.c
	gcc -g -D DEBUG -o $@ $^ ${DBGLIB} ${DODBGLIB} ${HTMLFSLIB} ${NETDBGLIB} -lssl -lcrypto



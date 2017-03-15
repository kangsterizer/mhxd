# Makefile for hxd/src/hxd

bin_PROGRAMS = hxd
prefix = $(PWD)/../../run/hxd

INCLUDES = -I. -I.. -I../.. -I../common -I../conf -I../lib -I../protocol @INCLUDES@ @HXDINCLUDES@
DEFS = @DEFS@ -D_REENTRANT -D_GNU_SOURCE -DONLY_SERVER
COMMONLDADD = ../common/decode.o ../common/fd.o \
	      ../common/hlwrite.o ../common/hxd_config.o \
	      ../common/threads.o ../common/version.o @HXDCOMMONLDADD@
LIBLDADD = @HXDLIBLDADD@ @LIBLDADD@ ../lib/xmalloc.o
COMPRESSLDADD = @COMPRESSLDADD@
CIPHERLDADD = @CIPHERLDADD@
APPLELDADD = @APPLELDADD@
PROTOCOLLDADD = ../protocol/libprotocol.a
LIBS = @HXDLIBS@
LDADD = $(LIBLDADD) $(COMMONLDADD) $(COMPRESSLDADD) $(CIPHERLDADD) $(PROTOCOLLDADD) \
	$(APPLELDADD) ../conf/libconf.a

ONLY_SRC = hxd.c hldump.c
hxd_SOURCES = accounts.c chat.c commands.c files.c hlserver.c htxf.c ident.c news.c rcv.c snd.c spam.c sql.c tnews.c $(ONLY_SRC)
hxd_LDFLAGS = @DYNAMICLDFLAGS@ @HXDLDFLAGS@
hxd_LDADD = @HXDOBJS@ $(LDADD) $(LIBLTDL)
hxd_DEPENDENCIES = @HXDOBJS@

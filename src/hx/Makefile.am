# Makefile for hxd/src/hx

bin_PROGRAMS = hx
prefix = $(PWD)/../../run/hx

DEFS = @DEFS@ -D_REENTRANT -D_GNU_SOURCE -DONLY_CLIENT
INCLUDES = -I../.. -I. -I../common -I../conf -I../lib -I../protocol @INCLUDES@ @HXINCLUDES@
COMMONLDADD = ../common/decode.o ../common/fd.o ../common/hlwrite.o ../common/threads.o \
	      ../common/version.c \
	      @HXCOMMONLDADD@
LIBLDADD = @HXLIBLDADD@ @LIBLDADD@ ../lib/xmalloc.o
COMPRESSLDADD = @COMPRESSLDADD@
CIPHERLDADD = @CIPHERLDADD@
APPLELDADD = @APPLELDADD@
LIBS = @HXLIBS@
LDADD = $(COMMONLDADD) $(LIBLDADD) $(COMPRESSLDADD) $(CIPHERLDADD) \
	$(APPLELDADD)

ONLY_SRC = hxd.c hldump.c
TTY_SRC = hx_tty.c
hx_SOURCES = connect.c files.c hlclient.c hx_chat.c hx_commands.c hx_rcv.c hx_users.c \
	     macres.c news.c tasks.c tracker.c $(ONLY_SRC) $(TTY_SRC)
hx_LDFLAGS = @HXLDFLAGS@
hx_LDADD = @HXOBJS@ $(LDADD)
hx_DEPENDENCIES = @HXOBJS@

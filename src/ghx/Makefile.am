# Makefile for hxd/src/ghx

bin_PROGRAMS = ghx
prefix = $(PWD)/../../run/hx

INCLUDES = -I. -I../.. -I../common -I../conf -I../hx -I../lib -I../protocol @INCLUDES@ @HXINCLUDES@ @GINCLUDES@
DEFS = @DEFS@ -D_REENTRANT -D_GNU_SOURCE -DONLY_CLIENT -DONLY_GTK
COMMONLDADD = ../common/decode.o ../common/fd.o ../common/threads.o \
	      ../common/version.c @HXCOMMONLDADD@
LIBLDADD = @HXLIBLDADD@ @LIBLDADD@ ../lib/xmalloc.o
COMPRESSLDADD = @COMPRESSLDADD@
CIPHERLDADD = @CIPHERLDADD@
HXLDADD = ../hx/connect.o ../hx/files.o ../common/hlwrite.o ../hx/hx_commands.o ../hx/hx_chat.o \
	  ../hx/hx_rcv.o ../hx/hx_users.o \
	  ../hx/macres.o ../hx/news.o ../hx/tracker.o ../hx/tasks.o @GHXHXLDADD@
LDADD = $(COMMONLDADD) $(LIBLDADD) $(COMPRESSLDADD) $(CIPHERLDADD) $(HXLDADD)
LIBS = @LIBS@ @HXLIBS@ @GHXLIBS@

ONLY_SRC = hlclient.c hx_tty.c hxd.c
ghx_SOURCES = chat.c cicn.c cmenu.c connect.c files.c gtk_hlist.c history.c hx_gtk.c \
	      hx_gtk_about.c icons.c news.c options.c tasks.c toolbar.c tracker.c \
	      useredit.c users.c news15.c $(ONLY_SRC)
ghx_LDADD = @GHXOBJS@ $(LDADD)
ghx_DEPENDENCIES = $(LDADD)

# Makefile for hxd/src/acctedit

bin_PROGRAMS = acctedit
prefix = $(PWD)/../../run/hxd

INCLUDES = -I. -I../.. -I../common -I../lib @INCLUDES@ @HXDINCLUDES@
DEFS = @DEFS@ -D_REENTRANT -D_GNU_SOURCE -DONLY_SERVER
COMMONLDADD =
LIBLDADD =
LIBS = @CURSESLIBS@ @ACCTEDITLIBS@
LDADD = $(LIBLDADD) $(COMMONLDADD) $(ACCTEDITLIBLDADD)

acctedit_SOURCES = acctedit.c

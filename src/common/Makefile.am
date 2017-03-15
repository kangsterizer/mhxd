# Makefile for hxd/src/common

INCLUDES = -I. -I.. -I../.. -I../lib -I../conf -I../protocol $(INCLTDL) @INCLUDES@ @COMMONINCLUDES@
DEFS = @DEFS@ -D_REENTRANT -D_GNU_SOURCE
GENCONFFLAGS = @GENCONFFLAGS@
GENCONF = ../conf/genconf

noinst_LIBRARIES = libcommon.a
libcommon_a_SOURCES =  decode.c fd.c hlwrite.c hxd_config.c threads.c version.c
libcommon_a_LIBADD = @COMMONLIBADD@
libcommon_a_DEPENDENCIES = @COMMONLIBADD@ hldump.h hxd_config.h

hldump.h: hotline.h
	./mkhldumph.pl >hldump.h

decode.c: hxd_config.h
hxd_config.c: hxd_config.h hxd_wanted.c

hxd_config.h: hxd.conf.in ../conf/libconf.a
	$(GENCONF) $(GENCONFFLAGS) -p hxd_ -h hxd.conf.in >hxd_config.h

hxd_wanted.c: hxd.conf.in ../conf/libconf.a
	$(GENCONF) $(GENCONFFLAGS) -p hxd_ -w hxd.conf.in >hxd_wanted.c

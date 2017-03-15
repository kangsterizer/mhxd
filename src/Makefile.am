# Makefile for hxd/src

if build_HOPE
SUB_CIPHER = cipher
endif

if build_COMPRESS
SUB_COMPRESS = compress
endif

if build_HXD
SUB_HXD = hxd
endif

if build_ACCTEDIT
SUB_ACCTEDIT = acctedit
endif

if build_HXTRACKD
SUB_HXTRACKD = hxtrackd
endif

if build_HX
SUB_HX = hx
endif

if build_GHX
SUB_GHX = ghx
endif

if build_HXD
SUB_PROTO = protocol
endif

if build_APPLE
SUB_APPLE = apple
endif

SUBDIRS = lib conf common $(SUB_APPLE) $(SUB_PROTO) $(SUB_CIPHER) $(SUB_COMPRESS) $(SUB_HXD) $(SUB_ACCTEDIT) $(SUB_HXTRACKD) $(SUB_HX) $(SUB_GHX)
DIST_SUBDIRS = lib conf common $(SUB_APPLE) $(SUB_PROTO) $(SUB_CIPHER) $(SUB_COMPRESS) $(SUB_HXD) $(SUB_ACCTEDIT) $(SUB_HXTRACKD) $(SUB_HX) $(SUB_GHX)

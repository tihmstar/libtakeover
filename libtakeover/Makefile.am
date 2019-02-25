AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/include/libtakeover -I$(top_srcdir)/external/liboffsetfinder64/include
AM_LDFLAGS  = -L/usr/local/lib/

lib_LTLIBRARIES = libtakeover.la 

libtakeover_la_CXXFLAGS = $(AM_CPPFLAGS)
libtakeover_la_LIBADD = $(AM_LDFLAGS)
libtakeover_la_SOURCES = libtakeover.cpp $(top_srcdir)/external/liboffsetfinder64/liboffsetfinder64/exception.cpp

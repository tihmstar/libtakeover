AM_CFLAGS = -I$(top_srcdir)/include $(GLOBAL_CFLAGS) $(libgeneral_CFLAGS)
AM_CXXFLAGS = $(AM_CFLAGS) $(GLOBAL_CXXFLAGS)
AM_LDFLAGS = $(libgeneral_LIBS) 

lib_LTLIBRARIES = libtakeover.la 

libtakeover_la_CXXFLAGS = $(AM_CXXFLAGS)
libtakeover_la_LIBADD = $(AM_LDFLAGS)
libtakeover_la_SOURCES = libtakeover.cpp 

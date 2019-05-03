AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/external/libgeneral/include
AM_LDFLAGS  = -L$(top_srcdir)/libtakeover/.libs/

bin_PROGRAMS = inject_criticald

inject_criticald_CXXFLAGS = $(AM_CFLAGS)
inject_criticald_LDADD 	  = $(AM_LDFLAGS) -ltakeover
inject_criticald_SOURCES  = inject_criticald.cpp


all-local: inject_criticald
	strip $^
	ldid2 -SEnt.plist $^

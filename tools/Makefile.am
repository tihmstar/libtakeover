AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/external/libgeneral/include
AM_LDFLAGS  = -L$(top_srcdir)/libtakeover/.libs/

bin_PROGRAMS = inject_criticald orphan_commander

inject_criticald_CXXFLAGS = $(AM_CFLAGS)
inject_criticald_LDADD 	  = $(AM_LDFLAGS) -ltakeover
inject_criticald_SOURCES  = inject_criticald.cpp

orphan_commander_CXXFLAGS = $(AM_CFLAGS)
orphan_commander_LDADD 	  = $(AM_LDFLAGS) -ltakeover
orphan_commander_SOURCES  = orphan_commander.cpp


all-local: inject_criticald orphan_commander
	strip inject_criticald
	strip orphan_commander
	ldid2 -SEnt.plist inject_criticald
	ldid2 -SEnt.plist orphan_commander

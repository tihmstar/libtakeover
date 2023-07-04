AM_CFLAGS = -I$(top_srcdir)/include $(libgeneral_CFLAGS)
AM_CXXFLAGS = $(AM_CFLAGS) $(GLOBAL_CXXFLAGS)
AM_LDFLAGS  = $(libgeneral_LIBS)

bin_PROGRAMS = inject_criticald orphan_commander

inject_criticald_CXXFLAGS = $(AM_CFLAGS)
inject_criticald_LDADD 	  = $(AM_LDFLAGS) ../libtakeover/libtakeover.la
inject_criticald_SOURCES  = inject_criticald.cpp

orphan_commander_CXXFLAGS = $(AM_CFLAGS)
orphan_commander_LDADD 	  = $(AM_LDFLAGS) ../libtakeover/libtakeover.la
orphan_commander_SOURCES  = orphan_commander.cpp


all-local: inject_criticald orphan_commander
	ldid -SEnt.plist inject_criticald || ldid -SEnt.plist .libs/inject_criticald
	ldid -SEnt.plist orphan_commander || ldid -SEnt.plist .libs/orphan_commander

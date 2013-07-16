ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = atomic-install

atomic_install_SOURCES = \
	src/journal.cxx src/journal.hxx \
	src/posixio.cxx src/posixio.hxx \
	src/atomic-install.cxx
atomic_install_CPPFLAGS = $(LIBCOPYFILE_CFLAGS) $(OPENSSL_CFLAGS)
atomic_install_LDADD = $(LIBCOPYFILE_LIBS) $(OPENSSL_LIBS)

EXTRA_DIST = NEWS
NEWS: configure.ac Makefile.am
	git for-each-ref refs/tags --sort '-*committerdate' \
		--format '# %(tag) (%(*committerdate:short))%0a%(contents:body)' \
		> NEWS

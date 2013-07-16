/**
 * atomic-install
 * (c) 2013 Michał Górny
 * Released under the 2-clause BSD license
 */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include "journal.hxx"

using namespace atomic_install;

int main(int argc, char* argv[])
{
	Journal j("/var/tmp/1", "/var/tmp/2");

	j.scan_files();

	return 0;
}

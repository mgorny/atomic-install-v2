/**
 * atomic-install
 * (c) 2013 Michał Górny
 * Released under the 2-clause BSD license
 */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include "journal.hxx"

#include <iostream>

using namespace atomic_install;

int main(int argc, char* argv[])
{
	Journal j("/var/tmp/1", "/var/tmp/2");

	std::cerr << "Scanning files..." << std::endl;
	j.scan_files();

	j.save_journal("journal.tmp");

	std::cerr << "Copying files..." << std::endl;
	j.copy_files();

	std::cerr << "Backing up files..." << std::endl;
	j.backup_files();

	std::cerr << "Replacing..." << std::endl;
	j.replace();

	std::cerr << "Reverting..." << std::endl;
	j.revert();

	j.cleanup();

	return 0;
}

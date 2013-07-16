/**
 * atomic-install
 * (c) 2013 Michał Górny
 * Released under the 2-clause BSD license
 */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include "posixio.hxx"

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <stdexcept>

using namespace atomic_install;

POSIXIOException::POSIXIOException(const char* message, const std::string& fn)
{
	// TODO: shorten too long errors
	snprintf(_formatted_message, sizeof(_formatted_message),
			"IO error: %s, file: %s, errno: %s",
			message,
			fn.c_str(),
			strerror(errno));
}

const char* POSIXIOException::what() const throw()
{
	return _formatted_message;
}

DirectoryScanner::DirectoryScanner(const std::string& name, bool omit_special)
	: _dir(opendir(name.c_str())), _path(name), _omit_special(omit_special)
{
	if (!_dir)
		throw POSIXIOException("opendir() failed", name);

	++*this;
}

DirectoryScanner::~DirectoryScanner()
{
	closedir(_dir);
}

bool DirectoryScanner::eof() const
{
	return !_curr_entry;
}

dirent& DirectoryScanner::operator*() const
{
	assert(_curr_entry);

	return *_curr_entry;
}

dirent* DirectoryScanner::operator->() const
{
	assert(_curr_entry);

	return _curr_entry;
}

DirectoryScanner& DirectoryScanner::operator++()
{
	errno = 0;
	do
	{
		_curr_entry = readdir(_dir);
		if (errno)
			throw POSIXIOException("readdir() failed", _path);
	}
	while (_omit_special && _curr_entry
			&& (!strcmp(_curr_entry->d_name, ".")
				|| !strcmp(_curr_entry->d_name, "..")));

	return *this;
}

FileType::FileType(enum_type val)
	: _val(val)
{
	assert(val < n_types);
}

FileType::operator enum_type() const
{
	return _val;
}

FileStat::FileStat(const std::string& path)
{
	if (lstat(path.c_str(), this))
		throw POSIXIOException("lstat() failed", path);
}

FileType FileStat::file_type()
{
	if (S_ISREG(st_mode))
		return FileType::regular_file;
	else if (S_ISDIR(st_mode))
		return FileType::directory;
	else
		// TODO: a better exception
		throw std::runtime_error("unknown file type");
}

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

StdIOFile::StdIOFile(const std::string& path, const char* mode)
	: _f(fopen(path.c_str(), mode)), _path(path)
{
	if (!_f)
		throw POSIXIOException("fopen() failed", path);
}

StdIOFile::~StdIOFile()
{
	if (fclose(_f))
		throw POSIXIOException("fclose() failed", _path);
}

StdIOFile::operator FILE*()
{
	return _f;
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

std::string BinMD5::as_hex()
{
	std::string ret;
	char digits[] = "0123456789abcdef";

	for (int i = 0; i < 16; ++i)
	{
		ret.push_back(digits[(data[i] & 0xf0) >> 4]);
		ret.push_back(digits[data[i] & 0x0f]);
	}

	return ret;
}

MD5Counter::MD5Counter(const std::string& path)
	: _path(path)
{
	if (!MD5_Init(&_ctx))
		throw POSIXIOException("MD5_Init() failed", path);
}

void MD5Counter::feed(const char* buf, unsigned long len)
{
	if (!MD5_Update(&_ctx, buf, len))
		throw POSIXIOException("MD5_Update() failed", _path);
}

void MD5Counter::finish(BinMD5& out)
{
	if (!MD5_Final(out.data, &_ctx))
		throw POSIXIOException("MD5_Final() failed", _path);
}

FileStat::FileStat(const std::string& path)
{
	if (lstat(path.c_str(), this))
		throw POSIXIOException("lstat() failed", path);

	// count md5 for regular files
	if (file_type() == FileType::regular_file)
	{
		StdIOFile f(path);
		MD5Counter md5(path);

		while (!feof(f))
		{
			char buf[4096];
			size_t ret = fread(buf, 1, sizeof(buf), f);

			if (ret < sizeof(buf) && ferror(f))
				throw POSIXIOException("fread() failed", path);
			md5.feed(buf, ret);
		}

		md5.finish(_md5);
	}
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

BinMD5 FileStat::data_md5()
{
	if (file_type() == FileType::regular_file)
		return _md5;
	else
		throw std::logic_error("data_md5() is valid on regular files only");
}

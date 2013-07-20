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
#include <cstdlib>
#include <cstring>
#include <stdexcept>

extern "C"
{
#	include <unistd.h>
};

using namespace atomic_install;

POSIXIOException::POSIXIOException(const char* message, const std::string& fn)
	: sys_errno(errno)
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

CopyFile::CopyFile(std::string& from, std::string& to)
	: _from(from), _to(to)
{
}

void CopyFile::wrap_error(copyfile_error_t ret)
{
	// TODO: proper choice of from/to
	if (ret)
		throw POSIXIOException(copyfile_error_message(ret), _from);
}

void CopyFile::move()
{
	wrap_error(copyfile_move_file(_from.c_str(), _to.c_str(), 0, 0, 0));
}

void CopyFile::link_or_copy()
{
	wrap_error(copyfile_link_file(_from.c_str(), _to.c_str(), 0, 0, 0));
}

void CopyFile::copy()
{
	wrap_error(copyfile_archive_file(_from.c_str(), _to.c_str(),
				0, 0, 0, 0, 0));
}

void CopyFile::copy_metadata()
{
	wrap_error(copyfile_copy_metadata(_from.c_str(), _to.c_str(), 0, 0, 0));
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

StdIOFile::StdIOFile(const std::string& path, int dummy)
	: _f(0), _path(path)
{
}

StdIOFile::~StdIOFile()
{
	if (fclose(_f))
		throw POSIXIOException("fclose() failed", _path);
}

size_t StdIOFile::read(void* _buf, size_t len)
{
	char* buf = static_cast<char*>(_buf);
	size_t sret = 0;

	do
	{
		size_t ret = fread(buf, 1, len, _f);

		if (ret < len && ferror(_f))
			throw POSIXIOException("fread() failed", _path);

		sret += ret;
		buf += ret;
		len -= ret;

		if (ret == 0 && feof(_f))
			break;
	} while (len > 0);

	return sret;
}

void StdIOFile::read_exact(void* buf, size_t len)
{
	if (read(buf, len) < len)
		throw POSIXIOException("Short read occured", _path);
}

void StdIOFile::write(const void* _buf, size_t len)
{
	const char* buf = static_cast<const char*>(_buf);

	do
	{
		size_t ret = fwrite(buf, 1, len, _f);

		if (ret < len && ferror(_f))
			throw POSIXIOException("fwrite() failed", _path);

		buf += ret;
		len -= ret;
	} while (len > 0);
}

void StdIOFile::read_string(std::string& out)
{
	size_t len;
	char buf[4096];

	read_exact(&len, sizeof(len));
	out.clear();
	out.reserve(len);

	while (len > 0)
	{
		size_t rd = len > sizeof(buf) ? sizeof(buf) : len;
		read_exact(buf, rd);
		out.append(buf, rd);

		len -= rd;
	};
}

void StdIOFile::write_string(const std::string& s)
{
	size_t len = s.size();

	write(&len, sizeof(len));
	write(s.data(), len);
}

StdIOFile::operator FILE*()
{
	return _f;
}

AtomicIOFile::AtomicIOFile(const std::string& path, const char* mode)
	: StdIOFile(path, 1)
{
	char* tmpl = new char[path.size() + 8];
	const char template_suffix[] = ".XXXXXX";

	memcpy(tmpl, path.data(), path.size());
	memcpy(tmpl + path.size(), template_suffix, sizeof(template_suffix));

	int fd = mkstemp(tmpl);

	if (fd != -1)
		_tmp_path = tmpl;
	delete tmpl;

	if (fd == -1)
		throw POSIXIOException("mkstemp() failed", path);
	_f = fdopen(fd, mode);
}

AtomicIOFile::~AtomicIOFile()
{
	copyfile_error_t ret = copyfile_move_file(
			_tmp_path.c_str(), _path.c_str(), 0, 0, 0);

	if (ret)
		throw POSIXIOException(copyfile_error_message(ret), _tmp_path);
}

FileType::FileType(enum_type val)
	: _val(val)
{
	assert(val < n_types);
}

FileType::FileType()
	: _val(n_types)
{
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
			size_t ret = f.read(buf, sizeof(buf));

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

void atomic_install::remove_file(const std::string& path, bool ignore_nonexist)
{
	if (unlink(path.c_str()))
	{
		if (ignore_nonexist && errno == ENOENT)
			return;
		throw POSIXIOException("Unable to unlink file", path);
	}
}

void atomic_install::remove_dir(const std::string& path, bool ignore_nonexist,
		bool ignore_nonempty)
{
	if (rmdir(path.c_str()))
	{
		if (ignore_nonexist && errno == ENOENT)
			return;
		if (ignore_nonempty && (errno == EEXIST || errno == ENOTEMPTY))
			return;
		throw POSIXIOException("Unable to remove directory", path);
	}
}

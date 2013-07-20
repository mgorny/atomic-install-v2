/**
 * atomic-install
 * (c) 2013 Michał Górny
 * Released under the 2-clause BSD license
 */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include "journal.hxx"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <iostream>
#include <stdexcept>

extern "C"
{
#	include <libcopyfile.h>
};

using namespace atomic_install;

File::File(const char* rel_path, const std::string& full_path)
	: path(rel_path), file_type(FileType::regular_file),
	existed(false)
{
	FileStat st(full_path);

	file_type = st.file_type();
	if (file_type == FileType::regular_file)
	{
		md5 = st.data_md5();
		mtime = st.st_mtime;
	}
}

File::File()
{
}

const std::string PathBuffer::_def_prefix = "";

PathBuffer::PathBuffer(const std::string& root, const std::string& prefix)
	: std::string(root), _prefix(prefix)
{
	_prefix_len = size();
	_directory_len = _prefix_len;
}

void PathBuffer::set_directory(const std::string& rel_path)
{
	assert(rel_path[0] == '/');

	erase(_prefix_len);
	*this += rel_path;
	if (rel_path != "/")
		*this += '/';
	_directory_len = size();
}

void PathBuffer::set_filename(const std::string& filename)
{
	erase(_directory_len);
	*this += _prefix;
	*this += filename;
}

void PathBuffer::set_path(const std::string& path)
{
	erase(_prefix_len);
	*this += path;

	int fn_pos = rfind('/');
	assert(fn_pos != std::string::npos);

	insert(fn_pos + 1, _prefix);
}

const char* PathBuffer::get_relative_path() const
{
	return c_str() + _prefix_len;
}

Journal::Journal(const std::string& source, const std::string& dest)
	: _source(source),
	_dest(dest)
{
	// generate semi-random prefix
	time_t t = time(0);

	std::string prefix;
	for (int i = 0; i < sizeof(t)*8; i += 6, t >>= 6)
	{
		// we're losing a few bits here but it doesn't matter
		if (t % 27)
			prefix += 'A' + (t % 27 - 1) | (t & 32);
	}

	_new_prefix = ".AIn~" + prefix + '.';
	_backup_prefix = ".AIb~" + prefix + '.';
}

static const char magic_start[4] = { 'A', 'I', 'j', '!' };
static const char magic_end[4] = { '!', 'A', 'I', 'j' };

enum file_flags
{
	FILE_EXISTED = 1,
};

void Journal::save_journal(const char* path)
{
	AtomicIOFile f(path, "wb");

	f.write(magic_start, sizeof(magic_start));
	f.write_string(_source);
	f.write_string(_dest);

	f.write_string(_new_prefix);
	f.write_string(_backup_prefix);

	for (_files_type::iterator i = _files.begin();
			i != _files.end();
			++i)
	{
		unsigned char ft = i->file_type;
		f.write(&ft, sizeof(ft));

		f.write_string(i->path);
		f.write(i->md5.data, sizeof(i->md5.data));
		f.write(&i->mtime, sizeof(i->mtime));

		unsigned int flags = 0;
		if (i->existed)
			flags |= FILE_EXISTED;
		f.write(&flags, sizeof(flags));
	}

	// terminator
	unsigned char ft = FileType::n_types;
	f.write(&ft, sizeof(ft));

	// another magic to ensure complete write
	f.write(magic_end, sizeof(magic_end));
}

Journal Journal::read_journal(const char* path)
{
	StdIOFile f(path);

	char magic_buf[sizeof(magic_start)];
	f.read_exact(magic_buf, sizeof(magic_buf));
	if (memcmp(magic_buf, magic_start, sizeof(magic_buf)))
		throw std::runtime_error("Journal magic invalid.");

	std::string source, dest;
	f.read_string(source);
	f.read_string(dest);

	Journal j(source, dest);

	f.read_string(j._new_prefix);
	f.read_string(j._backup_prefix);

	while (1)
	{
		unsigned char ft;
		f.read_exact(&ft, sizeof(ft));

		if (ft == FileType::n_types)
			break;

		File sub_f;
		sub_f.file_type = static_cast<FileType::enum_type>(ft);
		f.read_string(sub_f.path);
		f.read_exact(sub_f.md5.data, sizeof(sub_f.md5));
		f.read_exact(&sub_f.mtime, sizeof(sub_f.mtime));

		unsigned int flags;
		f.read_exact(&flags, sizeof(flags));
		if (flags & FILE_EXISTED)
			sub_f.existed = true;

		j._files.push_back(sub_f);
	}

	char magic_buf2[sizeof(magic_end)];
	f.read_exact(magic_buf2, sizeof(magic_buf2));
	if (memcmp(magic_buf2, magic_end, sizeof(magic_buf2)))
		throw std::runtime_error("Journal end magic invalid.");

	return j;
}

void Journal::scan_files()
{
	PathBuffer path_buf(_source);

	File root("/", _source);
	_files.push_back(root);

	// directories change _files, so we need to use indexes
	// but for quickly iterating files we can just use iterator
	for (_files_type::size_type start_n = 0; start_n < _files.size();)
	{
		// build the full path
		path_buf.set_directory(_files[start_n].path);
		++start_n;

		// scan the directory
		for (DirectoryScanner dir(path_buf, true);
				!dir.eof(); ++dir)
		{
			path_buf.set_filename(dir->d_name);

			File f(path_buf.get_relative_path(), path_buf);
			_files.push_back(f);
		}

		// skip files
		for (_files_type::iterator i = _files.begin() + start_n;
				i->file_type != FileType::directory && i != _files.end();
				++i, ++start_n);
	}
}

void Journal::copy_files()
{
	PathBuffer src_buf(_source);
	PathBuffer dst_buf(_dest, _new_prefix);

	for (_files_type::iterator i = _files.begin(); i != _files.end(); ++i)
	{
		File& f = *i;

		if (f.file_type == FileType::directory)
		{
			std::cerr << "--- ";
			src_buf.set_directory(f.path);
			dst_buf.set_directory(f.path);
		}
		else
		{
			std::cerr << ">>> ";
			src_buf.set_path(f.path);
			dst_buf.set_path(f.path);
		}

		std::cerr << f.path << std::endl;

		CopyFile cf(src_buf, dst_buf);

		if (f.file_type == FileType::directory)
		{
			try
			{
				cf.copy();
			}
			catch (POSIXIOException& e)
			{
				if (e.sys_errno == EEXIST)
					cf.copy_metadata();
				else
					throw;
			};
		}
		else
			cf.link_or_copy();
	}
}

void Journal::backup_files()
{
	PathBuffer src_buf(_dest);
	PathBuffer dst_buf(_dest, _backup_prefix);

	for (_files_type::iterator i = _files.begin(); i != _files.end(); ++i)
	{
		File& f = *i;

		if (f.file_type == FileType::directory)
			continue;

		src_buf.set_path(f.path);
		dst_buf.set_path(f.path);

		CopyFile cf(src_buf, dst_buf);

		try
		{
			cf.link_or_copy();

			f.existed = true;
		}
		catch (POSIXIOException& e)
		{
			// ignore missing files, no need to back them up :).
			if (e.sys_errno != ENOENT)
				throw;
		}
	}
}

void Journal::replace()
{
	PathBuffer src_buf(_dest, _new_prefix);
	PathBuffer dst_buf(_dest);

	for (_files_type::iterator i = _files.begin(); i != _files.end(); ++i)
	{
		File& f = *i;

		if (f.file_type == FileType::directory)
			continue;

		src_buf.set_path(f.path);
		dst_buf.set_path(f.path);

		CopyFile cf(src_buf, dst_buf);

		cf.move();
	}
}

void Journal::cleanup()
{
	PathBuffer new_buf(_dest, _new_prefix);
	PathBuffer backup_buf(_dest, _backup_prefix);

	for (_files_type::iterator i = _files.begin(); i != _files.end(); ++i)
	{
		File& f = *i;

		if (f.file_type == FileType::directory)
			continue;

		new_buf.set_path(f.path);
		backup_buf.set_path(f.path);

		remove_file(new_buf, true);
		remove_file(backup_buf, true);
	}
}

void Journal::revert()
{
	PathBuffer src_buf(_dest, _backup_prefix);
	PathBuffer dst_buf(_dest);

	for (_files_type::iterator i = _files.begin(); i != _files.end(); ++i)
	{
		File& f = *i;

		if (f.file_type == FileType::directory)
			continue;

		src_buf.set_path(f.path);
		dst_buf.set_path(f.path);

		if (f.existed)
		{
			CopyFile cf(src_buf, dst_buf);

			try
			{
				cf.move();
			}
			catch (POSIXIOException& e)
			{
				// The files may have been reverted already,
				// i.e. we can be doing a second retry.
				if (e.sys_errno != ENOENT)
					throw;
			}
		}
		else
			remove_file(dst_buf, true);
	}
}

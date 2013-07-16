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
#include <iostream>

using namespace atomic_install;

File::File(const char* rel_path, const std::string& full_path)
	: path(rel_path), file_type(FileType::regular_file)
{
	FileStat st(full_path);

	file_type = st.file_type();

	std::cerr << full_path << " (" << rel_path << "):" << file_type << std::endl;
}

PathBuffer::PathBuffer(const std::string& root)
	: std::string(root)
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
	*this += filename;
}

const char* PathBuffer::get_relative_path() const
{
	return c_str() + _prefix_len;
}

Journal::Journal(const char* source, const char* dest)
	: _source(source),
	_dest(dest)
{
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

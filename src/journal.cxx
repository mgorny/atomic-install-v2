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
#include <cstring>
#include <stdexcept>

using namespace atomic_install;

File::File(const char* rel_path, const std::string& full_path)
	: path(rel_path), file_type(FileType::regular_file)
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

Journal::Journal(const std::string& source, const std::string& dest)
	: _source(source),
	_dest(dest)
{
}

static const char magic_start[4] = { 'A', 'I', 'j', '!' };
static const char magic_end[4] = { '!', 'A', 'I', 'j' };

void Journal::save_journal(const char* path)
{
	AtomicIOFile f(path, "wb");

	f.write(magic_start, sizeof(magic_start));
	f.write_string(_source);
	f.write_string(_dest);

	for (_files_type::iterator i = _files.begin();
			i != _files.end();
			++i)
	{
		unsigned char ft = i->file_type;
		f.write(&ft, sizeof(ft));

		f.write_string(i->path);
		f.write(i->md5.data, sizeof(i->md5.data));
		f.write(&i->mtime, sizeof(i->mtime));
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

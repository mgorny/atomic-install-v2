/**
 * atomic-install
 * (c) 2013 Michał Górny
 * Released under the 2-clause BSD license
 */

#pragma once

#ifndef JOURNAL_HXX
#define JOURNAL_HXX 1

#include "posixio.hxx"

#include <string>
#include <vector>

namespace atomic_install
{
	class File
	{
	public:
		File(const char* rel_path, const std::string& root_path);
		File();

		std::string path;
		FileType file_type;
		BinMD5 md5;
		time_t mtime;

		bool existed;
	};

	class PathBuffer : public std::string
	{
		std::string::size_type _prefix_len;
		std::string::size_type _directory_len;

		static const std::string _def_prefix;

		const std::string& _prefix;

	public:
		PathBuffer(const std::string& root, const std::string& prefix = _def_prefix);

		void set_directory(const std::string& rel_path);
		void set_filename(const std::string& filename);
		void set_path(const std::string& path);

		const char* get_relative_path() const;
	};

	class Journal
	{
		std::string _source;
		std::string _dest;

		std::string _new_prefix, _backup_prefix;

		typedef std::vector<File> _files_type;
		_files_type _files;

		void build_path(std::string& buf,
				const std::string& root,
				const std::string& rel_path);

	public:
		// Instantiate a new, empty journal for copying files
		// from @source to @dest.
		Journal(const std::string& source, const std::string& dest);

		void save_journal(const char* path);
		static Journal read_journal(const char* path);

		// Scan source directory and add files from it to the journal.
		void scan_files();
		// Copy new files to destdir.
		void copy_files();
		// Backup existing files in destdir.
		void backup_files();
		// Perform the atomic replacement.
		void replace();
		// Remove new & backup files.
		void cleanup();

		// Revert the started replacement.
		void revert();
	};
};

#endif /*JOURNAL_HXX*/

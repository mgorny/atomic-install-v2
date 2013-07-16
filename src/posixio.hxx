/**
 * atomic-install
 * (c) 2013 Michał Górny
 * Released under the 2-clause BSD license
 */

#pragma once

#ifndef POSIXIO_HXX
#define POSIXIO_HXX 1

#include <exception>
#include <iterator>
#include <string>

extern "C"
{
#	include <sys/stat.h>
#	include <sys/types.h>
#	include <dirent.h>
};

namespace atomic_install
{
	class POSIXIOException : public std::exception
	{
		char _formatted_message[512];

	public:
		POSIXIOException(const char* message, const std::string& fn);

		virtual const char* what() const throw();
	};

	class DirectoryScanner : public std::iterator<
				std::input_iterator_tag, struct dirent>
	{
		DIR* _dir;
		std::string _path;
		dirent* _curr_entry;

		bool _omit_special;

	public:
		DirectoryScanner(const std::string& name, bool omit_special = false);
		~DirectoryScanner();

		bool eof() const;
		dirent& operator*() const;
		dirent* operator->() const;

		DirectoryScanner& operator++();
	};

	class FileType
	{
	public:
		typedef enum
		{
			regular_file,
			directory,

			// TODO: more types

			n_types
		} enum_type;

	private:
		enum_type _val;

	public:
		FileType(enum_type val);
		operator enum_type() const;
	};

	class FileStat : public stat
	{
	public:
		FileStat(const std::string& path);

		FileType file_type();
	};
};

#endif /*POSIXIO_HXX*/

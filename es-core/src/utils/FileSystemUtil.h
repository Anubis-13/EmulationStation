#pragma once
#ifndef ES_CORE_UTILS_FILE_SYSTEM_UTIL_H
#define ES_CORE_UTILS_FILE_SYSTEM_UTIL_H

#include <list>
#include <string>
#include <pugixml/src/pugixml.hpp>

namespace Utils
{
	namespace FileSystem
	{
		struct FileInfo
		{
		public:
			std::string path;
			bool hidden;
			bool directory;
		};

		typedef std::list<std::string> stringList;
		typedef std::list<FileInfo> fileList;

		fileList  getDirInfo(const std::string& _path/*, const bool _recursive = false*/);
		stringList  getDirContent      (const std::string& _path, const bool _recursive = false, const bool includeHidden = true);
		stringList  getPathList        (const std::string& _path);
		std::string getHomePath        ();
		std::string getCWDPath         ();
		std::string getExePath         ();
		std::string getPreferredPath   (const std::string& _path);
		std::string getGenericPath     (const std::string& _path);
		std::string getEscapedPath     (const std::string& _path);
		std::string getCanonicalPath   (const std::string& _path);
		std::string getAbsolutePath    (const std::string& _path, const std::string& _base = getCWDPath());
		std::string getParent          (const std::string& _path);
		std::string getFileName        (const std::string& _path);
		std::string getStem            (const std::string& _path);
		std::string getExtension       (const std::string& _path);
		std::string resolveRelativePath(const std::string& _path, const std::string& _relativeTo, const bool _allowHome);
		std::string createRelativePath (const std::string& _path, const std::string& _relativeTo, const bool _allowHome);
		std::string removeCommonPath   (const std::string& _path, const std::string& _common, bool& _contains);
		std::string resolveSymlink     (const std::string& _path);
		std::string combine(const std::string& _path, const std::string& filename);
		bool        removeFile         (const std::string& _path);
		bool        createDirectory    (const std::string& _path);
		bool        exists             (const std::string& _path);
		size_t		getFileSize(const std::string& _path);
		bool        isAbsolute         (const std::string& _path);
		bool        isRegularFile      (const std::string& _path);
		bool        isDirectory        (const std::string& _path);
		bool        isSymlink          (const std::string& _path);
		bool        isHidden           (const std::string& _path);

		void		setHomePath		   (std::string path);


		pugi::xml_parse_result	load_xml(pugi::xml_document& doc, const char* path);

	

	} // FileSystem::



} // Utils::

#endif // ES_CORE_UTILS_FILE_SYSTEM_UTIL_H

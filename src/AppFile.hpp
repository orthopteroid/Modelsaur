#ifndef _APPFILE_HPP_
#define _APPFILE_HPP_

// Copyright 2025 orthopteroid@gmail.com, MIT License

struct AppFile
{
    FILE* pFile = 0;

	enum file_type { Library, Preferences };
	enum file_mode { WriteMode, ReadMode };

	AppFile(const char* szFilename, file_type ft, file_mode fm);
	virtual ~AppFile();

	void Test();
	void Printf(const char* format, ...);
	void Scanf(const char* format, ...);
};

#endif //_APPFILE_HPP_

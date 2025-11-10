#ifndef _ANDROIDFILE_HPP_
#define _ANDROIDFILE_HPP_

// Copyright 2025 orthopteroid@gmail.com, MIT License

struct PlatformFile
{
	int fint = 0;

	enum file_type { Library, Preferences };
	
	PlatformFile();
	virtual ~PlatformFile();

	virtual void Open(file_type ft, char* szFilename);
	virtual void Close();
	
	virtual void Test();
};

#endif //_ANDROIDFILE_HPP_

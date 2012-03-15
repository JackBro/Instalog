#pragma once
#include <ostream>
#include <string>
#include <vector>

namespace Instalog {

	//time here is a FILETIME structure (though we're passing a plain __int64 to
	//avoid having to include Windows.h here)
	void WriteDefaultDateFormat(std::wostream &str, unsigned __int64 time);
	void WriteMillisecondDateFormat(std::wostream &str, unsigned __int64 time);
	void WriteFileAttributes(std::wostream &str, unsigned __int64 time);
	void WriteDefaultFileOutput(std::wostream &str, std::wstring const& targetFile);
	void WriteFileListingFile(std::wostream &str, std::wstring const& targetFile);

}
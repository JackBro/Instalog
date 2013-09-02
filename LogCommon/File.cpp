// Copyright © 2012 Jacob Snyder, Billy O'Neal III
// This is under the 2 clause BSD license.
// See the included LICENSE.TXT file for more details.

#include "pch.hpp"
#include "File.hpp"
#include <algorithm>
#include "Win32Exception.hpp"
#include <boost/algorithm/string/predicate.hpp>

#pragma comment(lib, "Version.lib")

namespace Instalog { namespace SystemFacades {

    File::File( 
        std::wstring const& filename, 
        DWORD desiredAccess, 
        DWORD shareMode, 
        LPSECURITY_ATTRIBUTES securityAttributes, 
        DWORD createdDisposition, 
        DWORD flags
        )
        : hFile(::CreateFileW(filename.c_str(), desiredAccess, shareMode, securityAttributes, createdDisposition, flags, nullptr))
    {
        if (hFile == INVALID_HANDLE_VALUE)
        {
            Win32Exception::ThrowFromLastError();
        }
    }

    File::File( File && other )
    {
        hFile = other.hFile;
        other.hFile = INVALID_HANDLE_VALUE;
    }

    File::File()
        : hFile(INVALID_HANDLE_VALUE)
    {
    }

    File::~File()
    {
        if (hFile == INVALID_HANDLE_VALUE)
        {
            return;
        }
        ::CloseHandle(hFile);
    }

    std::uint64_t File::GetSize() const
    {
        BY_HANDLE_FILE_INFORMATION info = GetExtendedAttributes();

        std::uint64_t highSize = info.nFileSizeHigh;
        highSize <<= 32;
        return highSize + info.nFileSizeLow;
    }

    DWORD File::GetAttributes() const
    {
        return GetExtendedAttributes().dwFileAttributes;
    }

    BY_HANDLE_FILE_INFORMATION File::GetExtendedAttributes() const
    {
        BY_HANDLE_FILE_INFORMATION info;

        if (GetFileInformationByHandle(hFile, &info) == false)
        {
            Win32Exception::ThrowFromLastError();
        }

        return info;
    }

    std::vector<char> File::ReadBytes( unsigned int bytesToRead ) const
    {
        std::vector<char> bytes(bytesToRead);
        DWORD bytesRead = 0;

        if (::ReadFile(hFile, bytes.data(), bytesToRead, &bytesRead, NULL) == false)
        {
            Win32Exception::ThrowFromLastError();
        }

        if (bytesRead < bytesToRead)
        {
            bytes.resize(bytesRead);
        }

        return bytes;
    }

    bool File::WriteBytes( std::vector<char> const& bytes )
    {
        DWORD bytesWritten;

        if (WriteFile(hFile, bytes.data(), static_cast<DWORD>(bytes.size()), &bytesWritten, NULL) == false)
        {
            Win32Exception::ThrowFromLastError();
        }

        if (bytesWritten == bytes.size())
            return true;
        else
            return false;
    }

    void File::Delete( std::wstring const& filename)
    {
        if (::DeleteFileW(filename.c_str()) == 0) 
        {
            Win32Exception::ThrowFromLastError();
        }
    }

    bool File::Exists( std::wstring const& filename)
    {
        DWORD attributes = ::GetFileAttributesW(filename.c_str());

        return attributes != INVALID_FILE_ATTRIBUTES;
    }

    bool File::IsDirectory( std::wstring const& filename)
    {
        DWORD attributes = ::GetFileAttributesW(filename.c_str());

        return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 && (attributes != INVALID_FILE_ATTRIBUTES);
    }

    bool File::IsExecutable( std::wstring const& filename )
    {
        if (Exists(filename) == false)
            return false;
        if (IsDirectory(filename))
            return false;

        File executable(filename, FILE_READ_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING);
        std::vector<char> bytes = executable.ReadBytes(2);
        return bytes.size() >=2 && bytes[0] == 'M' && bytes[1] == 'Z';
    }

    std::wstring File::GetCompany(std::wstring const& filename)
    {
        DWORD infoSize = ::GetFileVersionInfoSizeW(filename.c_str(), 0);
        if (infoSize == 0)
        {
            Win32Exception::ThrowFromLastError();
        }
        std::vector<char> buff(infoSize);
        if (::GetFileVersionInfoW(filename.c_str(), 0, static_cast<DWORD>(buff.size()), buff.data()) == 0)
        {
            Win32Exception::ThrowFromLastError();
        }
        wchar_t const targetPath[] = L"\\StringFileInfo\\040904B0\\CompanyName";
        void * companyData;
        UINT len;
        if (::VerQueryValueW(buff.data(), targetPath, &companyData, &len) == 0)
        {
            Win32Exception::ThrowFromLastError();
        }
        if (len == 0)
        {
            return std::wstring();
        }
        else
        {
            return std::wstring(static_cast<wchar_t *>(companyData), len-1);
        }
    }

    std::uint64_t File::GetSize( std::wstring const& filename )
    {
        WIN32_FILE_ATTRIBUTE_DATA fad = File::GetExtendedAttributes(filename);
        std::uint64_t size = fad.nFileSizeHigh;
        size <<= 32;
        size |= fad.nFileSizeLow;
        return size;
    }

    DWORD File::GetAttributes( std::wstring const& filename )
    {
        DWORD answer = ::GetFileAttributesW(filename.c_str());
        if (answer == INVALID_FILE_ATTRIBUTES)
        {
            Win32Exception::ThrowFromLastError();
        }
        return answer;
    }

    WIN32_FILE_ATTRIBUTE_DATA File::GetExtendedAttributes( std::wstring const& filename )
    {
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (::GetFileAttributesExW(filename.c_str(), GetFileExInfoStandard, &fad) == 0)
        {
            Win32Exception::ThrowFromLastError();
        }
        return fad;
    }

    File& File::operator=( File other )
    {
        File copied(std::move(other));
        std::swap(this->hFile, copied.hFile);
        return *this;
    }

    bool File::IsExclusiveFile( std::wstring const& fileName )
    {
        DWORD attribs = ::GetFileAttributesW(fileName.c_str());
        if (attribs == INVALID_FILE_ATTRIBUTES)
        {
            return false;
        }
        else
        {
            return (attribs & FILE_ATTRIBUTE_DIRECTORY) == 0;
        }
    }

    FindFilesRecord::FindFilesRecord(std::wstring prefix, WIN32_FIND_DATAW const& winSource)
        : dwFileAttributes(winSource.dwFileAttributes)
    {
        prefix.append(winSource.cFileName);
        cFileName = std::move(prefix);
        ftCreationTime = (static_cast<std::uint64_t>(winSource.ftCreationTime.dwHighDateTime) << 32) | winSource.ftCreationTime.dwLowDateTime;
        ftLastAccessTime = (static_cast<std::uint64_t>(winSource.ftLastAccessTime.dwHighDateTime) << 32) | winSource.ftLastAccessTime.dwLowDateTime;
        ftLastWriteTime = (static_cast<std::uint64_t>(winSource.ftLastWriteTime.dwHighDateTime) << 32) | winSource.ftLastWriteTime.dwLowDateTime;
        nFileSize = (static_cast<std::uint64_t>(winSource.nFileSizeHigh) << 32) | winSource.nFileSizeLow;
    }

    FindFilesRecord::FindFilesRecord(FindFilesRecord const& other)
        : cFileName(other.cFileName)
        , ftCreationTime(other.ftCreationTime)
        , ftLastAccessTime(other.ftLastAccessTime)
        , ftLastWriteTime(other.ftLastWriteTime)
        , nFileSize(other.nFileSize)
        , dwFileAttributes(other.dwFileAttributes)
    { }

    FindFilesRecord::FindFilesRecord(FindFilesRecord&& other) throw()
        : cFileName(std::move(other.cFileName))
        , ftCreationTime(other.ftCreationTime)
        , ftLastAccessTime(other.ftLastAccessTime)
        , ftLastWriteTime(other.ftLastWriteTime)
        , nFileSize(other.nFileSize)
        , dwFileAttributes(other.dwFileAttributes)
    { }

    FindFilesRecord& FindFilesRecord::operator=(FindFilesRecord other)
    {
        other.swap(*this);
        return *this;
    }

    std::wstring const& FindFilesRecord::GetFileName() const throw()
    {
        return cFileName;
    }

    std::uint64_t FindFilesRecord::GetCreationTime() const throw()
    {
        return ftCreationTime;
    }

    std::uint64_t FindFilesRecord::GetLastAccessTime() const throw()
    {
        return ftLastAccessTime;
    }

    std::uint64_t FindFilesRecord::GetLastWriteTime() const throw()
    {
        return ftLastWriteTime;
    }

    std::uint64_t FindFilesRecord::GetSize() const throw()
    {
        return nFileSize;
    }

    DWORD FindFilesRecord::GetAttributes() const throw()
    {
        return dwFileAttributes;
    }

    void FindFilesRecord::swap(FindFilesRecord &other) throw()
    {
        using std::swap;
        swap(cFileName, other.cFileName);
        swap(ftCreationTime, other.ftCreationTime);
        swap(ftLastAccessTime, other.ftLastAccessTime);
        swap(ftLastWriteTime, other.ftLastWriteTime);
        swap(nFileSize, other.nFileSize);
        swap(dwFileAttributes, other.dwFileAttributes);
    }

    /// <summary>Low level find handle.</summary>
    /// <seealso cref="T:boost::noncopyable"/>
    class FindHandle : private boost::noncopyable
    {
        HANDLE hFind;
        std::size_t index;
    public:
        bool IsInvalid() const throw() { return this->hFind == INVALID_HANDLE_VALUE; }

        FindHandle(std::size_t index, HANDLE hInit) throw() : hFind(hInit), index(index) {}
        FindHandle(FindHandle&& other) : hFind(other.hFind), index(other.index)
        {
            other.hFind = INVALID_HANDLE_VALUE;
        }

        FindHandle& operator=(FindHandle&& toMove)
        {
            static_cast<FindHandle>(std::move(toMove)).Swap(*this);
            return *this;
        }

        void Swap(FindHandle& other) throw()
        {
            std::swap(this->hFind, other.hFind);
            std::swap(this->index, other.index);
        }

        HANDLE Get() const throw() { return this->hFind; }
        std::size_t Index() const throw() { return this->index; }

        ~FindHandle() throw()
        {
            if (!this->IsInvalid())
            {
                ::FindClose(this->hFind);
            }
        }
    };

    /// <summary>Swaps a pair of FindHandle instances.</summary>
    /// <param name="lhs">[in,out] The left hand side.</param>
    /// <param name="rhs">[in,out] The right hand side.</param>
    inline void swap(FindHandle& lhs, FindHandle& rhs) throw()
    {
        lhs.Swap(rhs);
    }

    static inline bool IsDotDirectory(wchar_t const* toCheck)
    {
        if (toCheck[0] == L'.')
        {
            if (toCheck[1] == L'.')
            {
                return toCheck[2] == L'\0';
            }
            else if (toCheck[1] == L'\0')
            {
                return true;
            }
        }

        return false;
    }

    bool FindFiles::IsRecursive() const throw()
    {
        auto const bit = static_cast<unsigned char>(FindFilesOptions::RecursiveSearch);
        return (static_cast<unsigned char>(this->options) & bit) != 0;
    }

    bool FindFiles::IncludingDotDirectories() const throw()
    {
        auto const bit = static_cast<unsigned char>(FindFilesOptions::IncludeDotDirectories);
        return (static_cast<unsigned char>(this->options) & bit) != 0;
    }

    bool FindFiles::CanEnter() const throw()
    {
        auto const attributes = this->findData.dwFileAttributes;
        bool const isDot = IsDotDirectory(this->findData.cFileName);
        bool const isDirectory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        bool const isReparse = (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
        return this->LastSuccess() && isDirectory && !isReparse && !isDot;
    }

    bool FindFiles::LastSuccess() const throw()
    {
        return this->lastError == ERROR_SUCCESS;
    }

    void FindFiles::Leave()
    {
        this->prefix.resize(this->handleStack.back().Index());
        this->handleStack.pop_back();
    }

    void FindFiles::WinEnter()
    {
        std::size_t previousSize = this->prefix.size();
        this->prefix.append(this->findData.cFileName);
        if (!this->prefix.empty())
        {
            this->prefix.push_back(L'\\');
        }

        std::size_t noPatternSize = this->prefix.size();
        this->prefix.append(this->pattern);
        FindHandle hFind(previousSize, ::FindFirstFileW(this->prefix.c_str(), &this->findData));
        this->handleStack.push_back(std::move(hFind));
        this->prefix.resize(noPatternSize);

        if (this->handleStack.back().IsInvalid())
        {
            this->lastError = ::GetLastError();
            this->Leave();
        }
        else
        {
            this->lastError = ERROR_SUCCESS;
        }

    }

    void FindFiles::WinNext()
    {
        if (::FindNextFileW(this->handleStack.back().Get(), &this->findData) == 0)
        {
            this->lastError = ::GetLastError();
        }
        else
        {
            this->lastError = ERROR_SUCCESS;
        }
    }

    FindFiles::FindFiles() throw()
        : lastError(ERROR_NO_MORE_FILES)
        , options(FindFilesOptions::LocalSearch)
    {
    }

    FindFiles::FindFiles(std::wstring const& pattern)
        : options(FindFilesOptions::LocalSearch)
    {
        this->Construct(pattern);
    }

    FindFiles::FindFiles( std::wstring const& pattern, FindFilesOptions options )
        : options(options)
    {
        this->Construct(pattern);
    }

    FindFiles::FindFiles( FindFiles&& toMove ) throw()
        : handleStack(std::move(toMove.handleStack))
        , prefix(std::move(toMove.prefix))
        , pattern(std::move(toMove.pattern))
        , lastError(toMove.lastError)
        , findData(toMove.findData)
        , options(toMove.options)
    {
        toMove.handleStack.clear();
        toMove.prefix.clear();
        toMove.pattern.clear();
        toMove.lastError = ERROR_NO_MORE_FILES;
        toMove.findData.cFileName[0] = L'\0';
    }

    FindFiles& FindFiles::operator=( FindFiles&& toMove ) throw()
    {
        static_cast<FindFiles>(std::move(toMove)).Swap(*this);
        return *this;
    }

    FindFiles::~FindFiles() throw()
    {
        // Purposely empty.
    }

    void FindFiles::Swap( FindFiles& other ) throw()
    {
        using std::swap;
        swap(this->handleStack, other.handleStack);
        swap(this->prefix, other.prefix);
        swap(this->pattern, other.pattern);
        swap(this->lastError, other.lastError);
        swap(this->findData, other.findData);
        swap(this->options, other.options);
    }

    void FindFiles::NextImpl()
    {
        bool const noHandles = this->handleStack.empty();
        if (this->LastSuccess() && noHandles)
        {
            // This is the first call to NextImpl, so make the first entrance.
            this->WinEnter();
            return;
        }
        
        if (this->IsRecursive() && this->CanEnter())
        {
            // We are doing a recursive search and can enter a directory; do that.
            this->WinEnter();
            return;
        }

        if (this->OnEndShouldLeave())
        {
            this->Leave();
            if (!this->handleStack.empty())
            {
                this->WinNext();
            }
        }
        else if (noHandles)
        {
            this->lastError = ERROR_NO_MORE_FILES;
        }
        else
        {
            this->WinNext();
        }
    }

    bool FindFiles::Next()
    {
        do
        {
            this->NextImpl();
        } while (this->OnDotKeepGoing() || this->OnEndShouldLeave());
        return this->lastError != ERROR_NO_MORE_FILES;
    }

    bool FindFiles::NextSuccess()
    {
        do
        {
            this->Next();
        } while (!(this->LastSuccess() || this->lastError == ERROR_NO_MORE_FILES));

        return this->LastSuccess();
    }

    DWORD FindFiles::LastError() const throw()
    {
        return this->lastError;
    }

    FindFilesRecord FindFiles::GetRecord() const
    {
        if (this->lastError != ERROR_SUCCESS)
        {
            Win32Exception::Throw(this->lastError);
        }
        else if (this->handleStack.empty())
        {
            throw std::logic_error("Tried to get a record before Next was called.");
        }
        else
        {
            return FindFilesRecord(this->prefix, this->findData);
        }
    }

    expected<FindFilesRecord> FindFiles::TryGetRecord() const throw()
    {
        if (this->lastError != ERROR_SUCCESS)
        {
            return expected<FindFilesRecord>::from_exception(Win32Exception::FromWinError(this->lastError));
        }
        else if (this->handleStack.empty())
        {
            return expected<FindFilesRecord>::from_exception(std::logic_error("Tried to get a record before Next was called."));
        }
        else
        {
            return FindFilesRecord(this->prefix, this->findData);
        }
    }

    void FindFiles::Construct( std::wstring const &pattern )
    {
        this->findData.cFileName[0] = L'\0';
        auto dividerPoint = std::find(pattern.crbegin(), pattern.crend(), L'\\').base();
        this->pattern.assign(dividerPoint, pattern.cend());
        if (this->pattern.empty())
        {
            this->pattern.push_back(L'*');
        }


        if (dividerPoint == pattern.begin())
        {
            // There is no prefix, completely relative path.
            return;
        }

        this->prefix.assign(pattern.begin(), --dividerPoint);
        this->lastError = ERROR_SUCCESS;
    }

    bool FindFiles::OnDotKeepGoing() throw()
    {
        return
            this->LastSuccess() &&
            IsDotDirectory(this->findData.cFileName) &&
            (!this->IncludingDotDirectories() || this->handleStack.size() != 1)
            ;
    }

    bool FindFiles::OnEndShouldLeave() throw()
    {
        return this->lastError == ERROR_NO_MORE_FILES && !this->handleStack.empty();
    }

}}

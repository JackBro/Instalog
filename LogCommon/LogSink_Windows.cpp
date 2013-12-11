// Copyright � 2012-2013 Billy O'Neal III
// This is under the 2 clause BSD license.
// See the included LICENSE.TXT file for more details.

#include "pch.hpp"
#include "LogSink.hpp"
#include <stdexcept>
#include <limits>
#include <Windows.h>
#include "Utf8.hpp"
#include "Win32Exception.hpp"

namespace Instalog
{
    file_sink::file_sink(std::string const& filePath)
        : handleValue(reinterpret_cast<std::uintptr_t>(INVALID_HANDLE_VALUE))
    {
        std::wstring widePath(utf8::ToUtf16(filePath));
        HANDLE hFile =
            ::CreateFileW(widePath.c_str(),
                          FILE_WRITE_DATA | FILE_APPEND_DATA,
                          0,
                          nullptr,
                          CREATE_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                          nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
        {
            SystemFacades::Win32Exception::ThrowFromLastError();
        }

        this->handleValue = reinterpret_cast<std::uintptr_t>(hFile);
    }

    file_sink::file_sink(file_sink&& other) BOOST_NOEXCEPT_OR_NOTHROW
        : handleValue(other.handleValue)
    {
        other.handleValue = reinterpret_cast<std::uintptr_t>(INVALID_HANDLE_VALUE);
    }

    file_sink::~file_sink() BOOST_NOEXCEPT_OR_NOTHROW
    {
        HANDLE asHandle = reinterpret_cast<HANDLE>(this->handleValue);
        if (asHandle != INVALID_HANDLE_VALUE)
        {
            ::CloseHandle(asHandle);
        }
    }

    void file_sink::append(char const* data, std::size_t dataLength)
    {
        HANDLE asHandle = reinterpret_cast<HANDLE>(this->handleValue);
        if (asHandle == INVALID_HANDLE_VALUE)
        {
            throw std::logic_error("Attempted to use a moved-from file sink.");
        }

        std::size_t maxLength = std::numeric_limits<DWORD>::max();
        if (dataLength > maxLength)
        {
            throw std::overflow_error("This append can only write the number of bytes in a DWORD at a time.");
        }

        DWORD actualLength = static_cast<DWORD>(dataLength);
        DWORD writtenLength;
        if (::WriteFile(asHandle, data, actualLength, &writtenLength, nullptr) == FALSE)
        {
            SystemFacades::Win32Exception::ThrowFromLastError();
        }

        if (writtenLength != actualLength)
        {
            throw std::length_error("Unexpected number of bytes written.");
        }
    }
}

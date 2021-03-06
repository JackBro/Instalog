// Copyright © Jacob Snyder, Billy O'Neal III
// This is under the 2 clause BSD license.
// See the included LICENSE.TXT file for more details.

#include "../LogCommon/Process.hpp"
#include <algorithm>
#include <string>
#include <windows.h>
#define PSAPI_VERSION 1
#include <Psapi.h>
#include "gtest/gtest.h"
#include <boost/algorithm/string/predicate.hpp>
#include "../LogCommon/Win32Exception.hpp"
#include "../LogCommon/Utf8.hpp"

#pragma comment(lib, "psapi.lib")

using Instalog::SystemFacades::ProcessEnumerator;
using Instalog::SystemFacades::Process;
using Instalog::SystemFacades::ErrorAccessDeniedException;

TEST(Process, CanEnumerateAndCompareToProcessIds)
{
    std::size_t currentProcess = ::GetCurrentProcessId();
    ProcessEnumerator enumerator;
    ASSERT_NE(std::find(enumerator.begin(), enumerator.end(), currentProcess),
              enumerator.end());
}

TEST(Process, CanRunConcurrentSearches)
{
    ProcessEnumerator enumerator;
    std::vector<Process> processesA;
    std::vector<Process> processesB;
    ProcessEnumerator::iterator itDoubled = enumerator.begin();
    for (ProcessEnumerator::iterator it = enumerator.begin();
         it != enumerator.end();
         ++it)
    {
        processesA.push_back(*it);
        if (itDoubled != enumerator.end())
        {
            processesB.push_back(*itDoubled);
            ++itDoubled;
        }
        if (itDoubled != enumerator.end())
        {
            processesB.push_back(*itDoubled);
            ++itDoubled;
        }
    }

    ASSERT_EQ(processesA, processesB);
}

TEST(Process, CanGetProcessExecutables)
{
    wchar_t currentProcessExecutable[MAX_PATH];
    ::GetModuleFileName(NULL, currentProcessExecutable, MAX_PATH);
    std::string baseName = utf8::ToUtf8(currentProcessExecutable);
    ProcessEnumerator enumerator;
    bool couldFindMyOwnProcess = false;
    for (Process p : enumerator)
    {
        auto const path = p.GetExecutablePath();
        if (path.is_valid() && path.get() == baseName)
        {
            couldFindMyOwnProcess = true;
        }
    }

    ASSERT_TRUE(couldFindMyOwnProcess);
}

TEST(Process, CanGetProcessCommandLines)
{
    wchar_t const* currentProcessCmdLine = ::GetCommandLineW();
    std::string baseName = utf8::ToUtf8(currentProcessCmdLine);
    ProcessEnumerator enumerator;
    bool couldFindMyOwnProcess = false;
    for (Process p : enumerator)
    {
        auto const path = p.GetCmdLine();
        if (path.is_valid() && path.get() == baseName)
        {
            couldFindMyOwnProcess = true;
        }
    }

    ASSERT_TRUE(couldFindMyOwnProcess);
}

TEST(Process, NtoskrnlIsInTheBuilding)
{
    std::wstring ntoskrnl(L"C:\\Windows\\System32\\Ntoskrnl.exe");
    ProcessEnumerator enumerator;
    for (Process p : enumerator)
    {
        ASSERT_TRUE(p.GetProcessId() != 4 || boost::iequals(ntoskrnl,
            p.GetExecutablePath().get()));
    }
}

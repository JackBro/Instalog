// Copyright © 2012 Jacob Snyder, Billy O'Neal III
// This is under the 2 clause BSD license.
// See the included LICENSE.TXT file for more details.

#include "pch.hpp"
#include <functional>
#include <atlbase.h>
#include <Sddl.h>
#include <comdef.h>
#include <Wbemidl.h>
#include "Com.hpp"
#include "SecurityCenter.hpp"
#include "StockOutputFormats.hpp"
#include "Registry.hpp"
#include "PseudoHjt.hpp"
#include "ScopeExit.hpp"

namespace Instalog {
    using namespace SystemFacades;

	std::wstring PseudoHjt::GetScriptCommand() const
	{
		return L"pseudohijackthis";
	}

	std::wstring PseudoHjt::GetName() const
	{
		return L"Pseudo HijackThis";
	}

	LogSectionPriorities PseudoHjt::GetPriority() const
	{
		return SCANNING;
	}

    /**
     * Security center output for the PseudoHJT section.
     *
     * @param [in,out] output The stream to receive the output.
     */
	static void SecurityCenterOutput( std::wostream& output )
	{
		using SystemFacades::SecurityProduct;
		auto products = SystemFacades::EnumerateSecurityProducts();
		for (auto it = products.cbegin(); it != products.cend(); ++it)
		{
			output << it->GetTwoLetterPrefix()
				<< L": [" << it->GetInstanceGuid() << L"] ";
			if (it->IsEnabled())
			{
				output << L'E';
			}
			else
			{
				output << L'D';
			}
			switch (it->GetUpdateStatus())
			{
			case SecurityProduct::OutOfDate:
				output << L'O';
				break;
			case SecurityProduct::UpToDate:
				output << L'U';
				break;
			}
			output << L' ' << it->GetName() << L'\n';
		}
	}

    /**
     * A processing function which writes the target string in the default escaped function,
     * assuming that the newline is the ending delimiter.
     *
     * @param [in,out] out    The output stream.
     * @param [in,out] target Target which should be written.
     */
    static void GeneralProcess(std::wostream& out, std::wstring& target)
    {
        GeneralEscape(target, L'#', L'\n');
        out << target;
    }

    /**
     * A processing function which writes the default file output.
     *
     * @param [in,out] out    The output stream.
     * @param [in,out] target Target name of the file to print.
     */
    static void FileProcess(std::wostream& out, std::wstring& target)
    {
        WriteDefaultFileOutput(out, target);
    }

    /**
     * A processing function which writes the target as a URL.
     *
     * @param [in,out] out    The output stream.
     * @param [in,out] target Target to write as a URL.
     */
    static void HttpProcess(std::wostream& out, std::wstring& target)
    {
        UrlEscape(target, L'#', L'\n');
        out << target;
    }

    /**
     * Executes the "do nothing" filter operation. Filters only completely empty entries.
     *
     * @param target Target for the filter operation.
     *
     * @return true if the item should be filtered, false otherwise.
     */
    static bool DoNothingFilter(RegistryValueAndData const& target)
    {
        auto str = target.GetString();
        return target.GetName().empty() && str.empty();
    }

    /**
     * Value major based enumeration; general method to enumerate a registry key, where a single
     * log line is generated per value.
     *
     * @param [in,out] output The output stream.
     * @param root            The root registry key from where the key is enumerated.
     * @param prefix          The prefix which identifies the log line in the report.
     * @param rightProcess    (optional) The processing function which generates the report of the
     *                        values' data. The default writes the data as a file.
     * @param filter          (optional) a filter function which returns true if the target should
     *                        not be displayed in the output. The default function filters only
     *                        empty entries.
     */
    static void ValueMajorBasedEnumeration(
        std::wostream& output,
        std::wstring const& root,
        std::wstring const& prefix,
        std::function<void (std::wostream& out, std::wstring& source)> rightProcess = FileProcess,
        std::function<bool(RegistryValueAndData const& entry)> filter = DoNothingFilter
        )
    {
        RegistryKey key(RegistryKey::Open(root, KEY_QUERY_VALUE));
        if (key.Invalid())
        {
            DWORD err = ::GetLastError();
            if (err == STATUS_OBJECT_NAME_NOT_FOUND)
            {
                return;
            }
            else
            {
                Win32Exception::ThrowFromNtError(::GetLastError());
            }
        }
        auto values = key.EnumerateValues();
        std::vector<std::pair<std::wstring, std::wstring>> pods;
        std::for_each(values.cbegin(), values.cend(), [&](RegistryValueAndData const& val) {
            if (filter(val))
            {
                return;
            }
            pods.emplace_back(std::pair<std::wstring, std::wstring>(val.GetName(), val.GetString()));
        });
        std::sort(pods.begin(), pods.end());
        std::for_each(pods.begin(), pods.end(), [&](std::pair<std::wstring, std::wstring>& current) {
            GeneralEscape(current.first, L'#', L']');
            output << prefix << L": [" << current.first << L"] ";
            rightProcess(output, current.second);
            output << L'\n';
        });
    }

    /**
     * Executes enumeration for run keys.
     *
     * @param [in,out] output The output stream.
     * @param runRoot         The root of the hive where the run key is located.
     * @param name            The name of the run key to enumerate.
     */
    void RunKeyOutput(std::wostream& output, std::wstring const& runRoot, std::wstring const& name)
    {
#ifdef _M_X64
        ValueMajorBasedEnumeration(output, runRoot + L"\\Software\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\" + name, name);
        ValueMajorBasedEnumeration(output, runRoot + L"\\Software\\Microsoft\\Windows\\CurrentVersion\\" + name, name + L"64");
#else
        ValueMajorBasedEnumeration(output, runRoot + L"\\Software\\Microsoft\\Windows\\CurrentVersion\\" + name, name);
#endif
    }

    /**
     * Gets the user registry hive root paths.
     *
     * @return A list of the registry hives.
     */
    static std::vector<std::wstring> EnumerateUserHives()
    {
        std::vector<std::wstring> wht;
        wht.push_back(L"\\REGISTRY\\MACHINE\\HARDWARE");
        wht.push_back(L"\\REGISTRY\\MACHINE\\SYSTEM");
        wht.push_back(L"\\REGISTRY\\MACHINE\\BCD00000000");
        wht.push_back(L"\\REGISTRY\\MACHINE\\SOFTWARE");
        wht.push_back(L"\\REGISTRY\\MACHINE\\SECURITY");
        wht.push_back(L"\\REGISTRY\\MACHINE\\SAM");
        std::sort(wht.begin(), wht.end());
        std::vector<std::wstring> hives;
        {
            RegistryKey hiveList(RegistryKey::Open(L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Control\\hivelist", KEY_QUERY_VALUE));
            if (hiveList.Invalid())
            {
                Win32Exception::ThrowFromNtError(::GetLastError());
            }
            hives = hiveList.EnumerateValueNames();
        } //Block closes key
        hives.erase(std::remove_if(hives.begin(), hives.end(), [&wht] (std::wstring const& str) -> bool {
            if (boost::algorithm::ends_with(str, L"_Classes"))
            {
                return true;
            }
            return std::binary_search(wht.cbegin(), wht.cend(), str);
        }), hives.end());
        std::sort(hives.begin(), hives.end());
        return std::move(hives);
    }

    static void CommonHjt(std::wostream& output, std::wstring const& rootKey)
    {
        RunKeyOutput(output, rootKey, L"Run");
        RunKeyOutput(output, rootKey, L"RunOnce");
        RunKeyOutput(output, rootKey, L"RunServices");
        RunKeyOutput(output, rootKey, L"RunServicesOnce");
#ifdef _M_X64
        ValueMajorBasedEnumeration(output, rootKey + L"Software\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\Run", L"ExplorerRun");
        ValueMajorBasedEnumeration(output, rootKey + L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\Run", L"ExplorerRun64");
#else
        ValueMajorBasedEnumeration(output, rootKey + L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\Run", L"ExplorerRun");
#endif
    }

    /**
     * Looks up a given SID to find its associated account name, and returns a DOMAIN\User form
     * string matching that user's name.
     *
     * @param stringSid The SID of the user in string format.
     *
     * @return The user's name and domain in DOMAIN\USER format.
     */
    static std::wstring LookupAccountNameBySid(std::wstring const& stringSid)
    {
        if (stringSid == L".DEFAULT")
        {
            return L"Default User";
        }
        PSID sidPtr = nullptr;
        ScopeExit killSidPtr([sidPtr]() { if (sidPtr != nullptr) { ::LocalFree(sidPtr); } });
        if (::ConvertStringSidToSidW(stringSid.c_str(), &sidPtr) == 0)
        {
            Win32Exception::ThrowFromLastError();
        }
        SID_NAME_USE use;
        std::wstring userName, domainName;
        userName.resize(128);
        DWORD userNameCount = static_cast<DWORD>(userName.size());
        domainName.resize(128);
        DWORD domainNameCount = static_cast<DWORD>(domainName.size());
        if (::LookupAccountSidW(nullptr, sidPtr, &userName[0], &userNameCount, &domainName[0], &domainNameCount, &use) == 0)
        {
            if (userName.size() != static_cast<std::size_t>(userNameCount) || domainName.size() != static_cast<std::size_t>(domainNameCount))
            {
                userName.resize(userNameCount);
                domainName.resize(domainNameCount);
                if (::LookupAccountSidW(nullptr, sidPtr, &userName[0], &userNameCount, &domainName[0], &domainNameCount, &use) == 0)
                {
                    Win32Exception::ThrowFromLastError();
                }
            }
            else
            {
                Win32Exception::ThrowFromLastError();
            }
        }
        userName.resize(userNameCount);
        domainName.resize(domainNameCount);
        domainName.push_back(L'\\');
        return domainName + userName;
    }

	void PseudoHjt::Execute(
		std::wostream& output,
		ScriptSection const&,
		std::vector<std::wstring> const&
	) const
	{
        auto hives = EnumerateUserHives();
		SecurityCenterOutput(output);
        CommonHjt(output, L"\\Registry\\Machine");
        std::for_each(hives.cbegin(), hives.cend(), [&output](std::wstring const& hive) {
            std::wstring head(L"User Pseudo Hijack This");
            Header(head);
            std::wstring sid(std::find(hive.crbegin(), hive.crend(), L'\\').base(), hive.end());
            std::wstring user(LookupAccountNameBySid(sid));
            GeneralEscape(user, L'#', L']');
            output << L'\n' << head << L"\n\nIdentity: [" << user << L"] " << sid << L'\n';
            CommonHjt(output, hive);
        });
	}

}

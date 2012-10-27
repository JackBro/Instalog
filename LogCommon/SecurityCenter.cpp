// Copyright © 2012 Jacob Snyder, Billy O'Neal III
// This is under the 2 clause BSD license.
// See the included LICENSE.TXT file for more details.

#include "pch.hpp"
#include <iostream>
#define _WIN32_DCOM
#include <windows.h>
#include <comdef.h>
#include <wbemidl.h>
#include "Win32Exception.hpp"
#include "Wmi.hpp"
#include "SecurityCenter.hpp"
#include "IlTrace.hpp"
#include "Com.hpp"

namespace Instalog { namespace SystemFacades {

static const wchar_t avCode[] = L"AV";
static const wchar_t fwCode[] = L"FW";
static const wchar_t asCode[] = L"AS";

static void SecCenterProductCheck( 
	CComPtr<IWbemServices> securityCenter, 
	BSTR productToCheck, 
	std::vector<SecurityProduct> &result, 
	wchar_t const* twoCode,
	wchar_t const* enabledPropertyName,
	wchar_t const* upToDatePropertyName = nullptr) 
{
    HRESULT hr;
	CComPtr<IEnumWbemClassObject> objEnumerator;
	hr = securityCenter->CreateInstanceEnum(
		productToCheck,
		WBEM_FLAG_FORWARD_ONLY,
		0,
		&objEnumerator
		);
    if (hr == WBEM_E_INVALID_CLASS)
    {
        return; //Expected error on XP x64 machines
    }
    ThrowIfFailed(hr);
	ULONG returnCount = 0;
    INSTALOG_TRACE(L"Enumerating...");
	for(;;)
	{
		CComPtr<IWbemClassObject> obj;
		hr = objEnumerator->Next(WBEM_INFINITE, 1, &obj, &returnCount);
        INSTALOG_TRACE(L"Enumerator says 0x" << std::hex << hr << std::dec);
		if (hr == WBEM_S_FALSE)
		{
			break;
		}
		else if (FAILED(hr))
		{
			throw _com_error(hr);
		}
		else if (returnCount == 0)
		{
			throw std::runtime_error("Unexpected number of returned classes.");
		}
        INSTALOG_TRACE(L"Getting instanceGuid");
		CComVariant variant;
		ThrowIfFailed(obj->Get(L"instanceGuid",0,&variant,0,0));
		ThrowIfFailed(variant.ChangeType(VT_BSTR));
		std::wstring guid(variant.bstrVal, SysStringLen(variant.bstrVal));
		variant.Clear();
        INSTALOG_TRACE(L"Getting displayName");
		ThrowIfFailed(obj->Get(L"displayName",0,&variant,0,0));
		ThrowIfFailed(variant.ChangeType(VT_BSTR));
        std::wstring name(variant.bstrVal, SysStringLen(variant.bstrVal));
        INSTALOG_TRACE(name);

        INSTALOG_TRACE(L"Getting " << enabledPropertyName);
		ThrowIfFailed(obj->Get(enabledPropertyName,0,&variant,0,0));
		ThrowIfFailed(variant.ChangeType(VT_BOOL));
		bool productEnabled = variant.boolVal != 0;
		SecurityProduct::UpdateStatusValues updateStatus = SecurityProduct::UpdateNotRequired;
		if (upToDatePropertyName != nullptr)
		{
			INSTALOG_TRACE(L"Getting " << upToDatePropertyName);
			ThrowIfFailed(obj->Get(upToDatePropertyName, 0, &variant, 0, 0));
			ThrowIfFailed(variant.ChangeType(VT_BOOL));
			if (variant.boolVal)
			{
				updateStatus = SecurityProduct::UpToDate;
			}
			else
			{
				updateStatus = SecurityProduct::OutOfDate;
			}
		}
		result.push_back(SecurityProduct(
			std::move(name),
			std::move(guid),
			productEnabled,
			updateStatus,
			twoCode));
	}
}

static void SecCenter2ProductCheck( 
	CComPtr<IWbemServices> securityCenter2, 
	BSTR productToCheck, 
	std::vector<SecurityProduct> &result, 
	const wchar_t * twoCode ) 
{
	CComPtr<IEnumWbemClassObject> objEnumerator;
	ThrowIfFailed(securityCenter2->CreateInstanceEnum(
		productToCheck,
		WBEM_FLAG_FORWARD_ONLY,
		0,
		&objEnumerator
		));
	ULONG returnCount = 0;
	for(;;)
	{
		HRESULT hr;
		CComPtr<IWbemClassObject> obj;
		hr = objEnumerator->Next(WBEM_INFINITE, 1, &obj, &returnCount);
        INSTALOG_TRACE(L"Enumerator says 0x" << std::hex << hr << std::dec);
		if (hr == WBEM_S_FALSE)
		{
			break;
		}
		else if (FAILED(hr))
		{
			ThrowFromHResult(hr);
		}
		else if (returnCount == 0)
		{
			throw std::runtime_error("Unexpected number of returned classes.");
		}
		CComVariant variant;
        INSTALOG_TRACE(L"Getting instanceGuid");
		ThrowIfFailed(obj->Get(L"instanceGuid",0,&variant,0,0));
		ThrowIfFailed(variant.ChangeType(VT_BSTR));
		std::wstring guid(variant.bstrVal, SysStringLen(variant.bstrVal));
		variant.Clear();
        INSTALOG_TRACE(L"Getting displayName");
		ThrowIfFailed(obj->Get(L"displayName",0,&variant,0,0));
		ThrowIfFailed(variant.ChangeType(VT_BSTR));
        std::wstring name(variant.bstrVal, SysStringLen(variant.bstrVal));
        INSTALOG_TRACE(name);
        INSTALOG_TRACE(L"Getting productState");
		ThrowIfFailed(obj->Get(L"productState",0,&variant,0,0));
		ThrowIfFailed(variant.ChangeType(VT_UINT));
		UINT32 productState = variant.uintVal;
        INSTALOG_TRACE(L"ProductState is 0x" << std::hex << productState << std::dec);
		char productType = static_cast<char>(
			(productState & 0x00FF0000ul) >> 16);
		char enabledBits = static_cast<char>(
			(productState & 0x0000FF00ul) >> 8);
		char updateBits = productState & 0x000000FFul;
		SecurityProduct::UpdateStatusValues updateStatus;
		if ((productType & 2ul) == 0)
		{
			updateStatus =  SecurityProduct::UpdateNotRequired;
		}
		else
		{
			if (updateBits == 0)
			{
				updateStatus = SecurityProduct::UpToDate;
			}
			else
			{
				updateStatus = SecurityProduct::OutOfDate;
			}
		}
		result.push_back(SecurityProduct(
			std::move(name),
			std::move(guid),
			enabledBits == 16,
			updateStatus,
			twoCode));
	}
}

static void CheckSecurityCenter( CComPtr<IWbemServices> wbemServices, std::vector<SecurityProduct>& result )
{
	CComPtr<IWbemServices> securityCenter;
	HRESULT errorCheck = wbemServices->OpenNamespace(
		BSTR(L"SecurityCenter"),0,0,&securityCenter,0);
	//On versions of Windows prior to XP SP2, there is no security center to query; so this would be
	//an expected failure.
	if (errorCheck == WBEM_E_INVALID_NAMESPACE)
	{
		return;
	}
	ThrowIfFailed(errorCheck);
    INSTALOG_TRACE(L"AntiVirusProduct");
	SecCenterProductCheck(securityCenter, BSTR(L"AntiVirusProduct"), result, 
		avCode, BSTR(L"onAccessScanningEnabled"), BSTR(L"productUpToDate"));
    INSTALOG_TRACE(L"FireWallProduct");
	SecCenterProductCheck(securityCenter, BSTR(L"FireWallProduct"), result,
		fwCode, BSTR(L"enabled"));
    INSTALOG_TRACE(L"AntiSpywareProduct");
	SecCenterProductCheck(securityCenter, BSTR(L"AntiSpywareProduct"), result, 
		asCode, BSTR(L"productEnabled"), BSTR(L"productUpToDate"));
}
static void CheckSecurityCenter2( CComPtr<IWbemServices> wbemServices,
	std::vector<SecurityProduct>& result )
{
	CComPtr<IWbemServices> securityCenter2;
	ThrowIfFailed(wbemServices->OpenNamespace(
		BSTR(L"SecurityCenter2"),0,0,&securityCenter2,0));
    INSTALOG_TRACE(L"AntiVirusProduct");
	SecCenter2ProductCheck(securityCenter2, BSTR(L"AntiVirusProduct"), result, 
		avCode);
    INSTALOG_TRACE(L"FireWallProduct");
	SecCenter2ProductCheck(securityCenter2, BSTR(L"FireWallProduct"), result,
		fwCode);
    INSTALOG_TRACE(L"AntiSpywareProduct");
	SecCenter2ProductCheck(securityCenter2, BSTR(L"AntiSpywareProduct"), result, 
		asCode);
}


std::vector<SecurityProduct> EnumerateSecurityProducts()
{
	OSVERSIONINFOW version;
	version.dwOSVersionInfoSize = sizeof(version);
	std::vector<SecurityProduct> result;
	GetVersionExW(&version);
    INSTALOG_TRACE(L"Making IWbemServices");
	CComPtr<IWbemServices> wbemServices(GetWbemServices());
	if (version.dwMajorVersion >= 6)
	{
        INSTALOG_TRACE(L"Enumerating SecurityCenter2");
		CheckSecurityCenter2(wbemServices, result);
	}
    INSTALOG_TRACE(L"Enumerating SecurityCenter");
	CheckSecurityCenter(wbemServices, result);
	return result;
}
std::wostream& operator<<( std::wostream& lhs, const SecurityProduct& rhs )
{
	lhs << rhs.GetTwoLetterPrefix() << L": " << rhs.GetName();
	if (rhs.IsEnabled())
	{
		lhs << L" (Enabled";
	}
	else
	{
		lhs << L" (Disabled";
	}
	switch (rhs.GetUpdateStatus())
	{
	case SecurityProduct::OutOfDate:
		lhs << L"/Out Of Date) ";
		break;
	case SecurityProduct::UpToDate:
		lhs << L"/Up To Date) ";
		break;
	case SecurityProduct::UpdateNotRequired:
		lhs << L") ";
		break;
	}
	lhs << rhs.GetInstanceGuid() << std::endl;
	return lhs;
}

void SecurityProduct::Delete()
{
	CComPtr<IWbemServices> wbemServices(GetWbemServices());
	CComPtr<IWbemServices> securityCenter2;
	ThrowIfFailed(wbemServices->OpenNamespace(
		BSTR(L"SecurityCenter2"),0,0,&securityCenter2,0));
	std::wstring path;
	if (wcscmp(GetTwoLetterPrefix(), avCode) == 0)
	{
		path = L"AntiVirusProduct";
	}
	else if (wcscmp(GetTwoLetterPrefix(), fwCode) == 0)
	{
		path = L"FirewallProduct";
	}
	else if (wcscmp(GetTwoLetterPrefix(), asCode) == 0)
	{
		path = L"AntiSpywareProduct";
	}
	else
	{
		throw std::exception("Invalid product type");
	}

	path.append(L".instanceGuid=\"");
	path.append(guid_);
	path.push_back(L'"');
	UniqueBstr guid(path);
    ThrowIfFailed(securityCenter2->DeleteInstance(guid.AsInput(), 0, nullptr, nullptr));
}

}}


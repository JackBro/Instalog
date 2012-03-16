#pragma once
#include <string>
#include <memory>
#include <windows.h>
#include <boost/noncopyable.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include "DdkStructures.h"

namespace Instalog { namespace SystemFacades {

	class RegistryValue
	{
		HANDLE hKey_;
		std::wstring name_;
	public:
		RegistryValue(HANDLE hKey, std::wstring && name);
		RegistryValue(RegistryValue && other);
	};

	class RegistrySubkeyNameIterator
		: public boost::iterator_facade<RegistrySubkeyNameIterator, std::wstring,  boost::bidirectional_traversal_tag, std::wstring const&>
	{
		friend class boost::iterator_core_access;
		HANDLE hKey_;
		DWORD currentIndex;
		std::wstring name;
		void Update();
	public:
		RegistrySubkeyNameIterator(HANDLE hKey)
			: hKey_(hKey)
		{ }
		std::wstring const& dereference() const;
		void increment();
		void decrement();
		bool equal(RegistrySubkeyNameIterator const& other) const;
	};

	class RegistryKey : boost::noncopyable
	{
		HANDLE hKey_;
	public:
		typedef std::unique_ptr<RegistryKey> Ptr;
		explicit RegistryKey(HANDLE hKey);
		RegistryKey(RegistryKey && other);
		~RegistryKey();
		HANDLE GetHkey() const;
		RegistryValue GetValue(std::wstring name);
		RegistryValue operator[](std::wstring name);
		void Delete();

		RegistrySubkeyNameIterator SubKeyNameBegin() const;
		RegistrySubkeyNameIterator SubKeyNameEnd() const;

		static Ptr Open(std::wstring const& key, REGSAM samDesired = KEY_ALL_ACCESS);
		static Ptr Open(Ptr const& parent, std::wstring const& key, REGSAM samDesired = KEY_ALL_ACCESS);
		static Ptr Open(RegistryKey const* parent, UNICODE_STRING& key, REGSAM samDesired);
		static Ptr Create(
			std::wstring const& key,
			REGSAM samDesired = KEY_ALL_ACCESS,
			DWORD options = REG_OPTION_NON_VOLATILE
		);
		static Ptr Create(
			Ptr const& parent,
			std::wstring const& key,
			REGSAM samDesired = KEY_ALL_ACCESS,
			DWORD options = REG_OPTION_NON_VOLATILE
		);
	};

}}

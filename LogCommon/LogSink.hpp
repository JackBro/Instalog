// Copyright � Billy O'Neal III
// This is under the 2 clause BSD license.
// See the included LICENSE.TXT file for more details.
#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <utility>
#include <limits>
#include <type_traits>
#include <memory>
#include <boost/config.hpp>
#include <boost/utility/string_ref.hpp>
#include <utf8/utf8.h>
#include "OptimisticBuffer.hpp"

namespace Instalog
{
    // Log sinks recieve the results of logging.
    struct log_sink
    {
        virtual void append(char const* data, std::size_t dataLength) = 0;
        virtual ~log_sink() BOOST_NOEXCEPT_OR_NOTHROW;
    };

    // Log sink which writes the results into a std::string.
    class string_sink final : public log_sink
    {
        std::string target;
    public:
        virtual void append(char const* data, std::size_t dataLength);
        std::string const& get() const BOOST_NOEXCEPT_OR_NOTHROW;
    };

    // Log sink which writes results into a file.
    class file_sink final : public log_sink
    {
        // Typically HANDLE on Windows, FILE* or file number on Unix.
        std::uintptr_t handleValue;
    public:
        file_sink(std::string const& filePath);
        file_sink(file_sink const&) = delete;
        file_sink(file_sink&&) BOOST_NOEXCEPT_OR_NOTHROW;
        ~file_sink() BOOST_NOEXCEPT_OR_NOTHROW;
        virtual void append(char const* data, std::size_t dataLength);
    };

    inline bool operator==(string_sink const& lhs, string_sink const& rhs)
    {
        return lhs.get() == rhs.get();
    }

    inline bool operator!=(string_sink const& lhs, string_sink const& rhs)
    {
        return lhs.get() != rhs.get();
    }

    inline bool operator<(string_sink const& lhs, string_sink const& rhs)
    {
        return lhs.get() < rhs.get();
    }

    inline bool operator>(string_sink const& lhs, string_sink const& rhs)
    {
        return lhs.get() > rhs.get();
    }

    inline bool operator<=(string_sink const& lhs, string_sink const& rhs)
    {
        return lhs.get() <= rhs.get();
    }

    inline bool operator>=(string_sink const& lhs, string_sink const& rhs)
    {
        return lhs.get() >= rhs.get();
    }

    // Format results types.
    // When formatting a value (e.g. a single number), these types are where given
    // value formatters store the result.
    // The code that calls the value formatters is designed to accept any of these types;
    // they are seperate this way so that each formatter can do the most performant thing
    // for their specific formatted type.
    // 
    // Format results types have a member function "data" which returns a pointer to the data
    // to write, and a member function "size" which returns the length of the valid region
    // pointed to by data().
    
    // Intrusive results: For objects which contain a string representation as part of
    // themselves, e.g. std::string. Contains a pointer to the string data to write, and
    // a length.
    class format_intrusive_result
    {
        char const* ptr;
        std::size_t length;
    public:
        format_intrusive_result() = default;
        format_intrusive_result(char const* data_, std::size_t length_) BOOST_NOEXCEPT_OR_NOTHROW
            : ptr(data_)
            , length(length_)
        {}
        char const* data() const BOOST_NOEXCEPT_OR_NOTHROW
        {
            return this->ptr;
        }
        std::size_t size() const BOOST_NOEXCEPT_OR_NOTHROW
        {
            return this->length;
        }
    };

    // Stack results. For things like integers and similar where the value is formatted into a
    // stack buffer.
    template <std::size_t allocLength>
    class format_stack_result
    {
        typedef std::uint16_t size_type; // assume that stack buffers won't be bigger than 64k
        static_assert(allocLength < UINT16_MAX, "Length limit exceeded in format_stack_result");
        size_type length;
        char array[allocLength];
    public:
        format_stack_result() = default;
        format_stack_result(std::uint16_t length_)
            : length(length_)
        { }

        static const std::size_t declared_size = allocLength;
        decltype(array)& data() BOOST_NOEXCEPT_OR_NOTHROW
        {
            return this->array;
        }
        char const* data() const BOOST_NOEXCEPT_OR_NOTHROW
        {
            return this->array;
        }
        void set_size(std::size_t size_) BOOST_NOEXCEPT_OR_NOTHROW
        {
            this->length = static_cast<size_type>(size_);
        }
        std::size_t size() const BOOST_NOEXCEPT_OR_NOTHROW
        {
            return length;
        }
    };

    // Template metafunction: stack_result_for_digits. Generates the correct
    // format_stack_result type with enough space to format a number written
    // of the supplied type.
    template <typename IntegralType, typename>
    struct stack_result_for_digits_impl
    {};

    template <typename IntegralType>
    struct stack_result_for_digits_impl<IntegralType, std::true_type>
    {
        typedef format_stack_result<std::numeric_limits<IntegralType>::digits10 + 2> type;
    };

    template<typename IntegralType>
    struct stack_result_for_digits : public stack_result_for_digits_impl<IntegralType, typename std::is_arithmetic<IntegralType>::type>
    {};

    // Character result. For cases where one character is required.
    struct format_character_result
    {
        char result;
    public:
        format_character_result() = default;
        format_character_result(char value) BOOST_NOEXCEPT_OR_NOTHROW
            : result(value)
        { }
        std::size_t size() const BOOST_NOEXCEPT_OR_NOTHROW
        {
            return 1;
        }
        char const* data() const BOOST_NOEXCEPT_OR_NOTHROW
        {
            return &this->result;
        }
    };

    // format_value shim functions.
    // The format_value shims accept a single value, and produce a format result type
    // containing that type formatted as a string.
    //
    // Users who wish to add their types to this formatting mechanism can do so
    // by defining a format_value that gets selected via argument dependant lookup.
    // 
    
    // Format std::string
    format_intrusive_result format_value(std::string const& value) BOOST_NOEXCEPT_OR_NOTHROW;
    
    // Format character pointers. (Assume null terminated)
    format_intrusive_result format_value(char const* ptr) BOOST_NOEXCEPT_OR_NOTHROW;

    // Format single characters
    format_character_result format_value(char value) BOOST_NOEXCEPT_OR_NOTHROW;

    // Format wide character versions of the above.
    std::string format_value(std::wstring const& value);
    std::string format_value(wchar_t const* value);
    format_stack_result<4> format_value(wchar_t value);
    format_intrusive_result format_value(boost::string_ref value);

#define DECLARE_KARMA_GENERATOR(t) \
    stack_result_for_digits<t>::type format_value(t value) BOOST_NOEXCEPT_OR_NOTHROW

    DECLARE_KARMA_GENERATOR(short);
    DECLARE_KARMA_GENERATOR(int);
    DECLARE_KARMA_GENERATOR(long);
    DECLARE_KARMA_GENERATOR(long long);
    DECLARE_KARMA_GENERATOR(unsigned short);
    DECLARE_KARMA_GENERATOR(unsigned long);
    DECLARE_KARMA_GENERATOR(unsigned int);
    DECLARE_KARMA_GENERATOR(unsigned long long);
    DECLARE_KARMA_GENERATOR(double);

#undef DECLARE_KARMA_GENERATOR

    // Platform newline selection.
#ifdef BOOST_WINDOWS
    format_intrusive_result inline get_newline() BOOST_NOEXCEPT_OR_NOTHROW
    {
        return format_intrusive_result("\r\n", 2);
    }
#else
    format_stack_result<1> inline get_newline() BOOST_NOEXCEPT_OR_NOTHROW
    {
        return format_intrusive_result("\n", 1);
    }
#endif

    // sum_sizes function basis case. Returns the sum of all std::size_t instances
    // passed in as arguments.
    std::size_t inline sum_sizes() BOOST_NOEXCEPT_OR_NOTHROW
    {
        return 0;
    }

    template <typename... Integral>
    std::size_t inline sum_sizes(std::size_t size, Integral ...sizes) BOOST_NOEXCEPT_OR_NOTHROW
    {
        return size + sum_sizes(sizes...);
    }

    // Helper function implementing the write formatting API.
    template <typename Sink, typename... Slices>
    Sink& write_impl_n(Sink& target, Slices const&... slices)
    {
        // Special thanks to Nawaz for his help here.
        // See: http://stackoverflow.com/a/20440197/82320
        std::size_t const length = sum_sizes(slices.size()...);
        OptimisticBuffer<256> buff(length);
        char* ptr = buff.GetAs<char>();
        char* endPtr = ptr;
        char* expand[] = {(endPtr = std::copy_n(slices.data(), slices.size(), endPtr))...};
        (void)expand; // silence unreferenced parameter warning.
        target.append(ptr, endPtr - ptr);
        return target;
    }

    template <typename Sink, typename FirstSlice, typename... Slices>
    Sink& write_impl(Sink& target, FirstSlice firstSlice, Slices const&...slices)
    {
        write_impl_n(target, firstSlice, slices...);
        return target;
    }

    // This overload does an optimization such that when we are formatting a single
    // result, we can write directly from that result's buffer rather than
    template <typename Sink, typename Slice>
    Sink& write_impl(Sink& target, Slice const& slice)
    {
        target.append(slice.data(), slice.size());
        return target;
    }

    // TODO: Add specialization of write_impl for a single write of an intrusive
    // formatted result.

    // Writes a set of values to the given sink.
    template <typename Sink, typename... Values>
    Sink& write(Sink& target, Values &&...values)
    {
        return write_impl(target, format_value(std::forward<Values>(values))...);
    }

    // Writes a set of values to the given sink, followed by a newline.
    template <typename Sink, typename... Values>
    Sink& writeln(Sink& target, Values &&...values)
    {
        return write_impl(target, format_value(std::forward<Values>(values))..., get_newline());
    }

    template <typename NumberType>
    class padded_number
    {
        std::size_t size_impl;
        NumberType value;
        char fill_impl;
    public:
        padded_number(std::size_t size_, char fill_, NumberType value_) BOOST_NOEXCEPT_OR_NOTHROW
            : value(value_)
            , fill_impl(fill_)
            , size_impl(size_)
        {}
        
        std::size_t size() const BOOST_NOEXCEPT_OR_NOTHROW
        {
            return this->size_impl;
        }

        NumberType get() const BOOST_NOEXCEPT_OR_NOTHROW
        {
            return this->value;
        }

        char fill() const BOOST_NOEXCEPT_OR_NOTHROW
        {
            return this->fill_impl;
        }
    };

    template <typename NumberType>
    padded_number<NumberType> inline pad(std::size_t size, char fill, NumberType value) BOOST_NOEXCEPT_OR_NOTHROW
    {
        return padded_number<NumberType>(size, fill, value);
    }

    template <typename NumberType>
    std::string format_value(padded_number<NumberType> const& val)
    {
        auto const& basic = format_value(val.get());
        auto const unpaddedSize = basic.size();
        auto const desiredSize = val.size();
        std::string result;
        if (unpaddedSize < desiredSize)
        {
            result.reserve(desiredSize);
            auto const paddingCharacters = desiredSize - unpaddedSize;
            result.append(paddingCharacters, val.fill());
        }
        
        result.append(basic.data(), unpaddedSize);
        return result;
    }

    template <typename NumberType>
    struct hex_formatted_value
    {
        NumberType value;
    public:
        hex_formatted_value(NumberType value_) BOOST_NOEXCEPT_OR_NOTHROW
            : value(value_)
        {}
        NumberType get() const BOOST_NOEXCEPT_OR_NOTHROW
        {
            return this->value;
        }
    };

    template <typename NumberType>
    hex_formatted_value<typename std::make_unsigned<NumberType>::type> hex(NumberType value)
    {
        static_assert(std::is_integral<NumberType>::value, "Hex accepts only integral values.");
        typedef typename std::make_unsigned<NumberType>::type unsignedType;
        unsignedType formattedValue = static_cast<unsignedType>(value);
        return hex_formatted_value<unsignedType>(formattedValue);
    }

    template <typename NumberType>
    format_stack_result<sizeof(NumberType)* 2> format_value(hex_formatted_value<NumberType> const& val)
    {
        char const hexChars[] = "0123456789ABCDEF";
        auto const value = val.get();
        char const* asChar = reinterpret_cast<char const*>(&value);
        format_stack_result<sizeof(NumberType)* 2> result(sizeof(NumberType)* 2);
        for (std::size_t idx = 0; idx < sizeof(NumberType); ++idx)
        {
            char const currentChar = asChar[sizeof(NumberType) - idx - 1];
            result.data()[idx * 2    ] = hexChars[(currentChar & 0xF0) >> 4];
            result.data()[idx * 2 + 1] = hexChars[ currentChar & 0x0F];
        }

        return result;
    }
}

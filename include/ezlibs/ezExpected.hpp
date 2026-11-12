#pragma once

/*
MIT License

Copyright (c) 2014-2024 Stephane Cuillerdier (aka aiekick)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

// ezExpected is part of the ezLibs project : https://github.com/aiekick/ezLibs.git

/* a value or the error that prevented it, with the names of std::expected

ez::expected<std::string, LoadError> loadShader(const std::string& aPath) {
    if (!exists(aPath)) {
        return ez::unexpected<LoadError>(LoadError::NotFound);
    }
    return readAll(aPath);
}

auto result = loadShader(path);
if (result) {
    use(*result);
} else {
    report(result.error());
}

 - only this subset exists : has_value, operator bool, value, operator*,
   operator->, error, value_or
 - value() on an error and error() on a value assert
 - when the standard library has std::expected, the two names are its aliases
*/

#ifdef __has_include
#if __has_include(<version>)
#include <version>
#endif
#endif

#ifdef __cpp_lib_expected

#include <expected>

namespace ez {

template <typename TValue, typename TError>
using expected = std::expected<TValue, TError>;

template <typename TError>
using unexpected = std::unexpected<TError>;

}  // namespace ez

#else  // __cpp_lib_expected

#include <new>
#include <memory>
#include <cassert>
#include <utility>
#include <type_traits>

namespace ez {

template <typename TError>
class unexpected {
private:
    TError m_error;

public:
    explicit unexpected(const TError& aError) : m_error(aError) {
    }
    explicit unexpected(TError&& aError) : m_error(std::move(aError)) {
    }
    const TError& error() const {
        return m_error;
    }
    TError& error() {
        return m_error;
    }
};

namespace detail {

template <typename TType>
struct expectedIsUnexpected : std::false_type {};

template <typename TError>
struct expectedIsUnexpected<unexpected<TError>> : std::true_type {};

}  // namespace detail

template <typename TValue, typename TError>
class expected {
private:
    union {
        TValue m_value;
        TError m_error;
    };
    bool m_hasValue;

public:
    expected() : m_hasValue(true) {
        ::new (static_cast<void*>(std::addressof(m_value))) TValue();
    }
    template <typename TOther,
        typename = typename std::enable_if<std::is_constructible<TValue, TOther&&>::value &&                    //
                                           !std::is_same<typename std::decay<TOther>::type, expected>::value &&  //
                                           !detail::expectedIsUnexpected<typename std::decay<TOther>::type>::value>::type>
    expected(TOther&& aValue) : m_hasValue(true) {
        ::new (static_cast<void*>(std::addressof(m_value))) TValue(std::forward<TOther>(aValue));
    }
    template <typename TOtherError, typename = typename std::enable_if<std::is_constructible<TError, const TOtherError&>::value>::type>
    expected(const unexpected<TOtherError>& aUnexpected) : m_hasValue(false) {
        ::new (static_cast<void*>(std::addressof(m_error))) TError(aUnexpected.error());
    }
    template <typename TOtherError, typename = typename std::enable_if<std::is_constructible<TError, TOtherError&&>::value>::type>
    expected(unexpected<TOtherError>&& aUnexpected) : m_hasValue(false) {
        ::new (static_cast<void*>(std::addressof(m_error))) TError(std::move(aUnexpected.error()));
    }
    expected(const expected& aOther) : m_hasValue(aOther.m_hasValue) {
        m_copyFrom(aOther);
    }
    expected(expected&& aOther) : m_hasValue(aOther.m_hasValue) {
        m_moveFrom(std::move(aOther));
    }
    ~expected() {
        m_destroy();
    }
    expected& operator=(const expected& aOther) {
        if (this != &aOther) {
            // the copy is made first : a failing copy leaves this one untouched
            expected copy(aOther);
            m_destroy();
            m_hasValue = copy.m_hasValue;
            m_moveFrom(std::move(copy));
        }
        return *this;
    }
    expected& operator=(expected&& aOther) {
        if (this != &aOther) {
            m_destroy();
            m_hasValue = aOther.m_hasValue;
            m_moveFrom(std::move(aOther));
        }
        return *this;
    }
    bool has_value() const noexcept {
        return m_hasValue;
    }
    explicit operator bool() const noexcept {
        return m_hasValue;
    }
    TValue& value() {
        assert(m_hasValue && "value() called on an expected holding an error");
        return m_value;
    }
    const TValue& value() const {
        assert(m_hasValue && "value() called on an expected holding an error");
        return m_value;
    }
    TValue& operator*() {
        return value();
    }
    const TValue& operator*() const {
        return value();
    }
    TValue* operator->() {
        return std::addressof(value());
    }
    const TValue* operator->() const {
        return std::addressof(value());
    }
    TError& error() {
        assert(!m_hasValue && "error() called on an expected holding a value");
        return m_error;
    }
    const TError& error() const {
        assert(!m_hasValue && "error() called on an expected holding a value");
        return m_error;
    }
    template <typename TOther>
    TValue value_or(TOther&& aDefault) const {
        return m_hasValue ? m_value : static_cast<TValue>(std::forward<TOther>(aDefault));
    }

private:
    // m_hasValue already says which member to build
    void m_copyFrom(const expected& aOther) {
        if (m_hasValue) {
            ::new (static_cast<void*>(std::addressof(m_value))) TValue(aOther.m_value);
        } else {
            ::new (static_cast<void*>(std::addressof(m_error))) TError(aOther.m_error);
        }
    }
    void m_moveFrom(expected&& aOther) {
        if (m_hasValue) {
            ::new (static_cast<void*>(std::addressof(m_value))) TValue(std::move(aOther.m_value));
        } else {
            ::new (static_cast<void*>(std::addressof(m_error))) TError(std::move(aOther.m_error));
        }
    }
    void m_destroy() {
        if (m_hasValue) {
            m_value.~TValue();
        } else {
            m_error.~TError();
        }
    }
};

}  // namespace ez

#endif  // __cpp_lib_expected

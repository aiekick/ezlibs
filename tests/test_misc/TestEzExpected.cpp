#include <ezlibs/ezExpected.hpp>
#include <ezlibs/ezCTest.hpp>
#include <cstdint>
#include <string>
#include <utility>

////////////////////////////////////////////////////////////////////////////
//// Test fixtures /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

namespace {

enum class FixtureError { None = 0, NotFound, Unreadable };

// counts its living instances
class FixtureCounted {
public:
    static int32_t& living() {
        static int32_t s_living = 0;
        return s_living;
    }
    int32_t m_value{0};
    explicit FixtureCounted(int32_t aValue) : m_value(aValue) {
        ++living();
    }
    FixtureCounted(const FixtureCounted& aOther) : m_value(aOther.m_value) {
        ++living();
    }
    FixtureCounted(FixtureCounted&& aOther) noexcept : m_value(aOther.m_value) {
        ++living();
    }
    FixtureCounted& operator=(const FixtureCounted&) = default;
    FixtureCounted& operator=(FixtureCounted&&) = default;
    ~FixtureCounted() {
        --living();
    }
};

ez::unexpected<FixtureError> fixtureFail(FixtureError aError) {
    return ez::unexpected<FixtureError>(aError);
}

ez::expected<std::string, FixtureError> fixtureLoad(bool aSucceed) {
    if (!aSucceed) {
        return fixtureFail(FixtureError::NotFound);
    }
    return "loaded";
}

}  // namespace

////////////////////////////////////////////////////////////////////////////
//// Tests /////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzExpected_HoldsValue() {
    ez::expected<int32_t, FixtureError> result(5);
    CTEST_ASSERT(result.has_value());
    CTEST_ASSERT(static_cast<bool>(result));
    CTEST_ASSERT(result.value() == 5);
    CTEST_ASSERT(*result == 5);
    return true;
}

bool TestEzExpected_HoldsError() {
    ez::expected<int32_t, FixtureError> result(fixtureFail(FixtureError::Unreadable));
    CTEST_ASSERT(!result.has_value());
    CTEST_ASSERT(!static_cast<bool>(result));
    CTEST_ASSERT(result.error() == FixtureError::Unreadable);
    return true;
}

bool TestEzExpected_ReturnedFromFunction() {
    const auto loaded = fixtureLoad(true);
    CTEST_ASSERT(loaded.has_value());
    CTEST_ASSERT(*loaded == "loaded");
    const auto refused = fixtureLoad(false);
    CTEST_ASSERT(!refused.has_value());
    CTEST_ASSERT(refused.error() == FixtureError::NotFound);
    return true;
}

bool TestEzExpected_ValueOr() {
    const ez::expected<int32_t, FixtureError> withValue(5);
    CTEST_ASSERT(withValue.value_or(9) == 5);
    const ez::expected<int32_t, FixtureError> withError(fixtureFail(FixtureError::NotFound));
    CTEST_ASSERT(withError.value_or(9) == 9);
    return true;
}

bool TestEzExpected_ArrowReachesValue() {
    ez::expected<std::string, FixtureError> result(std::string("abc"));
    CTEST_ASSERT(result->size() == 3u);
    result->push_back('d');
    CTEST_ASSERT(*result == "abcd");
    return true;
}

bool TestEzExpected_ValueIsMutable() {
    ez::expected<int32_t, FixtureError> result(5);
    result.value() = 7;
    CTEST_ASSERT(*result == 7);
    return true;
}

bool TestEzExpected_CopyIsIndependent() {
    ez::expected<std::string, FixtureError> source(std::string("abc"));
    ez::expected<std::string, FixtureError> copy(source);
    copy.value() = "xyz";
    CTEST_ASSERT(*source == "abc");
    CTEST_ASSERT(*copy == "xyz");
    const ez::expected<std::string, FixtureError> sourceError(fixtureFail(FixtureError::NotFound));
    const ez::expected<std::string, FixtureError> copyError(sourceError);
    CTEST_ASSERT(!copyError.has_value());
    CTEST_ASSERT(copyError.error() == FixtureError::NotFound);
    return true;
}

bool TestEzExpected_MoveCarriesValue() {
    ez::expected<std::string, FixtureError> source(std::string("abc"));
    ez::expected<std::string, FixtureError> target(std::move(source));
    CTEST_ASSERT(target.has_value());
    CTEST_ASSERT(*target == "abc");
    return true;
}

bool TestEzExpected_AssignSwitchesState() {
    ez::expected<std::string, FixtureError> result(std::string("abc"));
    result = fixtureFail(FixtureError::Unreadable);
    CTEST_ASSERT(!result.has_value());
    CTEST_ASSERT(result.error() == FixtureError::Unreadable);
    const ez::expected<std::string, FixtureError> other(std::string("xyz"));
    result = other;
    CTEST_ASSERT(result.has_value());
    CTEST_ASSERT(*result == "xyz");
    return true;
}

bool TestEzExpected_NoDefaultCtorRequired() {
    const ez::expected<FixtureCounted, FixtureError> result(FixtureCounted(3));
    CTEST_ASSERT(result.has_value());
    CTEST_ASSERT(result->m_value == 3);
    return true;
}

bool TestEzExpected_DestroysWhatItHolds() {
    const auto livingBefore = FixtureCounted::living();
    {
        ez::expected<FixtureCounted, FixtureError> result(FixtureCounted(1));
        CTEST_ASSERT(FixtureCounted::living() == (livingBefore + 1));
        ez::expected<FixtureCounted, FixtureError> copy(result);
        CTEST_ASSERT(FixtureCounted::living() == (livingBefore + 2));
        // the value leaves when an error takes its place
        copy = fixtureFail(FixtureError::NotFound);
        CTEST_ASSERT(FixtureCounted::living() == (livingBefore + 1));
        // and comes back when a value takes the place of the error
        copy = result;
        CTEST_ASSERT(FixtureCounted::living() == (livingBefore + 2));
    }
    CTEST_ASSERT(FixtureCounted::living() == livingBefore);
    return true;
}

////////////////////////////////////////////////////////////////////////////
//// Dispatch //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzExpected(const std::string& vTest) {
    IfTestExist(TestEzExpected_HoldsValue);
    else IfTestExist(TestEzExpected_HoldsError);
    else IfTestExist(TestEzExpected_ReturnedFromFunction);
    else IfTestExist(TestEzExpected_ValueOr);
    else IfTestExist(TestEzExpected_ArrowReachesValue);
    else IfTestExist(TestEzExpected_ValueIsMutable);
    else IfTestExist(TestEzExpected_CopyIsIndependent);
    else IfTestExist(TestEzExpected_MoveCarriesValue);
    else IfTestExist(TestEzExpected_AssignSwitchesState);
    else IfTestExist(TestEzExpected_NoDefaultCtorRequired);
    else IfTestExist(TestEzExpected_DestroysWhatItHolds);
    return false;
}

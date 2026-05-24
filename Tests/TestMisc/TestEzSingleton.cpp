#include <ezlibs/ezSingleton.hpp>
#include <ezlibs/ezCTest.hpp>
#include <cstdint>
#include <string>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)  // Conversion from 'double' to 'float', possible loss of data
#pragma warning(disable : 4305)  // Truncation from 'double' to 'float'
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif

////////////////////////////////////////////////////////////////////////////
//// Test fixtures /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

namespace {

// default-constructible type
class FixtureNoArg {
    IMPLEMENT_SINGLETON(FixtureNoArg)
public:
    FixtureNoArg() = default;
    int32_t m_value{42};
};

// type whose constructor needs a value argument
class FixtureWithArg {
    IMPLEMENT_SINGLETON(FixtureWithArg)
public:
    explicit FixtureWithArg(int32_t aValue) : m_value(aValue) {}
    int32_t m_value{0};
};

// type with a reference member (no implicit default ctor)
class FixtureRefMember {
    IMPLEMENT_SINGLETON(FixtureRefMember)
public:
    explicit FixtureRefMember(int32_t& aRef) : mr_ref(aRef) {}
    int32_t& mr_ref;
};

// shared singleton with argument
class FixtureSharedWithArg {
    IMPLEMENT_SHARED_SINGLETON(FixtureSharedWithArg)
public:
    explicit FixtureSharedWithArg(int32_t aValue) : m_value(aValue) {}
    int32_t m_value{0};
};

}  // namespace

////////////////////////////////////////////////////////////////////////////
//// Tests /////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzSingleton_InitDefault() {
    auto& sp = FixtureNoArg::initSingleton();
    CTEST_ASSERT(sp != nullptr);
    CTEST_ASSERT(sp->m_value == 42);
    CTEST_ASSERT(&FixtureNoArg::ref() == sp.get());
    FixtureNoArg::unitSingleton();
    return true;
}

bool TestEzSingleton_InitWithArg() {
    auto& sp = FixtureWithArg::initSingleton(123);
    CTEST_ASSERT(sp != nullptr);
    CTEST_ASSERT(sp->m_value == 123);
    FixtureWithArg::unitSingleton();
    return true;
}

bool TestEzSingleton_NoDefaultCtorRequired() {
    int32_t target = 7;
    auto& sp = FixtureRefMember::initSingleton(target);
    CTEST_ASSERT(sp != nullptr);
    CTEST_ASSERT(&sp->mr_ref == &target);
    // ref() must NOT instantiate a default constructor of FixtureRefMember
    auto& sameRef = FixtureRefMember::ref();
    CTEST_ASSERT(&sameRef.mr_ref == &target);
    FixtureRefMember::unitSingleton();
    return true;
}

bool TestEzSingleton_SecondInitKeepsFirstInstance() {
    auto& sp1 = FixtureWithArg::initSingleton(111);
    auto* firstPtr = sp1.get();
    auto& sp2 = FixtureWithArg::initSingleton(222);
    CTEST_ASSERT(sp2.get() == firstPtr);
    CTEST_ASSERT(sp2->m_value == 111);
    FixtureWithArg::unitSingleton();
    return true;
}

bool TestEzSingleton_UnitThenReInit() {
    auto& sp1 = FixtureWithArg::initSingleton(111);
    CTEST_ASSERT(sp1->m_value == 111);
    FixtureWithArg::unitSingleton();
    auto& sp2 = FixtureWithArg::initSingleton(222);
    CTEST_ASSERT(sp2 != nullptr);
    CTEST_ASSERT(sp2->m_value == 222);
    FixtureWithArg::unitSingleton();
    return true;
}

bool TestEzSingleton_RefIsSameInstanceAsInit() {
    auto& sp = FixtureWithArg::initSingleton(99);
    CTEST_ASSERT(&FixtureWithArg::ref() == sp.get());
    FixtureWithArg::unitSingleton();
    return true;
}

bool TestEzSingleton_SharedInitAndRef() {
    auto sp1 = FixtureSharedWithArg::initSingleton(55);
    CTEST_ASSERT(sp1 != nullptr);
    CTEST_ASSERT(sp1->m_value == 55);
    auto sp2 = FixtureSharedWithArg::ref();
    CTEST_ASSERT(sp2.get() == sp1.get());
    FixtureSharedWithArg::unitSingleton();
    return true;
}

bool TestEzSingleton_SharedRefSurvivesUnit() {
    auto refCopy = FixtureSharedWithArg::initSingleton(77);
    CTEST_ASSERT(refCopy != nullptr);
    auto* rawPtr = refCopy.get();
    FixtureSharedWithArg::unitSingleton();
    // shared_ptr returned by value -> caller still owns the instance
    CTEST_ASSERT(refCopy != nullptr);
    CTEST_ASSERT(refCopy.get() == rawPtr);
    CTEST_ASSERT(refCopy->m_value == 77);
    return true;
}

////////////////////////////////////////////////////////////////////////////
//// Dispatch //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzSingleton(const std::string& vTest) {
    IfTestExist(TestEzSingleton_InitDefault);
    else IfTestExist(TestEzSingleton_InitWithArg);
    else IfTestExist(TestEzSingleton_NoDefaultCtorRequired);
    else IfTestExist(TestEzSingleton_SecondInitKeepsFirstInstance);
    else IfTestExist(TestEzSingleton_UnitThenReInit);
    else IfTestExist(TestEzSingleton_RefIsSameInstanceAsInit);
    else IfTestExist(TestEzSingleton_SharedInitAndRef);
    else IfTestExist(TestEzSingleton_SharedRefSurvivesUnit);
    return false;
}

#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

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
    FixtureNoArg::initSingleton();
    CTEST_ASSERT(FixtureNoArg::ref().m_value == 42);
    // ref() returns a stable reference to the unique instance
    CTEST_ASSERT(&FixtureNoArg::ref() == &FixtureNoArg::ref());
    FixtureNoArg::unitSingleton();
    return true;
}

bool TestEzSingleton_InitWithArg() {
    FixtureWithArg::initSingleton(123);
    CTEST_ASSERT(FixtureWithArg::ref().m_value == 123);
    FixtureWithArg::unitSingleton();
    return true;
}

bool TestEzSingleton_NoDefaultCtorRequired() {
    int32_t target = 7;
    FixtureRefMember::initSingleton(target);
    CTEST_ASSERT(&FixtureRefMember::ref().mr_ref == &target);
    // ref() must NOT instantiate a default constructor of FixtureRefMember
    auto& sameRef = FixtureRefMember::ref();
    CTEST_ASSERT(&sameRef.mr_ref == &target);
    FixtureRefMember::unitSingleton();
    return true;
}

bool TestEzSingleton_SecondInitRebuildsInstance() {
    FixtureWithArg::initSingleton(111);
    CTEST_ASSERT(FixtureWithArg::ref().m_value == 111);
    // initSingleton performs an unconditional reset(new T(...)), so a second
    // init rebuilds the instance and replaces it with a fresh value
    FixtureWithArg::initSingleton(222);
    CTEST_ASSERT(FixtureWithArg::ref().m_value == 222);
    FixtureWithArg::unitSingleton();
    return true;
}

bool TestEzSingleton_UnitThenReInit() {
    FixtureWithArg::initSingleton(111);
    CTEST_ASSERT(FixtureWithArg::ref().m_value == 111);
    FixtureWithArg::unitSingleton();
    FixtureWithArg::initSingleton(222);
    CTEST_ASSERT(FixtureWithArg::ref().m_value == 222);
    FixtureWithArg::unitSingleton();
    return true;
}

bool TestEzSingleton_RefIsSameInstanceAsInit() {
    FixtureWithArg::initSingleton(99);
    auto& firstRef = FixtureWithArg::ref();
    auto& secondRef = FixtureWithArg::ref();
    CTEST_ASSERT(&firstRef == &secondRef);
    CTEST_ASSERT(firstRef.m_value == 99);
    FixtureWithArg::unitSingleton();
    return true;
}

bool TestEzSingleton_SharedInitAndRef() {
    FixtureSharedWithArg::initSingleton(55);
    auto sp1 = FixtureSharedWithArg::ref();
    CTEST_ASSERT(sp1 != nullptr);
    CTEST_ASSERT(sp1->m_value == 55);
    auto sp2 = FixtureSharedWithArg::ref();
    CTEST_ASSERT(sp2.get() == sp1.get());
    FixtureSharedWithArg::unitSingleton();
    return true;
}

bool TestEzSingleton_SharedRefSurvivesUnit() {
    FixtureSharedWithArg::initSingleton(77);
    // ref() on a shared singleton returns the shared_ptr by value
    auto refCopy = FixtureSharedWithArg::ref();
    CTEST_ASSERT(refCopy != nullptr);
    auto* rawPtr = refCopy.get();
    FixtureSharedWithArg::unitSingleton();
    // the caller-held shared_ptr still owns the instance
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
    else IfTestExist(TestEzSingleton_SecondInitRebuildsInstance);
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

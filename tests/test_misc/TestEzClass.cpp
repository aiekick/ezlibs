#include <ezlibs/ezClass.hpp>
#include <ezlibs/ezCTest.hpp>
#include <cstdint>
#include <string>
#include <type_traits>

////////////////////////////////////////////////////////////////////////////
//// Test fixtures /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

namespace {

// a base that can only be built with a value : the very case where a
// defaulted constructor would be deleted, and DISABLE_CONSTRUCTORS unusable
class BaseWithArgOnly {
public:
    explicit BaseWithArgOnly(int32_t aValue) : m_value(aValue) {}
    int32_t m_value{0};
};

// the copy and the move are gone, the constructor of the type is KEPT
class FixtureCopyAndMoveDisabled : public BaseWithArgOnly {
    DISABLE_COPY_AND_MOVE(FixtureCopyAndMoveDisabled)
public:
    FixtureCopyAndMoveDisabled() : BaseWithArgOnly(42) {}
};

// the whole family is gone but the default one
class FixtureConstructorsDisabled {
    DISABLE_CONSTRUCTORS(FixtureConstructorsDisabled)
public:
    int32_t m_value{7};
};

class FixtureDestructorsDisabled {
    DISABLE_DESTRUCTORS(FixtureDestructorsDisabled)
};

}  // namespace

////////////////////////////////////////////////////////////////////////////
//// Tests /////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

// the law of DISABLE_COPY_AND_MOVE : the type keeps the constructor it
// wrote itself — the point of the macro — and loses only the copy and the move
bool TestEzClass_DisableCopyAndMoveKeepsTheOwnConstructor() {
    CTEST_ASSERT(std::is_default_constructible<FixtureCopyAndMoveDisabled>::value);
    CTEST_ASSERT(!std::is_copy_constructible<FixtureCopyAndMoveDisabled>::value);
    CTEST_ASSERT(!std::is_move_constructible<FixtureCopyAndMoveDisabled>::value);
    CTEST_ASSERT(!std::is_copy_assignable<FixtureCopyAndMoveDisabled>::value);
    CTEST_ASSERT(!std::is_move_assignable<FixtureCopyAndMoveDisabled>::value);
    FixtureCopyAndMoveDisabled fixture;  // the kept constructor really runs
    CTEST_ASSERT(fixture.m_value == 42);
    return true;
}

// the law of DISABLE_CONSTRUCTORS : only the default one survives
bool TestEzClass_DisableConstructorsKeepsOnlyTheDefaultOne() {
    CTEST_ASSERT(std::is_default_constructible<FixtureConstructorsDisabled>::value);
    CTEST_ASSERT(!std::is_copy_constructible<FixtureConstructorsDisabled>::value);
    CTEST_ASSERT(!std::is_move_constructible<FixtureConstructorsDisabled>::value);
    CTEST_ASSERT(!std::is_copy_assignable<FixtureConstructorsDisabled>::value);
    CTEST_ASSERT(!std::is_move_assignable<FixtureConstructorsDisabled>::value);
    FixtureConstructorsDisabled fixture;
    CTEST_ASSERT(fixture.m_value == 7);
    return true;
}

// the law of DISABLE_DESTRUCTORS : the type is destroyed through its base
bool TestEzClass_DisableDestructorsGivesAVirtualDestructor() {
    CTEST_ASSERT(std::has_virtual_destructor<FixtureDestructorsDisabled>::value);
    return true;
}

////////////////////////////////////////////////////////////////////////////
//// Dispatch //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzClass(const std::string& vTest) {
    IfTestExist(TestEzClass_DisableCopyAndMoveKeepsTheOwnConstructor);
    else IfTestExist(TestEzClass_DisableConstructorsKeepsOnlyTheDefaultOne);
    else IfTestExist(TestEzClass_DisableDestructorsGivesAVirtualDestructor);
    return false;
}

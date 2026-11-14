#include <ezlibs/ezCnt.hpp>
#include <ezlibs/ezCTest.hpp>
#include <string>

// Desactivation des warnings de conversion
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
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzCnt_Generic() {
    return true;
}

bool TestEzCnt_OneTypename() {
    ez::cnt::DicoVector<std::string> dico;
    CTEST_ASSERT(dico.tryAdd("toto") == true);
    CTEST_ASSERT(dico.tryAdd("toto", "tata") == false);
    CTEST_ASSERT(dico.trySetExisting("toto", "tata") == true);
    return true;
}

bool TestEzCnt_TwoTypename() {
    ez::cnt::DicoVector<std::string, int32_t> dico;
    CTEST_ASSERT(dico.tryAdd("toto", 2));
    return true;
}

// the values come out in the ascending order given with each one, whatever the order they were added in
bool TestEzCnt_OrderableDico_IteratesByAscendingOrder() {
    ez::cnt::OrderableDico<std::string, int32_t, std::string> dico;
    CTEST_ASSERT(dico.tryAdd("keyC", 30, "third"));
    CTEST_ASSERT(dico.tryAdd("keyA", 10, "first"));
    CTEST_ASSERT(dico.tryAdd("keyB", 20, "second"));
    std::string joined;
    for (const auto& value : dico) {
        joined += value + ";";
    }
    CTEST_ASSERT(joined == "first;second;third;");
    return true;
}

// the values of a same order keep the order they were added in
bool TestEzCnt_OrderableDico_SameOrderKeepsInsertion() {
    ez::cnt::OrderableDico<std::string, int32_t, std::string> dico;
    const char* const names[] = {"zulu", "alpha", "mike", "bravo", "yankee", "charlie", "kilo", "delta", "xray", "echo"};
    std::string expected;
    for (const auto* name : names) {
        CTEST_ASSERT(dico.tryAdd(name, 5, name));
        expected += std::string(name) + ";";
    }
    CTEST_ASSERT(dico.tryAdd("last", 9, "last"));
    CTEST_ASSERT(dico.tryAdd("first", 1, "first"));
    std::string joined;
    for (const auto& value : dico) {
        joined += value + ";";
    }
    CTEST_ASSERT(joined == ("first;" + expected + "last;"));
    return true;
}

// a key already there is refused, and what it holds stays
bool TestEzCnt_OrderableDico_RefusesAKeyAlreadyThere() {
    ez::cnt::OrderableDico<std::string, int32_t, std::string> dico;
    CTEST_ASSERT(dico.tryAdd("key", 1, "kept"));
    CTEST_ASSERT(!dico.tryAdd("key", 2, "refused"));
    CTEST_ASSERT(dico.size() == 1U);
    CTEST_ASSERT(dico.at("key") == "kept");
    CTEST_ASSERT(*dico.begin() == "kept");
    return true;
}

// a value is found by its key
bool TestEzCnt_OrderableDico_FindsByKey() {
    ez::cnt::OrderableDico<std::string, int32_t, std::string> dico;
    CTEST_ASSERT(!dico.exist("keyA"));
    CTEST_ASSERT(dico.tryAdd("keyB", 2, "valueB"));
    CTEST_ASSERT(dico.tryAdd("keyA", 1, "valueA"));
    CTEST_ASSERT(dico.exist("keyA"));
    CTEST_ASSERT(dico.exist("keyB"));
    CTEST_ASSERT(!dico.exist("keyC"));
    CTEST_ASSERT(dico.at("keyA") == "valueA");
    CTEST_ASSERT(dico.at("keyB") == "valueB");
    return true;
}

// clearing empties it, and a cleared key can be added again
bool TestEzCnt_OrderableDico_ClearEmptiesIt() {
    ez::cnt::OrderableDico<std::string, int32_t, std::string> dico;
    CTEST_ASSERT(dico.empty());
    CTEST_ASSERT(dico.tryAdd("key", 1, "value"));
    CTEST_ASSERT(!dico.empty());
    dico.clear();
    CTEST_ASSERT(dico.empty());
    CTEST_ASSERT(dico.size() == 0U);
    CTEST_ASSERT(!dico.exist("key"));
    CTEST_ASSERT(dico.begin() == dico.end());
    CTEST_ASSERT(dico.tryAdd("key", 1, "again"));
    CTEST_ASSERT(dico.at("key") == "again");
    return true;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

#define IfTestExist(v)            \
    if (vTest == std::string(#v)) \
    return v()

bool TestEzCnt(const std::string& vTest) {
    IfTestExist(TestEzCnt_Generic);
    else IfTestExist(TestEzCnt_OneTypename);
    else IfTestExist(TestEzCnt_TwoTypename);
    else IfTestExist(TestEzCnt_OrderableDico_IteratesByAscendingOrder);
    else IfTestExist(TestEzCnt_OrderableDico_SameOrderKeepsInsertion);
    else IfTestExist(TestEzCnt_OrderableDico_RefusesAKeyAlreadyThere);
    else IfTestExist(TestEzCnt_OrderableDico_FindsByKey);
    else IfTestExist(TestEzCnt_OrderableDico_ClearEmptiesIt);
    return false;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

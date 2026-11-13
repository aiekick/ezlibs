#include <ezlibs/ezPlugin.hpp>
#include <ezlibs/ezCTest.hpp>
#include <ezPluginFixture.h>
#include <string>

#ifndef WINDOWS_OS
#include <unistd.h>
#endif

////////////////////////////////////////////////////////////////////////////
//// Test fixtures /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

namespace {

class FixtureInterface {
public:
    virtual ~FixtureInterface() {
    }
    virtual bool init() = 0;
    virtual void unit() = 0;
};

typedef ez::plugin::PluginInstance<FixtureInterface> FixtureInstance;

// a shared library every machine of the os has, and that is no plugin
std::string fixtureSystemLibrary() {
#ifdef WINDOWS_OS
    char systemDir[MAX_PATH + 1] = {};
    GetSystemDirectoryA(systemDir, MAX_PATH);
    return std::string(systemDir) + "\\kernel32.dll";
#elif defined(LINUX_OS)
    return "libc.so.6";
#else
    return "libSystem.dylib";
#endif
}

typedef ez::plugin::PluginInstance<IEzPluginFixture> FixturePluginInstance;

// the plugin built beside the suite : its folder, and its file name alone
std::string fixturePluginPath() {
    return EZ_TEST_PLUGIN_PATH;
}

std::string fixturePluginDir() {
    const std::string path = fixturePluginPath();
    const size_t lastSep = path.find_last_of("/\\");
    return (lastSep == std::string::npos) ? std::string(".") : path.substr(0u, lastSep);
}

std::string fixturePluginFileName() {
    const std::string path = fixturePluginPath();
    const size_t lastSep = path.find_last_of("/\\");
    return (lastSep == std::string::npos) ? path : path.substr(lastSep + 1u);
}

// the working directory moved for the time of a test
class FixtureWorkingDir {
private:
    std::string m_previous;

public:
    explicit FixtureWorkingDir(const std::string& aDir) {
#ifdef WINDOWS_OS
        char current[MAX_PATH + 1] = {};
        GetCurrentDirectoryA(MAX_PATH, current);
        m_previous = current;
        SetCurrentDirectoryA(aDir.c_str());
#else
        char current[4096] = {};
        if (getcwd(current, sizeof(current) - 1u) != nullptr) {
            m_previous = current;
        }
        if (chdir(aDir.c_str()) != 0) {
            m_previous.clear();
        }
#endif
    }
    ~FixtureWorkingDir() {
        if (m_previous.empty()) {
            return;
        }
#ifdef WINDOWS_OS
        SetCurrentDirectoryA(m_previous.c_str());
#else
        if (chdir(m_previous.c_str()) != 0) {
            m_previous.clear();
        }
#endif
    }
};

}  // namespace

////////////////////////////////////////////////////////////////////////////
//// Tests /////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzPlugin_DllExtensionIsKnown() {
    const auto extension = ez::plugin::getDLLExtention();
    CTEST_ASSERT(!extension.empty());
    CTEST_ASSERT(extension.find('.') == std::string::npos);
    return true;
}

bool TestEzPlugin_NeverLoadedInstanceIsDestroyable() {
    {
        FixtureInstance instance;
        CTEST_ASSERT(instance.get().expired());
    }
    return true;
}

bool TestEzPlugin_MissingFileFails() {
    FixtureInstance instance;
    const auto ret = instance.init("missing", "ez_plugin_test_file_that_does_not_exist." + ez::plugin::getDLLExtention());
    CTEST_ASSERT(ret == FixtureInstance::PluginReturnMsg::LOADING_FAILED);
    CTEST_ASSERT(instance.get().expired());
    return true;
}

bool TestEzPlugin_LibraryWithoutSymbolsIsNotAPlugin() {
    FixtureInstance instance;
    const auto ret = instance.init("system", fixtureSystemLibrary());
    CTEST_ASSERT(ret == FixtureInstance::PluginReturnMsg::NOT_A_PLUGIN);
    CTEST_ASSERT(instance.get().expired());
    return true;
}

bool TestEzPlugin_UnitTwiceIsSafe() {
    FixtureInstance instance;
    instance.init("system", fixtureSystemLibrary());
    instance.unit();
    instance.unit();
    CTEST_ASSERT(instance.get().expired());
    return true;
}

// a library exporting the two symbols is loaded, its plugin made and inited, and gone after unit
bool TestEzPlugin_LoadsAPlugin() {
    FixturePluginInstance instance;
    const auto ret = instance.init("fixture", fixturePluginPath());
    CTEST_ASSERT(ret == FixturePluginInstance::PluginReturnMsg::LOADING_SUCCEED);
    CTEST_ASSERT(!instance.get().expired());
    instance.unit();
    CTEST_ASSERT(instance.get().expired());
    return true;
}

// an instance inited again lets its first plugin go the way unit does : asked to unit, then released
bool TestEzPlugin_SecondInitUnitsTheFirst() {
    // the keeper holds the library loaded : its count of units outlives the instance under test
    FixturePluginInstance keeper;
    CTEST_ASSERT(keeper.init("keeper", fixturePluginPath()) == FixturePluginInstance::PluginReturnMsg::LOADING_SUCCEED);
    const auto pKeeper = keeper.get().lock();
    CTEST_ASSERT(pKeeper != nullptr);
    const int32_t unitsBefore = pKeeper->unitCount();
    FixturePluginInstance instance;
    CTEST_ASSERT(instance.init("first", fixturePluginPath()) == FixturePluginInstance::PluginReturnMsg::LOADING_SUCCEED);
    CTEST_ASSERT(pKeeper->unitCount() == unitsBefore);
    CTEST_ASSERT(instance.init("second", fixturePluginPath()) == FixturePluginInstance::PluginReturnMsg::LOADING_SUCCEED);
    CTEST_ASSERT(pKeeper->unitCount() == unitsBefore + 1);
    CTEST_ASSERT(!instance.get().expired());
    return true;
}

// a plugin is found by a path relative to the working directory too
bool TestEzPlugin_RelativePathLoads() {
    const FixtureWorkingDir workingDir(fixturePluginDir());
    FixturePluginInstance instance;
    const auto ret = instance.init("relative", "./" + fixturePluginFileName());
    CTEST_ASSERT(ret == FixturePluginInstance::PluginReturnMsg::LOADING_SUCCEED);
    CTEST_ASSERT(!instance.get().expired());
    return true;
}

////////////////////////////////////////////////////////////////////////////
//// Dispatch //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzPlugin(const std::string& vTest) {
    IfTestExist(TestEzPlugin_DllExtensionIsKnown);
    else IfTestExist(TestEzPlugin_NeverLoadedInstanceIsDestroyable);
    else IfTestExist(TestEzPlugin_MissingFileFails);
    else IfTestExist(TestEzPlugin_LibraryWithoutSymbolsIsNotAPlugin);
    else IfTestExist(TestEzPlugin_UnitTwiceIsSafe);
    else IfTestExist(TestEzPlugin_LoadsAPlugin);
    else IfTestExist(TestEzPlugin_SecondInitUnitsTheFirst);
    else IfTestExist(TestEzPlugin_RelativePathLoads);
    return false;
}

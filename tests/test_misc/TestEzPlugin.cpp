#include <ezlibs/ezPlugin.hpp>
#include <ezlibs/ezCTest.hpp>
#include <string>

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

////////////////////////////////////////////////////////////////////////////
//// Dispatch //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

bool TestEzPlugin(const std::string& vTest) {
    IfTestExist(TestEzPlugin_DllExtensionIsKnown);
    else IfTestExist(TestEzPlugin_NeverLoadedInstanceIsDestroyable);
    else IfTestExist(TestEzPlugin_MissingFileFails);
    else IfTestExist(TestEzPlugin_LibraryWithoutSymbolsIsNotAPlugin);
    else IfTestExist(TestEzPlugin_UnitTwiceIsSafe);
    return false;
}

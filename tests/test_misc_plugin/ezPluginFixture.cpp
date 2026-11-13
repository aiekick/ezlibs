#include "ezPluginFixture.h"

#if defined(_WIN32)
#define EZ_PLUGIN_FIXTURE_API __declspec(dllexport)
#else
#define EZ_PLUGIN_FIXTURE_API __attribute__((visibility("default")))
#endif

namespace {

// lives as long as the library stays loaded
int32_t& unitCounter() {
    static int32_t sUnitCount = 0;
    return sUnitCount;
}

class EzPluginFixture : public IEzPluginFixture {
public:
    bool init() override {
        return true;
    }
    void unit() override {
        ++unitCounter();
    }
    int32_t unitCount() const override {
        return unitCounter();
    }
};

}  // namespace

// the two symbols ez::plugin::Loader looks for
extern "C" {

EZ_PLUGIN_FIXTURE_API IEzPluginFixture* allocator() {
    return new EzPluginFixture();
}

EZ_PLUGIN_FIXTURE_API void deleter(IEzPluginFixture* apPlugin) {
    delete apPlugin;
}

}

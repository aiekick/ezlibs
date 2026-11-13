#pragma once

#include <cstdint>

// the interface shared by the tests of ezPlugin and the plugin they load
class IEzPluginFixture {
public:
    virtual ~IEzPluginFixture() {
    }
    virtual bool init() = 0;
    virtual void unit() = 0;
    // how many plugins of the library were asked to unit, since it was loaded
    virtual int32_t unitCount() const = 0;
};

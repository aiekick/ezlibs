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

// ezPlugin is part of the ezLibs project : https://github.com/aiekick/ezLibs.git

#include "ezOS.hpp"

#include <fstream>
#include <iterator>
#include <sstream>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <ctime>

#include "ezLog.hpp"

#ifdef WINDOWS_OS
#include "Windows.h"
#else
#include <dlfcn.h>
#endif

namespace ez {
namespace plugin {

template <class T>
class Loader {
private:
#ifdef WINDOWS_OS
    HMODULE _handle = nullptr;
#else // UNIX
    void *_handle = nullptr;
#endif // WINDOWS_OS / UNIX

    std::string _pathToLib;
    std::string _allocClassSymbol;
    std::string _deleteClassSymbol;
    bool _isAPlugin = true;

public:
    Loader() = default;
    Loader(std::string const &pathToLib, std::string const &allocClassSymbol = "allocator", std::string const &deleteClassSymbol = "deleter")
        : _handle(nullptr), _pathToLib(pathToLib), _allocClassSymbol(allocClassSymbol), _deleteClassSymbol(deleteClassSymbol) {}
    ~Loader() = default;

    bool isValid() const  { return (_handle != nullptr); }

    bool isAPlugin() const  { return _isAPlugin; }

    void dlOpenLib() {
#ifdef WINDOWS_OS
        _handle = LoadLibraryExA(
            _pathToLib.c_str(),
            NULL,  //
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!isValid()) {
            const auto &dw = GetLastError();
            if (dw == ERROR_MOD_NOT_FOUND) {
                LogVarError("Can't open and load %s with error : ERROR_MOD_NOT_FOUND", _pathToLib.c_str());
            } else {
                LogVarError("Can't open and load %s", _pathToLib.c_str());
            }
        }
#else  // UNIX
        _handle = dlopen(_pathToLib.c_str(), RTLD_NOW | RTLD_LAZY);
        if (!isValid()) {
            LogVarError("%s", dlerror());
        }
#endif  // WINDOWS_OS / UNIX
    }

    std::shared_ptr<T> dlGetInstance() {
        using allocClass = T *(*)();
        using deleteClass = void (*)(T *);
        if (isValid()) {
#ifdef WINDOWS_OS
            auto allocFunc = reinterpret_cast<allocClass>(GetProcAddress(_handle, _allocClassSymbol.c_str()));
            auto deleteFunc = reinterpret_cast<deleteClass>(GetProcAddress(_handle, _deleteClassSymbol.c_str()));
#else  // UNIX
            auto allocFunc = reinterpret_cast<allocClass>(dlsym(_handle, _allocClassSymbol.c_str()));
            auto deleteFunc = reinterpret_cast<deleteClass>(dlsym(_handle, _deleteClassSymbol.c_str()));
#endif  // WINDOWS_OS / UNIX
            if (!allocFunc || !deleteFunc) {
                _isAPlugin = false;
                dlCloseLib();
                // LogVarDebugError("Can't find allocator or deleter symbol in %s", _pathToLib.c_str());
                return nullptr;
            }
            return std::shared_ptr<T>(allocFunc(), [deleteFunc](T *p) { deleteFunc(p); });
        }
        return nullptr;
    }

    void dlCloseLib() {
#ifdef WINDOWS_OS
        if (isValid() && (FreeLibrary(_handle) == 0)) {
#else  // UNIX
        if (isValid() && (dlclose(_handle) != 0)) {
#endif  // WINDOWS_OS / UNIX
        // LogVarDebugError("Can't close %s", _pathToLib.c_str());
        }
        // closed once : a second close would free a handle that is no more ours
        _handle = nullptr;
    }
};

inline std::string getDLLExtention() {
#ifdef WINDOWS_OS
    return "dll";
#elif defined(LINUX_OS)
    return "so";
#else
    return "dylib";
#endif
}

template<class T_PluginInterface>
class PluginInstance {
public:
    enum class PluginReturnMsg { LOADING_SUCCEED = 1, LOADING_FAILED = 0, NOT_A_PLUGIN = -1 };
    typedef std::shared_ptr<T_PluginInterface> PluginInterfacePtr;
    typedef std::weak_ptr<T_PluginInterface> PluginInterfaceWeak;

private:
    Loader<T_PluginInterface> m_Loader;
    PluginInterfacePtr m_PluginInstance = nullptr;
    std::string m_Name;

public:
    PluginInstance() = default;
    ~PluginInstance() { unit(); }

    PluginReturnMsg init(const std::string &vName, const std::string &vFilePathName) {
        m_Name = vName;
        m_Loader = Loader<T_PluginInterface>(vFilePathName);
        m_Loader.dlOpenLib();
        m_PluginInstance = m_Loader.dlGetInstance();
        if (m_Loader.isAPlugin()) {
            if (m_Loader.isValid()) {
                if (m_PluginInstance) {
                    if (!m_PluginInstance->init()) {
                        m_PluginInstance.reset();
                    } else {
                        return PluginReturnMsg::LOADING_SUCCEED;
                    }
                }
            }
            return PluginReturnMsg::LOADING_FAILED;
        }
        return PluginReturnMsg::NOT_A_PLUGIN;
    }

    void unit() {
        if (m_PluginInstance != nullptr) {
            m_PluginInstance->unit();
            m_PluginInstance.reset();
        }
        m_Loader.dlCloseLib();
    }

    PluginInterfaceWeak get() const { return m_PluginInstance; }
};

}  // namespace plugin
}  // namespace ez

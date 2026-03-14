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

// ezCurl is part of the ezLibs project : https://github.com/aiekick/ezLibs.git

#include <curl/curl.h>

#include <string>

namespace ez {
namespace curl {

class Fetcher {
protected:
    // Enable in-memory cookie engine (persistent across requests on this handle)
    void m_enableCookies() {
        if (m_pHandle != nullptr) {
            curl_easy_setopt(m_pHandle, CURLOPT_COOKIEFILE, "");
            curl_easy_setopt(m_pHandle, CURLOPT_COOKIEJAR, "");
        }
    }

private:
    CURL* m_pHandle{nullptr};
    struct curl_slist* m_pHeaders{nullptr};

    static size_t m_writeCallback(char* apData, size_t aSize, size_t aNmemb, void* apUserData) {
        auto totalSize = aSize * aNmemb;
        auto* pResponse = static_cast<std::string*>(apUserData);
        pResponse->append(apData, totalSize);
        return totalSize;
    }

public:
    bool init() {
        m_pHandle = curl_easy_init();
        return m_pHandle != nullptr;
    }

    void unit() {
        clearHeaders();
        if (m_pHandle != nullptr) {
            curl_easy_cleanup(m_pHandle);
            m_pHandle = nullptr;
        }
    }

    // Perform a GET request, returns the response body
    // aoErrorCode is cleared on success, filled with error message on failure
    std::string get(const std::string& aUrl, std::string& aoErrorCode) {
        std::string response;
        if (m_pHandle == nullptr) {
            aoErrorCode = "ez::curl::Fetcher not initialized";
            return response;
        }
        curl_easy_setopt(m_pHandle, CURLOPT_URL, aUrl.c_str());
        curl_easy_setopt(m_pHandle, CURLOPT_WRITEFUNCTION, m_writeCallback);
        curl_easy_setopt(m_pHandle, CURLOPT_WRITEDATA, &response);
        if (m_pHeaders != nullptr) { curl_easy_setopt(m_pHandle, CURLOPT_HTTPHEADER, m_pHeaders); }
        auto curlCode = curl_easy_perform(m_pHandle);
        if (curlCode != CURLE_OK) {
            aoErrorCode = curl_easy_strerror(curlCode);
        } else {
            aoErrorCode.clear();
        }
        return response;
    }

    void addHeader(const std::string& aHeader) { m_pHeaders = curl_slist_append(m_pHeaders, aHeader.c_str()); }

    void clearHeaders() {
        if (m_pHeaders != nullptr) {
            curl_slist_free_all(m_pHeaders);
            m_pHeaders = nullptr;
        }
    }

    void setUserAgent(const std::string& aUserAgent) {
        if (m_pHandle != nullptr) { curl_easy_setopt(m_pHandle, CURLOPT_USERAGENT, aUserAgent.c_str()); }
    }

    void setSSLVerify(bool aEnable) {
        if (m_pHandle != nullptr) {
            curl_easy_setopt(m_pHandle, CURLOPT_SSL_VERIFYPEER, aEnable ? 1L : 0L);
            curl_easy_setopt(m_pHandle, CURLOPT_SSL_VERIFYHOST, aEnable ? 2L : 0L);
        }
    }
};

}  // namespace curl
}  // namespace ez
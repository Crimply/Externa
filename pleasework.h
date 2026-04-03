//
// Created by Crimp on 3/25/2026.
//
#ifndef GET_URL_H
#define GET_URL_H

#include <string>
#include <iostream>
#include <curl/curl.h>

// Callback to write response data into a std::string
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

/**
 * Perform an HTTP GET request and return the response body.
 *
 * @param url The URL to request.
 * @return The response body as a string. If the request fails (e.g., network error,
 *         non‑200 HTTP status), an empty string is returned and an error message
 *         is printed to stderr.
 */
static std::string getUrl(const std::string& url) {
    curl_global_init(CURL_GLOBAL_ALL);

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "getUrl: Failed to initialize libcurl\n";
        curl_global_cleanup();
        return "";
    }

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "getUrl: curl_easy_perform failed: " << curl_easy_strerror(res) << "\n";
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return "";
    }

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    if (httpCode != 200) {
        std::cerr << "getUrl: HTTP request returned code " << httpCode << "\n";
        return "";
    }

    return response;
}

#endif // GET_URL_H
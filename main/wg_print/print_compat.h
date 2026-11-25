#pragma once
// Narrow Arduino compatibility for the imported LoveLH printer implementation.
#include <string>
#include <utility>
#include <type_traits>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <esp_intr_alloc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
class String {
    std::string s_;
public:
    String() = default;
    String(const char* s): s_(s ? s : "") {}
    String(const char* s, size_t n): s_(s,n) {}
    String(std::string s): s_(std::move(s)) {}
    template<class T, std::enable_if_t<std::is_integral_v<T> || std::is_enum_v<T>,int> = 0>
    explicit String(T n): s_(std::to_string(static_cast<long long>(n))) {}
    size_t length() const { return s_.size(); }
    const char* c_str() const { return s_.c_str(); }
    void reserve(size_t n) { s_.reserve(n); }
    int indexOf(char c, size_t from=0) const { auto p=s_.find(c,from); return p==std::string::npos ? -1 : (int)p; }
    String substring(size_t start, size_t end=std::string::npos) const { start=std::min(start,s_.size()); end=std::min(end,s_.size()); return s_.substr(start,end>start?end-start:0); }
    void trim() { auto a=s_.find_first_not_of(" \t\r\n"); if(a==std::string::npos) s_.clear(); else s_=s_.substr(a,s_.find_last_not_of(" \t\r\n")-a+1); }
    void toUpperCase() { for(auto& c:s_) c=(char)std::toupper((unsigned char)c); }
    String& operator+=(const String& o) { s_+=o.s_; return *this; }
    String& operator+=(char c) { s_+=c; return *this; }
    friend String operator+(String a,const String& b) { return a+=b; }
    friend bool operator==(const String& a,const String& b) { return a.s_==b.s_; }
    friend bool operator!=(const String& a,const String& b) { return !(a==b); }
};
inline unsigned long millis() { return (unsigned long)(esp_timer_get_time()/1000); }
inline void yield() { vTaskDelay(1); }
inline void* ps_malloc(size_t n) { return heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT); }
struct PrintSerial { template<class... T> void printf(const char* f,T... a) { std::printf(f,a...); } void println(const char* s) { std::printf("%s\n",s); } };
inline PrintSerial Serial;

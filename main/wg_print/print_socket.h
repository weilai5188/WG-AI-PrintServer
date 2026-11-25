#pragma once
#include "print_compat.h"
#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <lwip/tcp.h>
#include <esp_netif.h>
#include <cerrno>
#include <utility>
#include <fcntl.h>
class PrintIP {
    in_addr addr_{};
public:
    PrintIP()=default; explicit PrintIP(in_addr a):addr_(a){}
    String toString() const { char b[INET_ADDRSTRLEN]{}; return inet_ntop(AF_INET,&addr_,b,sizeof(b)) ? String(b) : String("0.0.0.0"); }
};
struct PrintWifi { PrintIP localIP() const { esp_netif_ip_info_t i{}; auto n=esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"); if(n) esp_netif_get_ip_info(n,&i); in_addr a{}; a.s_addr=i.ip.addr; return PrintIP(a); } };
inline PrintWifi WiFi;
class WiFiClient {
    int fd_=-1; in_addr addr_{};
public:
    WiFiClient()=default; WiFiClient(int f,in_addr a):fd_(f),addr_(a){}
    ~WiFiClient(){stop();}
    WiFiClient(const WiFiClient&)=delete; WiFiClient& operator=(const WiFiClient&)=delete;
    WiFiClient(WiFiClient&& o) noexcept:fd_(std::exchange(o.fd_,-1)),addr_(o.addr_){}
    WiFiClient& operator=(WiFiClient&& o) noexcept { if(this!=&o){stop();fd_=std::exchange(o.fd_,-1);addr_=o.addr_;}return *this; }
    int fd() const{return fd_;} explicit operator bool() const{return fd_>=0;}
    void stop(){if(fd_>=0){lwip_close(fd_);fd_=-1;}}
    PrintIP remoteIP() const{return PrintIP(addr_);}
    int available() const{int n=0;return fd_>=0 && lwip_ioctl(fd_,FIONREAD,&n)==0 ? n:0;}
    // 1: data or still open, 0: orderly peer FIN, -1: socket error.
    int peerState() const { if(fd_<0)return -1; uint8_t b; int n=lwip_recv(fd_,&b,1,MSG_PEEK|MSG_DONTWAIT); if(n>0)return 1; if(n==0)return 0; return (errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR)?1:-1; }
    bool connected() const { return peerState()==1; }
    int read(uint8_t* b,size_t n){if(fd_<0)return 0;int r=lwip_recv(fd_,b,n,MSG_DONTWAIT);return r>0?r:0;}
    int availableForWrite() const {if(fd_<0)return 0;fd_set w;FD_ZERO(&w);FD_SET(fd_,&w);timeval tv{};return lwip_select(fd_+1,nullptr,&w,nullptr,&tv)>0?512:0;}
    size_t write(const uint8_t* b,size_t n){if(fd_<0)return 0;int r=lwip_send(fd_,b,n,MSG_DONTWAIT);return r>0?(size_t)r:0;}
};
class WiFiServer {
    int fd_=-1; uint16_t port_; bool noDelay_=true;
public:
    explicit WiFiServer(uint16_t p):port_(p){}
    ~WiFiServer(){if(fd_>=0)lwip_close(fd_);}
    bool begin(){if(fd_>=0)return true;fd_=lwip_socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(fd_<0)return false;int yes=1;lwip_setsockopt(fd_,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));sockaddr_in a{};a.sin_family=AF_INET;a.sin_port=htons(port_);a.sin_addr.s_addr=htonl(INADDR_ANY);if(lwip_bind(fd_,(sockaddr*)&a,sizeof(a))<0||lwip_listen(fd_,4)<0||lwip_fcntl(fd_,F_SETFL,O_NONBLOCK)<0){lwip_close(fd_);fd_=-1;return false;}return true;}
    void setNoDelay(bool v){noDelay_=v;}
    bool hasClient() const{if(fd_<0)return false;fd_set r;FD_ZERO(&r);FD_SET(fd_,&r);timeval tv{};return lwip_select(fd_+1,&r,nullptr,nullptr,&tv)>0;}
    WiFiClient accept(){if(fd_<0)return {};sockaddr_in a{};socklen_t n=sizeof(a);int f=lwip_accept(fd_,(sockaddr*)&a,&n);if(f<0)return {};int yes=noDelay_;lwip_setsockopt(f,IPPROTO_TCP,TCP_NODELAY,&yes,sizeof(yes));if(lwip_fcntl(f,F_SETFL,O_NONBLOCK)<0){lwip_close(f);return {};}return WiFiClient(f,a.sin_addr);}
};

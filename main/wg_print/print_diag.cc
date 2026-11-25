#include "print_diag.h"
#include <esp_http_server.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <mutex>
#include <string>
#include <cstdio>
namespace {
std::mutex lock;
std::string snapshot = "Waiting for print task";
int64_t updated = 0;
httpd_handle_t server = nullptr;
constexpr char page[] = R"HTML(<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>威哥 AI 打印助手 P2</title><style>body{background:#101827;color:#e5edf7;font:16px system-ui;margin:24px}main{max-width:850px;margin:auto}pre{background:#1d293b;padding:18px;border-radius:12px;line-height:1.65;white-space:pre-wrap;overflow-wrap:anywhere}small{color:#a5b4ca}</style><main><h2>威哥 AI 打印助手 P2</h2><small>只读诊断 · RAW 9100 · 自动刷新 · 数据传输完成不代表纸张完成</small><pre id="s">连接中…</pre><script>async function tick(){try{let r=await fetch('/api/status',{cache:'no-store'});if(!r.ok)throw Error(r.status);document.getElementById('s').textContent=await r.text()}catch(e){document.getElementById('s').textContent='读取失败: '+e}setTimeout(tick,1000)}tick()</script></main></html>)HTML";
esp_err_t root(httpd_req_t* req){httpd_resp_set_type(req,"text/html; charset=utf-8");return httpd_resp_send(req,page,HTTPD_RESP_USE_STRLEN);}
esp_err_t state(httpd_req_t* req){
  std::string copy; int64_t stamp;
  {std::lock_guard<std::mutex> g(lock);copy=snapshot;stamp=updated;}
  char tail[320];
  snprintf(tail,sizeof(tail),"\n快照距今: %lld ms\n内部可用内存: %u B\n最大内部连续块: %u B\nPSRAM 总容量: %u B\nPSRAM 可用: %u B\n",(long long)(stamp?(esp_timer_get_time()-stamp)/1000:0),
  (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
  (unsigned)heap_caps_get_total_size(MALLOC_CAP_SPIRAM),(unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  copy += tail; httpd_resp_set_type(req,"text/plain; charset=utf-8");httpd_resp_set_hdr(req,"Cache-Control","no-store");return httpd_resp_send(req,copy.c_str(),copy.size());
}
}
void WgPrintDiagPublish(const char* text){std::lock_guard<std::mutex> g(lock);snapshot=text;updated=esp_timer_get_time();}
void WgPrintDiagStart(){
  if (server) return;
  httpd_config_t cfg=HTTPD_DEFAULT_CONFIG();
  cfg.server_port=8080; cfg.ctrl_port=32770; cfg.max_open_sockets=2; cfg.lru_purge_enable=true;
  auto err=httpd_start(&server,&cfg);if(err!=ESP_OK){ESP_LOGE("WG_PRINT","Diagnostic HTTP failed: %s",esp_err_to_name(err));return;}
  httpd_uri_t a{};a.uri="/";a.method=HTTP_GET;a.handler=root;
  httpd_uri_t b{};b.uri="/api/status";b.method=HTTP_GET;b.handler=state;
  if(httpd_register_uri_handler(server,&a)!=ESP_OK||httpd_register_uri_handler(server,&b)!=ESP_OK){httpd_stop(server);server=nullptr;return;}
  ESP_LOGI("WG_PRINT","Diagnostic HTTP listening :8080");
}

#include "wg_print_service.h"
#include "print_server.h"
#include "mcp_server.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_task_wdt.h"
#include <atomic>
#include <mutex>
#include <string>
namespace {
constexpr char TAG[]="WG_PRINT";
std::atomic<bool> netChanged{false};
std::mutex statusLock;
std::string status="打印服务正在初始化";
void setStatus(std::string s) { std::lock_guard<std::mutex> l(statusLock); status=std::move(s); }
void netEvent(void*,esp_event_base_t,int32_t,void*) { netChanged=true; }
void usbTask(void* arg) { auto& p=*static_cast<UsbPrinter*>(arg); for(;;){ p.task(); vTaskDelay(1); } }
void worker(void*) {
    static UsbPrinter printer;
    static PrintServer server(printer);
    if(!printer.begin()) { setStatus("USB Host 初始化失败"); ESP_LOGE(TAG,"USB Host init failed"); vTaskDelete(nullptr); return; }
    if(xTaskCreate(usbTask,"wg_usb_printer",USB_HOST_TASK_STACK_SIZE,&printer,USB_HOST_TASK_PRIORITY,nullptr)!=pdPASS){setStatus("USB 任务创建失败");vTaskDelete(nullptr);return;}
    esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,netEvent,nullptr);
    // Allow the existing Wi-Fi manager to connect and retain full ownership of Wi-Fi.
    for(;;){auto ip=WiFi.localIP().toString();if(ip!="0.0.0.0")break; vTaskDelay(pdMS_TO_TICKS(500));}
    if(!server.begin()){setStatus("RAW 9100 启动失败");vTaskDelete(nullptr);return;}
    netChanged=false;
    bool wdt=esp_task_wdt_add(nullptr)==ESP_OK;
    unsigned long last=0;
    for(;;){
        if(wdt)esp_task_wdt_reset();
        if(netChanged.exchange(false))server.onNetworkUp();
        server.task();
        if(millis()-last>=1000){last=millis();PrinterInfo pi; printer.getPrinterInfoCopy(pi,20);auto job=server.getCurrentJob();
            auto ip=WiFi.localIP().toString();auto state=printer.getStateText();
            std::string s="打印机："+std::string(pi.mdl.c_str())+"；状态："+state.c_str()+"；IP："+ip.c_str()+"；RAW端口9100；作业："+(job.active?"传输中":"空闲")+"；已发送字节："+std::to_string(job.bytesSent)+"；累计作业："+std::to_string(server.getTotalJobs());
            setStatus(std::move(s));
        }
        vTaskDelay(1);
    }
}
}
void WgPrintStart(){
    McpServer::GetInstance().AddTool("self.printer.get_status","查询无线打印机连接、IP地址和数据传输状态；不代表纸张已打印完成",PropertyList(),[](const PropertyList&)->ReturnValue{std::lock_guard<std::mutex> l(statusLock);return status;});
    if(xTaskCreate(worker,"wg_print_raw",PRINT_TASK_STACK_SIZE,nullptr,PRINT_TASK_PRIORITY,nullptr)!=pdPASS){setStatus("打印服务任务创建失败");ESP_LOGE(TAG,"Print task allocation failed");}
}

#pragma once
#include <esp_log.h>
#include <esp_intr_alloc.h>
#include <usb/usb_host.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Phase 1: independent host enumeration only. No interface claim or print writes.
namespace wg_print_probe {
inline constexpr char TAG[] = "WG_PRINT";
inline void LibraryTask(void*) {
    for (;;) {
        uint32_t flags = 0;
        esp_err_t err = usb_host_lib_handle_events(portMAX_DELAY, &flags);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Host events: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}
inline void Event(const usb_host_client_event_msg_t* msg, void* arg) {
    auto client = *static_cast<usb_host_client_handle_t*>(arg);
    if (msg->event == USB_HOST_CLIENT_EVENT_NEW_DEV) {
        usb_device_handle_t dev = nullptr;
        const uint8_t addr = msg->new_dev.address;
        esp_err_t err = usb_host_device_open(client, addr, &dev);
        if (err != ESP_OK) { ESP_LOGW(TAG, "Open %u: %s", addr, esp_err_to_name(err)); return; }
        const usb_device_desc_t* desc = nullptr;
        err = usb_host_get_device_descriptor(dev, &desc);
        if (err == ESP_OK && desc) {
            ESP_LOGI(TAG, "USB connected addr=%u VID=%04x PID=%04x", addr, desc->idVendor, desc->idProduct);
        } else { ESP_LOGW(TAG, "Descriptor: %s", esp_err_to_name(err)); }
        usb_host_device_close(client, dev);
    } else if (msg->event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
        ESP_LOGI(TAG, "USB device gone");
    }
}
inline void ClientTask(void*) {
    usb_host_client_handle_t client = nullptr;
    usb_host_client_config_t cfg = {};
    cfg.is_synchronous = false;
    cfg.max_num_event_msg = 8;
    cfg.async.client_event_callback = Event;
    cfg.async.callback_arg = &client;
    esp_err_t err = usb_host_client_register(&cfg, &client);
    if (err != ESP_OK) { ESP_LOGE(TAG, "Client register: %s", esp_err_to_name(err)); vTaskDelete(nullptr); return; }
    ESP_LOGI(TAG, "USB enumeration probe ready");
    for (;;) {
        err = usb_host_client_handle_events(client, portMAX_DELAY);
        if (err != ESP_OK) { ESP_LOGW(TAG, "Client events: %s", esp_err_to_name(err)); vTaskDelay(pdMS_TO_TICKS(100)); }
    }
}
}
inline void WgPrintProbeStart() {
    usb_host_config_t cfg = {};
    cfg.skip_phy_setup = false;
    cfg.intr_flags = ESP_INTR_FLAG_LEVEL1;
    esp_err_t err = usb_host_install(&cfg);
    if (err != ESP_OK) { ESP_LOGE(wg_print_probe::TAG, "Host install: %s", esp_err_to_name(err)); return; }
    TaskHandle_t lib = nullptr;
    if (xTaskCreate(wg_print_probe::LibraryTask, "print_usb_lib", 4096, nullptr, 5, &lib) != pdPASS) {
        usb_host_uninstall(); ESP_LOGE(wg_print_probe::TAG, "Host task allocation failed"); return;
    }
    if (xTaskCreate(wg_print_probe::ClientTask, "print_usb_client", 4096, nullptr, 4, nullptr) != pdPASS) {
        ESP_LOGE(wg_print_probe::TAG, "Client task allocation failed");
    }
}

#include "DockingStation.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_bt.h"
#include <cstring>
#include <arpa/inet.h>

extern "C" {
    #include "wifi_provisioning/manager.h"
    #include "wifi_provisioning/scheme_ble.h"
}

static const char* TAG = "DockingStation";

#define WIFI_CONNECT_TIMEOUT_MS 10000
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

DockingStation::DockingStation() {
    ESP_ERROR_CHECK(nvs_flash_init());
}

void DockingStation::start() {
    if (!hasStoredCredentials() || !connectToWiFi()) {
		connectToWiFi();
        //startBLEProvisioning();
    }
}

bool DockingStation::hasStoredCredentials() {
    nvs_handle_t nvs;
    if (nvs_open("wifi", NVS_READONLY, &nvs) != ESP_OK) return false;

    size_t len = 0;
    bool hasSSID = (nvs_get_str(nvs, "ssid", nullptr, &len) == ESP_OK);
    bool hasPass = (nvs_get_str(nvs, "pass", nullptr, &len) == ESP_OK);
    nvs_close(nvs);
    return hasSSID && hasPass;
}

int retry_num=0;
static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,void *event_data){

	if(event_id == WIFI_EVENT_STA_START) {
  		ESP_LOGD(TAG, "WIFI CONNECTING....\n");
	} else if (event_id == WIFI_EVENT_STA_CONNECTED) {
  		ESP_LOGD(TAG, "WiFi CONNECTED\n");
	} else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
  		ESP_LOGD(TAG, "WiFi lost connection\n");
  		if (retry_num < 5) {
			esp_wifi_connect();retry_num++;printf("Retrying to Connect...\n");
		}
	} else if (event_id == IP_EVENT_STA_GOT_IP) {
 		ESP_LOGD(TAG, "Wifi got IP...\n\n");
	}
}

bool DockingStation::connectToWiFi() {
	ESP_LOGD(TAG, "esp_netif_init()");
	ESP_ERROR_CHECK(esp_netif_init()); //network interdace initialization
	ESP_LOGD(TAG, "esp_event_loop_create_default()");
	ESP_ERROR_CHECK(esp_event_loop_create_default()); //responsible for handling and dispatching events
	ESP_LOGD(TAG, "esp_netif_create_default_wifi_sta()");
	esp_netif_create_default_wifi_sta(); //sets up necessary data structs for wifi station interface
	wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();//sets up wifi wifi_init_config struct with default values
	ESP_LOGD(TAG, "esp_wifi_init(&wifi_initiation)");
	ESP_ERROR_CHECK(esp_wifi_init(&wifi_initiation)); //wifi initialised with dafault wifi_initiation

	// register events
	ESP_LOGD(TAG, "register events");
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));//creating event handler register for wifi
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));//creating event handler register for ip event

 	wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.sta.ssid, "Osnat5");
    strcpy((char*)wifi_config.sta.password, "password2222");

/*
	wifi_config_t wifi_configuration ={ //struct wifi_config_t var wifi_configuration
		.sta= {
    		.ssid = "Osnat5"
    		.password= "password2222"
  		}//also this part is used if you donot want to use Kconfig.projbuild
	};
*/
	ESP_LOGD(TAG, "esp_wifi_set_config(WIFI_IF_STA, &wifi_config)");
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));//setting up configs when event ESP_IF_WIFI_STA
	ESP_LOGD(TAG, "esp_wifi_start()");
	ESP_ERROR_CHECK(esp_wifi_start());//start connection with configurations provided in funtion
	ESP_LOGD(TAG, "esp_wifi_set_mode(WIFI_MODE_STA)");
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));//station mode selected
	ESP_LOGD(TAG, "esp_wifi_connect()");
	ESP_ERROR_CHECK(esp_wifi_connect()); //connect with saved ssid and pass
	//printf( "wifi_init_softap finished. SSID:%s  password:%s",ssid,pass);
	return true;

}

void DockingStation::startBLEProvisioning() {
	// Init TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Create default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create default Wi-Fi station
    esp_netif_create_default_wifi_sta();

    // Init Wi-Fi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	std::string devName = getDeviceName();
   	const char *service_name = devName.c_str();

    wifi_prov_mgr_config_t config = {
	 	.scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE,
        .app_event_handler = {
            .event_cb = NULL,
            .user_data = NULL
        }
    };

	ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

    ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(
        WIFI_PROV_SECURITY_1, NULL, service_name, NULL));

    ESP_LOGI(TAG, "BLE provisioning started with name: %s", service_name);
}

std::string DockingStation::getDeviceName() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    char name[32];
    snprintf(name, sizeof(name), "ioto_%02X%02X%02X", mac[3], mac[4], mac[5]);
    return std::string(name);
}

std::string DockingStation::getIpAddress() {
    esp_netif_ip_info_t ip_info;
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

    if (!netif) {
        ESP_LOGW(TAG, "❌ Failed to get netif handle.");
        return "0.0.0.0";
    }

    esp_err_t err = esp_netif_get_ip_info(netif, &ip_info);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "❌ Failed to get IP info: %s", esp_err_to_name(err));
        return "0.0.0.0";
    }

    char ipStr[INET_ADDRSTRLEN];
    snprintf(ipStr, sizeof(ipStr), IPSTR, IP2STR(&ip_info.ip));
    return std::string(ipStr);
}


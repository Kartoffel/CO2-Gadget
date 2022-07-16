#ifndef CO2_Gadget_ESP_NOW
#define CO2_Gadget_ESP_NOW

/*****************************************************************************************************/
/*********                                                                                   *********/
/*********                           SETUP ESP-NOW FUNCTIONALITY                             *********/
/*********                                                                                   *********/
/*****************************************************************************************************/

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// IN CO2_Gadget.ino: String rootTopic = MQTT_TOPIC_BASE UNITHOSTNAME

bool EspNowInititialized = false;
// uint8_t peerESPNowAddress[] = ESPNOW_PEER_MAC_ADDRESS;

// Peer info
esp_now_peer_info_t peerInfo;


  /*  float temp;
    float hum;
    uint16_t co2;
    float battery;
    int readingId;
    int command = cmdCO2GadgetNone;
    uint16_t parameter = 0;
  */

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    Serial.printf("-->[ESPN] Last packet sent to %s with status: %s\n", macStr, (status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail"));
}

void printESPNowError(esp_err_t result) {
    Serial.print("-->[ESPN] Error ");
    Serial.print(result);
    Serial.println(" sending the data");
    if (result == ESP_ERR_ESPNOW_NOT_INIT) {
        // How did we get so far!!
        Serial.println("-->[ESPN] ESPNOW not Init.");
    } else if (result == ESP_ERR_ESPNOW_ARG) {
        Serial.println("-->[ESPN] Invalid Argument");
    } else if (result == ESP_ERR_ESPNOW_INTERNAL) {
        Serial.println("-->[ESPN] Internal Error");
    } else if (result == ESP_ERR_ESPNOW_NO_MEM) {
        Serial.println("-->[ESPN] ESP_ERR_ESPNOW_NO_MEM");
    } else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
        Serial.println("-->[ESPN] Peer not found.");
    } else {
        Serial.println("-->[ESPN] Not sure what happened");
    }
}

void disableESPNow() {
    esp_err_t result = esp_now_deinit();
    if (result == ESP_OK) {
        Serial.println("-->[ESPN] ESP-NOW stoped");
    } else {
        Serial.println("-->[ESPN] Error stoping SP-NOW");
        printESPNowError(result);
    }
    activeESPNOW = false;
    EspNowInititialized = false;
}

void initESPNow() {
    if (!activeESPNOW) return;
    EspNowInititialized = false;
    if ((activeWIFI) && (WiFi.status() == WL_CONNECTED)) {
        channelESPNow = WiFi.channel();
        Serial.printf("-->[ESPN] Initializing ESP-NOW in already connected WiFi channel: %u\n", channelESPNow);
    } else {
        WiFi.mode(WIFI_STA);
        Serial.printf("-->[ESPN] Initializing ESP-NOW in channel: %u\n", channelESPNow);
    }

    esp_wifi_set_channel(channelESPNow, WIFI_SECOND_CHAN_NONE);

    //Init ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("-->[ESPN] Error initializing ESP-NOW");
        return;
    }

    //Register peer
    memcpy(peerInfo.peer_addr, peerESPNowAddress, 6);
    peerInfo.channel = channelESPNow;
    peerInfo.encrypt = false;

    //Add peer
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("-->[ESPN] Failed to add peer");
        return;
    } else {
        char macStr[18];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", peerESPNowAddress[0], peerESPNowAddress[1], peerESPNowAddress[2], peerESPNowAddress[3], peerESPNowAddress[4], peerESPNowAddress[5]);
        Serial.printf("-->[ESPN] Added ESP-NOW peer: %s\n", macStr);
    }

    // Register callback function that will be called when data is received
    // esp_now_register_recv_cb(OnDataRecv);

    // Register callback function to get the status of Trasnmitted packet
    esp_now_register_send_cb(OnDataSent);

    activeESPNOW = true;
    EspNowInititialized = true;
}

void send_topic_text(const char *topic, const char *text) {
    char buf[200];
    int n = snprintf(buf, sizeof(buf), "%s%s%s %s", "#R", rootTopic.c_str(), topic, text);

    // Send message via ESP-NOW
    esp_err_t result = esp_now_send(peerESPNowAddress, (uint8_t *)buf, n);
    if (result == ESP_OK) {
        Serial.println("-->[ESPN] Sent with success");
    } else {
        printESPNowError(result);
    }
}

void publishESPNow() {
    if ((!activeESPNOW) || (!EspNowInititialized)) return;
    if (millis() - lastTimeESPNowPublished >= timeBetweenESPNowPublish * 1000) {
        /*
        volatile uint16_t co2 = 0;
        float temp, tempFahrenheit, hum = 0;
        float battery_voltage = 0;
        */

        char text[120];

        sprintf(text, "%d", co2);
        send_topic_text("co2", text);

        sprintf(text, "%.1f", temp);
        send_topic_text("temp", text);

        sprintf(text, "%.0f", hum);
        send_topic_text("hum", text);

        sprintf(text, "%.2f", battery_voltage);
        send_topic_text("battery", text);

        lastTimeESPNowPublished = millis();
    }
}

#endif  // CO2_Gadget_ESP_NOW

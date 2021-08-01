/* HIAS ESP32 PIR Sensor

  A HIAS BLE server that provides readings from a PIR sensor.

  Based on https://github.com/nkolban/ESP32_BLE_Arduino/tree/master/examples/BLE_server_multiconnect
  
  MIT License
  
  Copyright (c) 2021 Asociación de Investigacion en Inteligencia Artificial
  Para la Leucemia Peter Moss
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files(the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
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
  
  Contributors:
  - Adam Milton-Barker
*/

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include <ArduinoJson.h>
#include <SPIFFS.h>

struct Config {
  String serviceUUID;
  String characteristicUUID;
  String deviceUUID;
  String deviceName;
};

Config config; 
int motion;

const int LED = 2; 
const int pirPin = 4;

char bleBuffer[200] = {0};

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

class DeviceBleCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      BLEDevice::startAdvertising();
      Serial.println("Client connected");
      digitalWrite(LED, HIGH);
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Client disconnected");
      digitalWrite(LED, LOW);
    }
};

void configs(Config &config)
{
  if (!SPIFFS.begin())
  {
    Serial.println(F("Failed to mount SPIFFS"));
  }
  else
  {
    File file = SPIFFS.open("/config.json", FILE_READ);
    if (!file)
    {
      return;
    }
    else
    {
      StaticJsonDocument<1048> doc;
      DeserializationError error = deserializeJson(doc, file);
      if (error)
      {
        Serial.println(error.c_str());
      }
      else
      {
        config.serviceUUID = doc["ble"]["service"].as<String>();
        config.characteristicUUID = doc["ble"]["characteristic"].as<String>();
        config.deviceUUID = doc["iotJumpWay"]["device"].as<String>();
        config.deviceName = doc["iotJumpWay"]["deviceName"].as<String>();
        Serial.println(F("Configuration loaded!"));
      }
    }
    file.close();
  }
}

void setup() {
  Serial.begin(115200);
  
  if(!SPIFFS.begin(true)){
    return;
  }
  
  configs(config);
  
  // Set ultrasonic trigger and echo pins
  pinMode(pirPin, INPUT);
  
  // Set LED pin
  pinMode(LED, OUTPUT);

  // Create the BLE Device
  BLEDevice::init(config.deviceName.c_str());

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new DeviceBleCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(config.serviceUUID.c_str());

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      config.characteristicUUID.c_str(),
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

  // Create a BLE Descriptor
  pCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(config.serviceUUID.c_str());
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0); 
  BLEDevice::startAdvertising();
  Serial.println("Awaiting client connection...");
}

int getReadings(){
  motion = digitalRead(pirPin);
}

void loop() {
    
    // Send measurement to client
    if (deviceConnected) {
        String sendString = "{\"EntityType\":\"Device\",\"Entity\":\"" + config.deviceUUID + "\",\"Sensor\":\"" + config.deviceName + "\",\"Type\":\"Motion\",\"Value\":\"" + String(motion) + "\",\"Message\":\"Current motion\"}";
        memset(bleBuffer, 0, 200);
        memcpy(bleBuffer, (char*)sendString.c_str(), 200); 
        pCharacteristic->setValue(bleBuffer);
        pCharacteristic->notify();
        String message = "Sent " + sendString + " to client";
        Serial.println(F(message.c_str()));
    }
    
    // Connection
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }
    
    // Disconnection
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); 
        pServer->startAdvertising();
        Serial.println("Client disconnected. Began advertising");
        oldDeviceConnected = deviceConnected;
    }
    
    // Get current distance measurement 
    getReadings();
}

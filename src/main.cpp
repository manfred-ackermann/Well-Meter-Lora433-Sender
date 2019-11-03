#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <ESP8266WiFi.h>
#include "main.h"

void setup() {
  WiFi.mode(WIFI_OFF);                      // Turn off WiFi
  WiFi.macAddress(mac);                     // Read MAC address

  Serial.begin(115200);
  while(!Serial);

  pinMode(LED_BUILTIN, OUTPUT);             // LED setup
  digitalWrite(LED_BUILTIN, 1);             // LED off

  noInterrupts();

    // interupt to collect data and trigger calculations
    timer0_isr_init();
    timer0_attachInterrupt(ISR_timer0);
    timer0_write(ESP.getCycleCount() + 20000000L); // start in 250ms (80M/20M=1/4)

  interrupts();

#ifdef LORA
  LoRa.setPins(D3,D1,D2);

  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed. Check your connections.");
    while (true); // if failed, do nothing
  }

  LoRa.setTxPower(20);
#endif
}

void loop() {
  delay(10);

  if(process) {
    // Sort array (BubbleSort)
    for(int i=0; i<(numReadings-1); i++) {
        for(int o=0; o<(numReadings-(i+1)); o++) {
                if(readings[o] > readings[o+1]) {
                    int t = readings[o];
                    readings[o] = readings[o+1];
                    readings[o+1] = t;
                }
        }
    }

    // get median value
    uint16_t median;
    if((numReadings & 1) == 0) {
      median = (readings[(numReadings/2)-1]+readings[numReadings/2])/2;
    } else {
      median = readings[numReadings/2];
    }

#ifdef DEBUG
    // print sorted array and median value
    for(int c=0;c<numReadings;c++) {
      Serial.print((uint16_t)readings[c]);
      if((c+1)<numReadings) Serial.print('|');
    }
    Serial.print('=');
    Serial.println(median);
#endif

#ifdef LORA
    // send packet
    LoRa.beginPacket();
    for(int c=0;c<6;c++) {             // Sender ID = WiFi MAC address
      LoRa.write((uint8_t)mac[c]);
    }
    LoRa.write((uint8_t)'L');           // Type: L = Level
    LoRa.write((uint8_t)(median >> 8)); // MSB
    LoRa.write((uint8_t)median);        // LSB
    LoRa.write(lookupCRC8(median));     // CRC
    LoRa.endPacket();
#endif

    process = false;
  }
}

/*
 * lookup a CRC8 over data
 */
uint8_t lookupCRC8(uint16_t data) {
  uint8_t CRC = 0xFF;                         // Inital value
  CRC ^= (uint8_t)(data >> 8);                // Start with MSB
  CRC = CRC8LookupTable[CRC >> 4][CRC & 0xF]; // Look up table [MSnibble][LSnibble]
  CRC ^= (uint8_t)data;                       // Use LSB
  CRC = CRC8LookupTable[CRC >> 4][CRC & 0xF]; // Look up table [MSnibble][LSnibble]
  return CRC;
}

/*
 * ISR
 * - Collect data every half seconds
 * - Process data every numReadings*0,5seconds
 */
void ICACHE_RAM_ATTR inline ISR_timer0() {
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); // Heartbeat
  if(!process) {                                        // Collect only when not processing
    readings[readIndex] = (uint16_t)analogRead(A0);
    readIndex = readIndex + 1;
  }
  if (readIndex >= numReadings) {                       // Start processing when data collected
    readIndex = 0;
    process = true;
  }
  timer0_write(ESP.getCycleCount() + 40000000L);        // come back in 0,5s (80E6/40E6)
}

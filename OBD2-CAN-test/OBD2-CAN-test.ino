/*
 * OBD-II CAN Bus Hardware Test
 * Adafruit RP2040 CAN Bus Feather + Adafruit_MCP2515 library
 *
 * Sends a Mode 01 request for engine coolant temperature (PID 0x05)
 * every 2 seconds and prints whatever comes back on the bus.
 */

#include <Adafruit_MCP2515.h>

#if defined(ARDUINO_ADAFRUIT_FEATHER_RP2040_CAN)
#define CS_PIN PIN_CAN_CS
#else
#define CS_PIN 5
#endif

// Try 500000 first. If setup succeeds but you NEVER see any traffic at all
// (not even "Other traffic seen..."), switch this to 250000 and re-upload —
// this vehicle's exact bus speed hasn't been confirmed.
#define CAN_BAUDRATE (500000)

// Standard OBD-II request/response IDs (ISO 15765-4)
#define OBD_REQUEST_ID 0x7DF
#define OBD_RESPONSE_ID 0x7E8

// Mode 01, PID 0x05 = Engine coolant temperature
#define OBD_MODE 0x01
#define OBD_PID 0x05

Adafruit_MCP2515 mcp(CS_PIN);

unsigned long lastRequestTime = 0;
const unsigned long requestInterval = 2000;  // send a request every 2 seconds

void sendCoolantTempRequest() {
  Serial.println("Sending coolant temp request...");
  mcp.beginPacket(OBD_REQUEST_ID);
  mcp.write(0x02);           // number of meaningful bytes that follow
  mcp.write(OBD_MODE);       // Mode 01: show current data
  mcp.write(OBD_PID);        // PID 0x05: coolant temp
  mcp.write((uint8_t)0x00);  // padding
  mcp.write((uint8_t)0x00);
  mcp.write((uint8_t)0x00);
  mcp.write((uint8_t)0x00);
  mcp.write((uint8_t)0x00);
  mcp.endPacket();
}

void setup() {
  Serial.begin(115200);
  delay(5000);
  while (!Serial) delay(10);

  Serial.println("OBD-II CAN hardware test starting...");

  if (!mcp.begin(CAN_BAUDRATE)) {
    Serial.println("Error initializing MCP2515. Check wiring/connections.");
    while (1) delay(10);
  }
  Serial.println("MCP2515 chip found. Listening for CAN traffic now.");
  Serial.println("Make sure vehicle ignition is ON (engine doesn't need to be running).");
}

void loop() {
  // Periodically send our request
  if (millis() - lastRequestTime >= requestInterval) {
    sendCoolantTempRequest();
    lastRequestTime = millis();
  }

  // Check for an incoming packet
  int packetSize = mcp.parsePacket();
  if (packetSize) {
    long id = mcp.packetId();

    if (id == OBD_RESPONSE_ID) {
      uint8_t data[8];
      int i = 0;
      while (mcp.available() && i < 8) {
        data[i++] = mcp.read();
      }

      Serial.print("Response from 0x7E8: ");
      for (int j = 0; j < i; j++) {
        Serial.print(data[j], HEX);
        Serial.print(" ");
      }
      Serial.println();

      // Confirm this is a valid Mode 01 response to the PID we asked for
      // (byte 0 = length, byte 1 = 0x41 is "response to mode 1", byte 2 = echoed PID)
      if (i >= 4 && data[1] == 0x41 && data[2] == OBD_PID) {
        int coolantTempC = data[3] - 40;
        Serial.print(">> Coolant temp: ");
        Serial.print(coolantTempC);
        Serial.println(" C");
      }
    }
    /*
    else {
      // Any other traffic on the bus - useful even before your own reply shows up,
      // since it confirms the bus itself is alive and your wiring/baud rate are correct.
      Serial.print("Other traffic seen, id 0x");
      Serial.println(id, HEX);
    }
    */
  }
}

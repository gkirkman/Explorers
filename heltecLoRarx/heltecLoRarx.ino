#include <heltec_unofficial.h>

#define FREQUENCY 915.0 // Set your matching transmitter frequency

// --- TEST MODE CONFIGURATION ---
#define TEST_MODE false  // Set to 'true' to generate fake data without TX. Set to 'false' for live LoRa.
// -------------------------------

// --- CRUCIAL FIX FOR SCOPE ERROR ---
volatile bool rxFlag = false;  // Flags when a real physical packet lands
void rxCallback() {            // Interrupt function called by the radio chip
  rxFlag = true;
}
// ------------------------------------

float receivedPressure = 0.0;
unsigned long lastTestTrigger = 0;
float mockBasePressure = 1013.25; // Standard sea-level pressure in hPa

// --- HELTEC HARDWARE POWER/RESET PINS ---
#define OLED_VEXT 36  // Controls power rail to the OLED display on V3/V3.2
#define OLED_RST  21  // Physical reset pin for the SSD1306 screen
// ----------------------------------------

void updateOutput(float pressure, float rssi, float snr);

void setup() {
  heltec_setup();
  Serial.begin(115200);

  // 1. Force screen hardware power rails BEFORE running the library setup
  pinMode(OLED_VEXT, OUTPUT);
  digitalWrite(OLED_VEXT, LOW); // LOW turns ON the Vext power rail on Heltec V3 modules
  
  // 2. Perform a crisp physical chip reset on the screen
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);  // Pull reset down
  delay(50);                     // Hold reset
  digitalWrite(OLED_RST, HIGH); // Release reset
  delay(50);                     // Let chip stabilize
 
  // ============================================================
  // HARDWARE OLED TEST & DIAGNOSTIC BLOCK
  // ============================================================
  Serial.println("[OLED TEST] Initializing hardware screen power...");
  
  display.init();
  display.flipScreenVertically();
  display.clear();
  
  display.drawRect(0, 0, 128, 64);
  display.setFont(ArialMT_Plain_10);
  display.drawString(10, 12, "OLED HARDWARE TEST");
  display.setFont(ArialMT_Plain_16);
  display.drawString(10, 28, "SCREEN IS OK!");
  display.display();
  
  Serial.println("[OLED TEST] Pattern pushed to screen. Holding for 5 seconds...");
  delay(5000); 
  // ============================================================

  display.clear();
  if (TEST_MODE) {
    display.drawString(0, 0, "SYSTEM IN TEST MODE");
    display.drawString(0, 16, "Generating fake data...");
    Serial.println("--- SENSING SYSTEM STARTED IN TEST MODE ---");
  } else {
    display.drawString(0, 0, "LoRa RX Initializing...");
    Serial.println("--- SENSING SYSTEM STARTED IN LIVE LORA MODE ---");
  }
  display.display();

  // Only initialize and boot up the radio hardware if we are NOT in test mode
  if (!TEST_MODE) {
    if (radio.begin() != RADIOLIB_ERR_NONE) {
      Serial.println("Radio initialization failed!");
      display.clear();
      display.drawString(0, 16, "Radio Error!");
      display.display();
      while (true);
    }
    radio.setFrequency(FREQUENCY);
    
    // Tell the radio pin handler to execute our callback function when data lands
    radio.setDio1Action(rxCallback); 
    
    // Start listening for the first packet indefinitely 
    if (radio.startReceive(RADIOLIB_SX126X_RX_TIMEOUT_INF) == RADIOLIB_ERR_NONE) {
      Serial.println("RX Station Listening...");
      display.clear();
      display.drawString(0, 0, "System Ready");
      display.drawString(0, 16, "Waiting for live data...");
      display.display();
    } else {
      Serial.println("Failed to start listening.");
    }
  }
}

void loop() {
  heltec_loop();

  // --- PATH A: TEST MODE PACKET SIMULATION ---
  if (TEST_MODE) {
    if (millis() - lastTestTrigger >= 3000) {
      lastTestTrigger = millis();

      float noise = ((float)random(-150, 150)) / 100.0;
      receivedPressure = mockBasePressure + noise;

      float fakeRSSI = (float)random(-65, -45); 
      float fakeSNR = (float)random(8, 12);

      updateOutput(receivedPressure, fakeRSSI, fakeSNR);
    }
  } 
  // --- PATH B: LIVE LORA PACKET CAPTURE ---
  else {
    // Only look at the radio data buffer IF the callback flag is tripped!
    if (rxFlag) {
      rxFlag = false; // Immediately clear the flag so we don't double-process

      // Read the raw over-the-air payload into our float variable
      int state = radio.readData((uint8_t*)&receivedPressure, sizeof(receivedPressure));
      
      if (state == RADIOLIB_ERR_NONE) {
        float currentRSSI = radio.getRSSI();
        float currentSNR = radio.getSNR();
        
        // Push the fresh, valid sensor reading to the display and serial pipeline
        updateOutput(receivedPressure, currentRSSI, currentSNR);
      } else {
        Serial.printf("Packet reception error code: %d\n", state);
      }
                    
      // Put the radio back to sleep-listening mode for the next packet
      radio.startReceive(RADIOLIB_SX126X_RX_TIMEOUT_INF); 
    }
  }
}

// Reusable function to handle formatting the text for your Mac and visual layout for the screen
void updateOutput(float pressure, float rssi, float snr) {
  // 1. Output string directly down the USB pipeline to your Mac Python file logger
  Serial.printf("%.2f,%.2f,%.2f\n", pressure, rssi, snr);
                
  // 2. Draw layout onto the integrated hardware OLED display
  display.clear();
  
  display.setFont(ArialMT_Plain_10);
  if (TEST_MODE) {
    display.drawString(0, 0, "[MOCK TEST TELEMETRY]");
  } else {
    display.drawString(0, 0, "--- LORA TELEMETRY ---");
  }
  
  // Main Data Readout Box
  display.setFont(ArialMT_Plain_16);
  char buf[32]; 
  snprintf(buf, sizeof(buf), "%.2f hPa", pressure);
  display.drawString(0, 20, buf);
  
  // Radio Performance Statistics Footnote
  display.setFont(ArialMT_Plain_10);
  snprintf(buf, sizeof(buf), "RSSI: %.1f dBm", rssi);
  display.drawString(0, 48, buf);
  
  display.display();
}

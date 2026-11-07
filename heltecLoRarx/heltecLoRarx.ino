#include <heltec_unofficial.h>

#define FREQUENCY 915.0 // Set your matching transmitter frequency

// --- TEST MODE CONFIGURATION ---
#define TEST_MODE true  // Set to 'true' to generate fake data without TX. Set to 'false' for live LoRa.
// -------------------------------

float receivedPressure = 0.0;
unsigned long lastTestTrigger = 0;
float mockBasePressure = 1013.25; // Standard sea-level pressure in hPa
// --- HELTEC HARDWARE POWER/RESET PINS ---
#define OLED_VEXT 36  // Controls power rail to the OLED display on V3/V3.2
#define OLED_RST  21  // Physical reset pin for the SSD1306 screen
// ----------------------------------------

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
  
  // Initialize the screen driver
  display.init();
  display.flipScreenVertically();
  display.clear();
  
  // Draw a test box around the entire perimeter of the screen (128x64)
  display.drawRect(0, 0, 128, 64);
  
  // Write a prominent test string to check for rendering capability
  display.setFont(ArialMT_Plain_10);
  display.drawString(10, 12, "OLED HARDWARE TEST");
  display.setFont(ArialMT_Plain_16);
  display.drawString(10, 28, "SCREEN IS OK!");
  
  // Push changes to the physical panel
  display.display();
  Serial.println("[OLED TEST] Pattern pushed to screen. Holding for 5 seconds...");
  
  // Pause execution so you can visually verify the panel lights up
  delay(5000); 
  // ============================================================

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
    
    if (radio.startReceive() == RADIOLIB_ERR_NONE) {
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
    // Generate simulated data every 3 seconds (3000ms) without blocking the processor
    if (millis() - lastTestTrigger >= 3000) {
      lastTestTrigger = millis();

      // Create a slight fluctuation in pressure (-1.5 to +1.5 hPa) to simulate real sensor noise
      float noise = ((float)random(-150, 150)) / 100.0;
      receivedPressure = mockBasePressure + noise;

      // Create fake hardware telemetry metrics
      float fakeRSSI = (float)random(-65, -45); // Good, stable simulated line-of-sight signal
      float fakeSNR = (float)random(8, 12);

      // Handle displaying and logging the simulated data
      updateOutput(receivedPressure, fakeRSSI, fakeSNR);
    }
  } 
  // --- PATH B: LIVE LORA PACKET CAPTURE ---
  else {
    if (radio.readData((uint8_t*)&receivedPressure, sizeof(receivedPressure)) == RADIOLIB_ERR_NONE) {
      
      // Pull actual signal metrics directly out of the live hardware register
      updateOutput(receivedPressure, radio.getRSSI(), radio.getSNR());
                    
      // Reset back into hardware receive mode for the next incoming packet
      radio.startReceive(); 
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
  char buf[32]; // <--- FIXED: Now safely holds up to 31 characters + null terminator
  snprintf(buf, sizeof(buf), "%.2f hPa", pressure);
  display.drawString(0, 20, buf);
  
  // Radio Performance Statistics Footnote
  display.setFont(ArialMT_Plain_10);
  // Reusing the fixed 32-byte buffer safely
  snprintf(buf, sizeof(buf), "RSSI: %.1f dBm", rssi);
  display.drawString(0, 48, buf);
  
  display.display();
}
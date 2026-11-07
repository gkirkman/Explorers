#include <heltec_unofficial.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h> 

#define FREQUENCY 915.0 // Match your receiver's frequency exactly

// --- TEST MODE CONFIGURATION ---
#define TEST_MODE true  // Change to 'false' once you wire up your physical BMP280!
// -------------------------------

// --- HELTEC HARDWARE POWER/RESET PINS ---
#define OLED_VEXT 36  // Controls power rail to the OLED display on V3/V3.2
#define OLED_RST  21  // Physical reset pin for the SSD1306 screen
// ----------------------------------------

Adafruit_BMP280 bmp; // I2C sensor object
float txPressure = 0.0;
unsigned long lastTxTrigger = 0;
float mockBasePressure = 1013.25; 

// Cleaned up function to pull sensor data
float readPressure() {
  float p = bmp.readPressure(); // Reads pressure in Pascals
  
  if (!isnan(p)) {
    p = p / 100.0F; // Convert Pascals to hPa
  } else {
    Serial.println("Error: Could not read pressure data. Defaulting to 0.0");
    p = 0.0;
  }
  return p;
}

void setup() {
  // 1. Force screen hardware power rails BEFORE running the library setup
  pinMode(OLED_VEXT, OUTPUT);
  digitalWrite(OLED_VEXT, LOW); // LOW turns ON the Vext power rail on Heltec V3 modules
  
  // 2. Perform a crisp physical chip reset on the screen
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);  
  delay(50);                     
  digitalWrite(OLED_RST, HIGH); 
  delay(50);                     

  // 3. Let heltec framework finish bootup
  heltec_setup();
  Serial.begin(115200);
  delay(500); 
  
  Serial.println("--- TX HARDWARE INITIALIZING ---");

  // ============================================================
  // HARDWARE OLED TEST & DIAGNOSTIC BLOCK
  // ============================================================
  display.init();
  display.flipScreenVertically();
  display.clear();
  
  display.drawRect(0, 0, 128, 64);
  display.setFont(ArialMT_Plain_10);
  display.drawString(10, 12, "OLED TX HARDWARE TEST");
  display.setFont(ArialMT_Plain_16);
  display.drawString(10, 28, "SCREEN IS OK!");
  display.display();
  delay(5000); 
  // ============================================================

  // Initialize Radio Hardware
  if (radio.begin() != RADIOLIB_ERR_NONE) {
    Serial.println("Radio initialization failed!");
    display.clear();
    display.drawString(0, 16, "Radio Error!");
    display.display();
    while (true);
  }
  radio.setFrequency(FREQUENCY);
  
  // --- BMP280 INITIALIZATION (ONLY IN LIVE MODE) ---
  if (!TEST_MODE) {
    Serial.println("Initializing BMP280 Sensor...");
    // 0x77 is the default Adafruit I2C address. If it fails, try 0x76 for generic sensors
    if (!bmp.begin(0x77)) { 
      Serial.println("Could not find a valid BMP280 sensor, check wiring!");
      display.clear();
      display.setFont(ArialMT_Plain_10);
      display.drawString(0, 0, "SENSOR ERROR!");
      display.drawString(0, 16, "BMP280 not found.");
      display.drawString(0, 32, "Check I2C Wiring!");
      display.display();
      while (true); // Freeze here so you don't transmit garbage data
    }
    Serial.println("BMP280 Sensor Detected Successfully.");
  }

  Serial.println("TX Setup Complete. Radio Ready...");
}

void loop() {
  heltec_loop();

  // Send packet and update display every 3 seconds (3000ms) without blocking
  if (millis() - lastTxTrigger >= 3000) {
    lastTxTrigger = millis();

    // --- STEP 1: GATHER DATA ---
    if (TEST_MODE) {
      // Generate mock sensor noise (-1.5 to +1.5 hPa)
      float noise = ((float)random(-150, 150)) / 100.0;
      txPressure = mockBasePressure + noise;
      Serial.print("[TEST MODE] ");
    } else {
      // Live Mode reading from your Adafruit helper function
      txPressure = readPressure(); 
    }

    // --- STEP 2: BROADCAST OVER LORA ---
    Serial.printf("Broadcasting Pressure: %.2f hPa... ", txPressure);
    
    // Transmit the raw float across the airwaves
    int state = radio.transmit((uint8_t*)&txPressure, sizeof(txPressure));
    
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println("Success!");
    } else {
      Serial.printf("Failed, code %d\n", state);
    }

    // --- STEP 3: UPDATE LOCAL OLED DISPLAY ---
    updateDisplay(txPressure, state == RADIOLIB_ERR_NONE);
  }
}

// Reusable function to draw TX status to the screen
void updateDisplay(float pressure, bool txSuccess) {
  display.clear();
  
  // Header Status bar
  display.setFont(ArialMT_Plain_10);
  if (TEST_MODE) {
    display.drawString(0, 0, "[TX MOCK DATA ACTIVE]");
  } else {
    display.drawString(0, 0, "--- LORA TRANSMITTER ---");
  }
  
  // Main Telemetry Display 
  display.setFont(ArialMT_Plain_16);
  char buf[32]; 
  snprintf(buf, sizeof(buf), "%.2f hPa", pressure);
  display.drawString(0, 20, buf);
  
  // Bottom Transmission Telemetry Row
  display.setFont(ArialMT_Plain_10);
  if (txSuccess) {
    display.drawString(0, 48, "Packet Sent: OK");
  } else {
    display.drawString(0, 48, "Packet Sent: FAILED");
  }
  
  display.display();
}

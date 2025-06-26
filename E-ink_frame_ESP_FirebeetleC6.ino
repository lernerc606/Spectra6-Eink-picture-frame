#include "EPD_13in3e.h"

// Define the Debug macro if not already defined
#ifndef Debug
  #define Debug(msg) Serial.print(msg)
#endif

void printResetReason() {
  esp_reset_reason_t reason = esp_reset_reason();
  Serial.print("Reset reason: ");
  
  switch (reason) {
    case ESP_RST_POWERON:    Serial.println("Power-on Reset"); break;
    case ESP_RST_EXT:        Serial.println("External Reset (Reset Pin)"); break;
    case ESP_RST_SW:         Serial.println("Software Reset (ESP.restart() or exception)"); break;
    case ESP_RST_PANIC:      Serial.println("Panic Reset (Guru Meditation Error)"); break;
    case ESP_RST_INT_WDT:    Serial.println("Interrupt Watchdog Timer Reset"); break;
    case ESP_RST_TASK_WDT:   Serial.println("Task Watchdog Timer Reset"); break;
    case ESP_RST_WDT:        Serial.println("Other Watchdog Timer Reset"); break;
    case ESP_RST_DEEPSLEEP:  Serial.println("Wake from Deep Sleep"); break;
    case ESP_RST_BROWNOUT:   Serial.println("Brownout Reset (Power failure)"); break;
    case ESP_RST_SDIO:       Serial.println("Reset Over SDIO"); break;
    default:                 Serial.println("Unknown Reset Reason"); break;
  }
}


void setup() {

  Serial.begin(115200);  // Set baud rate
  delay(1000);  // Give time for Serial Monitor to connect
  Serial.println("🔥FireBeetle ESP32-C6 is running!🔥");

  Serial.println("\nESP32 Reset Diagnostic:");
  printResetReason();

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  pinMode(22, OUTPUT);
  pinMode(23, OUTPUT);
  pinMode(1, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(18, OUTPUT);
  pinMode(19, OUTPUT);
  pinMode(20, OUTPUT);
  pinMode(14, OUTPUT);
  pinMode(2, OUTPUT);
  pinMode(8, OUTPUT);
  pinMode(21, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(6, OUTPUT);

  
  DEV_Module_Init();
  Debug("e-Paper Init...\r\n");
  EPD_13IN3E_Init();
  Debug("init done\r\n");
 
  EPD_13IN3E_demo();
  //EPD_13IN3E_Clear(0);
  //EPD_13IN3E_Show6Block();

  // Put the e-Paper display into sleep mode
  Debug("Goto Sleep...\r\n");
  EPD_13IN3E_Sleep();
  Debug("Close 5V, Module enters 0 power consumption ...\r\n");
  DEV_Module_Exit();

}

void loop() {

}



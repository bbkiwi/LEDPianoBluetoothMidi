/**
   Combined MidiBLe_Client and Adafruit_SSD1306 demo
   Reports key presses from Bluetooth Midi keyboard
*/

#include <Arduino.h>
#include <BLEMIDI_Transport.h>

#include <hardware/BLEMIDI_Client_ESP32.h>

//#include <hardware/BLEMIDI_ESP32_NimBLE.h>
//#include <hardware/BLEMIDI_ESP32.h>
//#include <hardware/BLEMIDI_nRF52.h>
//#include <hardware/BLEMIDI_ArduinoBLE.h>

#include <Wire.h>               // Only needed for Arduino 1.6.5 and earlier
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Initialize the OLED display using Arduino Wire:
// For TTGO Wifi + Bluetooth Battery OLED (see http://www.areresearch.net/2018/01/how-to-use-ttgo-esp32-module-with-oled.html)
#define SDA 5
#define SCL 4

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


BLEMIDI_CREATE_DEFAULT_INSTANCE(); //Connect to first server found

//BLEMIDI_CREATE_INSTANCE("",MIDI)                  //Connect to the first server found
//BLEMIDI_CREATE_INSTANCE("f2:c1:d9:36:e7:6b",MIDI) //Connect to a specific BLE address server
//BLEMIDI_CREATE_INSTANCE("MyBLEserver",MIDI)       //Connect to a specific name server

#ifndef LED_BUILTIN
#define LED_BUILTIN 2 //modify for match with yout board
#endif

void ReadCB(void *parameter);       //Continuos Read function (See FreeRTOS multitasks)

unsigned long t0 = millis();
uint16_t firstline;
bool isConnected = false;
char buf[400];
uint8_t line_to_write = 56;
uint8_t shift_of_display = 0;
/**
   -----------------------------------------------------------------------------
   When BLE is connected, LED will turn on (indicating that connection was successful)
   When receiving a NoteOn, LED will go out, on NoteOff, light comes back on.
   This is an easy and conveniant way to show that the connection is alive and working.
   -----------------------------------------------------------------------------
*/
void setup()
{
  Serial.begin(115200);
  
//https://github.com/espressif/arduino-esp32/issues/3779
#ifdef ARDUINO_ARCH_ESP32
  Wire.setPins(5, 4);
#endif

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(2000); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();
  display.setTextSize(1);             // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0, line_to_write);
  display.ssd1306_command(0x40 + shift_of_display);
  display.println(F("MIDI keys!"));
  display.display();

  MIDI.begin(MIDI_CHANNEL_OMNI);

  BLEMIDI.setHandleConnected([]()
  {
    Serial.println("---------CONNECTED---------");
    isConnected = true;
    digitalWrite(LED_BUILTIN, HIGH);
  });

  BLEMIDI.setHandleDisconnected([]()
  {
    Serial.println("---------NOT CONNECTED---------");
    isConnected = false;
    digitalWrite(LED_BUILTIN, LOW);
  });

  MIDI.setHandleNoteOn([](byte channel, byte note, byte velocity)
  {
    digitalWrite(LED_BUILTIN, LOW);
    Serial.printf("ON %d CH %d, note:%d, vel:%d\n", millis() - t0, channel, note, velocity);
    Serial.printf("  cursor at %d, %d\n", display.getCursorX(), display.getCursorY());
    //display.print("ON %d CH %d, note:%d, vel:%d\n", millis() - t0,  channel, note, velocity);
    line_to_write += 8;
    line_to_write %= 64;
    shift_of_display = (8 + line_to_write) % 64;
    display.ssd1306_command(0x40 + shift_of_display); //40h ... 7Fh
    display.writeFillRect(0, line_to_write, 128, 8, 0); //clear line first
    display.setCursor(0, line_to_write);
    display.print("D-");
    display.print(millis() - t0);
    display.print("-");
    display.print(note);
    display.print("(");
    display.print(velocity);
    display.println(")");
    display.display();
  });
  MIDI.setHandleNoteOff([](byte channel, byte note, byte velocity)
  {
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.printf("OFF %d CH %d, note:%d, vel:%d\n", millis() - t0,  channel, note, velocity);
    Serial.printf("  cursor at %d, %d\n", display.getCursorX(), display.getCursorY());
    line_to_write += 8;
    line_to_write %= 64;
    shift_of_display = (8 + line_to_write) % 64;
    display.ssd1306_command(0x40 + shift_of_display);
    display.writeFillRect(0, line_to_write, 128, 8, 0);  //clear line first
    display.setCursor(0, line_to_write);
    display.print("U-");
    display.print(millis() - t0);
    display.print("-");
    display.print(note);
    display.print("(");
    display.print(velocity);
    display.println(")");
    display.display();
  });

  xTaskCreatePinnedToCore(ReadCB,           //See FreeRTOS for more multitask info
                          "MIDI-READ",
                          3000,
                          NULL,
                          1,
                          NULL,
                          1); //Core0 or Core1

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  t0 = millis();
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void loop()
{
  //MIDI.read();  // This function is called in the other task

  //  if (isConnected && (millis() - t0) > 1000)
  //  {
  //    t0 = millis();
  //
  //    MIDI.sendNoteOn(60, 100, 1); // note 60, velocity 100 on channel 1
  //    vTaskDelay(250 / portTICK_PERIOD_MS);
  //    MIDI.sendNoteOff(60, 0, 1);
  //  }
}

/**
   This function is called by xTaskCreatePinnedToCore() to perform a multitask execution.
   In this task, read() is called every millisecond (approx.).
   read() function performs connection, reconnection and scan-BLE functions.
   Call read() method repeatedly to perform a successfull connection with the server
   in case connection is lost.
*/
void ReadCB(void *parameter)
{
  //  Serial.print("READ Task is started on core: ");
  //  Serial.println(xPortGetCoreID());
  for (;;)
  {
    MIDI.read();
    vTaskDelay(1 / portTICK_PERIOD_MS); //Feed the watchdog of FreeRTOS.
    //Serial.println(uxTaskGetStackHighWaterMark(NULL)); //Only for debug. You can see the watermark of the free resources assigned by the xTaskCreatePinnedToCore() function.
  }
  vTaskDelay(1);
}

#include <Arduino.h>
#include <Thermal_Printer.h>
#include <JPEGDEC.h>
#include <U8g2lib.h>
#include "OV2640.h"
#include "U8x8lib.h"
#include <esp_camera.h>

U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(/* clock=*/22, /* data=*/21);

#define PIR_PIN 33
#define SHUTTER_PIN 22

OV2640 cam;
JPEGDEC jpg;

uint8_t ucDither[576 * 16]; // buffer for the dithered pixel output

// Called when the decoder has a batch of pixels to print
int printRows(JPEGDRAW *pDraw)
{
  int i, iCount;
  uint8_t *s = (uint8_t *)pDraw->pPixels;

  tpSetBackBuffer((uint8_t *)pDraw->pPixels, pDraw->iWidth, pDraw->iHeight);
  // The output is inverted, so flip the bits
  iCount = (pDraw->iWidth * pDraw->iHeight) / 8;
  for (i = 0; i < iCount; i++){
    s[i] = ~s[i];
  }

  tpPrintBuffer(); // Print this block of pixels
  return 1;        // Continue decode
}

bool last_shutter_input;

void setup_shutter()
{
  Serial.println("Setting up shutter");
  pinMode(SHUTTER_PIN, INPUT_PULLUP);
  last_shutter_input = digitalRead(SHUTTER_PIN);
}

#define PIR_PIN 33

void setup_pir()
{
  Serial.println("Setting up PIR");
  pinMode(PIR_PIN, INPUT);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIR_PIN, HIGH);
}

void setup_display()
{
  Serial.println("Setting up display");
  u8x8.setPowerSave(false);
  u8x8.begin();
  u8x8.setFont(u8x8_font_profont29_2x3_r);
  u8x8.setFlipMode(1);
}

bool try_setup_camera()
{
  Serial.println("Setting up camera:");

  esp_err_t res = cam.init(esp32cam_ttgo_t_config);

  if (res == ESP_OK)
  {
    Serial.println("   camera OK");
    return true;
  }
  else
  {
    Serial.println("   camera setup failed");
    return false;
  }
}

bool try_setup_bluetooth_printer()
{
  Serial.println("Setting up printer:");
  if (tpScan())
  {
    if (tpConnect())
    {
      Serial.println("   printer found");
      tpSetWriteMode(MODE_WITHOUT_RESPONSE);
      Serial.println("   printer connected");
      return true;
    }
    else
    {
      Serial.println("   no printer");
      return false;
    }
  }
  else
  {
    Serial.println("   failed");
    return false;
  }
}

bool try_setup(bool (*try_setup_function)(),
               int retry_gap_millis,
               int no_of_retries)
{
  while ((no_of_retries--) > 0)
  {
    if (try_setup_function())
      return true;
    delay(retry_gap_millis);
    Serial.print("    retry");
  }
  Serial.print("    failed");
  return false;
}

uint8_t *frame_buffer = NULL;
int frame_buffer_size;

void take_picture()
{
  Serial.println("   taking a picture");
  frame_buffer = (uint8_t *)cam.getfb();
  frame_buffer_size = cam.getSize();
}

void print_picture()
{
  Serial.println("   printing the picture");
  
  if (jpg.openRAM(frame_buffer, frame_buffer_size, printRows))
  {
    Serial.println("   picture decoded");
    jpg.setPixelType(ONE_BIT_DITHERED);
    jpg.decodeDither(ucDither, JPEG_SCALE_HALF);
  }
}

unsigned long start_time;
#define TIMEOUT_MILLISECS 30000

void show_hello()
{
  u8x8.setCursor(3, 2);
  u8x8.print("HELLO");
}

void show_camera_ok()
{
  u8x8.clear();
  u8x8.setCursor(3, 0);
  u8x8.print("CAMERA");
  u8x8.setCursor(6, 4);
  u8x8.print("OK");
}

void show_printer_ok()
{
  u8x8.clear();
  u8x8.setCursor(1, 0);
  u8x8.print("PRINTER");
  u8x8.setCursor(6, 4);
  u8x8.print("OK");
}

void show_smile(int count)
{
  char buffer[20];
  snprintf(buffer, 20, "%d", count);
  u8x8.clear();
  u8x8.setCursor(6, 0);
  u8x8.print(buffer);
  u8x8.setCursor(4, 4);
  u8x8.print("SMILE");
}

void show_click()
{
  u8x8.clear();
  u8x8.setCursor(0, 2);
  u8x8.print("*CLICK*");
}

void show_goodbye()
{
  u8x8.clear();
  u8x8.setCursor(0, 2);
  u8x8.print("GOODBYE");
}

void show_print()
{
  u8x8.clear();
  u8x8.setCursor(3, 2);
  u8x8.print("PRINT");
}

void show_press_button()
{
  u8x8.clear();
  u8x8.setCursor(3, 0);
  u8x8.print("PRESS");
  u8x8.setCursor(2, 4);
  u8x8.print("BUTTON");
}

void take_picture_and_print()
{
  Serial.println("Taking a picture and printing:");
  Serial.println("   countdown");

  delay(200);

  for (int i = 5; i > 0; i--)
  {
    show_smile(i);
    delay(1000);
    cam.run();
    frame_buffer = (uint8_t *)cam.getfb();
  }

  show_click();
  take_picture();
  show_print();
  print_picture();
  show_press_button();
}

void go_to_sleep()
{
  Serial.println("Device Sleeping...");
  show_goodbye();
  delay(1000);
  // turn off the screen
  u8x8.setPowerSave(true);
  // disconnect the printer
  tpDisconnect();
  // turn off bluetooth
  btStop();
  // enter deep sleep
  esp_deep_sleep_start();
}

void update_shutter_button()
{
  bool new_shutter = digitalRead(SHUTTER_PIN);
  if (last_shutter_input != new_shutter) {
    if (new_shutter == 0) {
      take_picture_and_print();
    }
    last_shutter_input = new_shutter;
  }
}

void update_timeout()
{
  unsigned long timer_millis = millis();
  if (digitalRead(PIR_PIN)) {
    // reset the timer if people are still in front of
    // the camera
    start_time = timer_millis;
  }

  if ((timer_millis - start_time) > TIMEOUT_MILLISECS) {
    // turn off the camera
    go_to_sleep();
  }
}

void setup()
{
  Serial.begin(115200);
  setup_shutter();
  setup_display();
  setup_pir();

  if (!digitalRead(PIR_PIN)) {
    // shut down if nobody is in front of the camera
    go_to_sleep();
  }

  show_hello();

  if (!try_setup(try_setup_camera, 200, 5))
  {
    Serial.println("*** CAMERA START FAILED");
    esp_restart();
  }

  show_camera_ok();

  if (!try_setup(try_setup_bluetooth_printer, 200, 5))
  {
    Serial.println("*** PRINTER START FAILED");
    esp_restart();
  };

  show_printer_ok();

  delay(200);

  show_press_button();
}

void loop()
{
  update_shutter_button();
  update_timeout();
  delay(10);
}
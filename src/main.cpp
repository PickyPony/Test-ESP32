#include <Arduino.h>
#include "SH1106Spi.h"
#include "ADS1X15.h"

// ****************************** WIRERING ****************************************************
// ** Displays **
#define DISPLAY_RST_PIN 36 // Reset the displays manually and so we need only one pin for all dispalys and not one for each. set the reset pin in the display init to -1
#define DISPLAY_0_DC_PIN 21
#define DISPLAY_0_CS_PIN 3
#define DISPLAY_1_DC_PIN 37
#define DISPLAY_1_CS_PIN 38
#define DISPLAY_2_DC_PIN 39
#define DISPLAY_2_CS_PIN 40

#define SENS_v_PIN 2
#define SENS_INDENT_C1_PIN 1

// ** ADC IC **
#define ADC_ID_ADDR 0x48

// ****************************** Init ****************************************************
// ** Displays **
SH1106Spi d1(-1, DISPLAY_0_DC_PIN, DISPLAY_0_CS_PIN); // RES, DC, CS
SH1106Spi d2(-1, DISPLAY_1_DC_PIN, DISPLAY_1_CS_PIN); // RES, DC, CS
SH1106Spi d3(-1, DISPLAY_2_DC_PIN, DISPLAY_2_CS_PIN); // RES, DC, CS

SH1106Spi *displays[] = {&d1, &d2, &d3};
#define display_count 3

// ** ADC IC **
ADS1115 ADS(0x48);
float adc_values_v[display_count];

float sense_usb_voltage = -1;
float sense_usb_current = -1;
float sense_ch1_indent = -1;
// ****************************** Setup routines ****************************************************
// ** Displays **
float display_vals[display_count];
float display_usb_voltage = -1;
float display_usb_current = -1;
float display_ch1_indent = -1;
void setupDisplay()
{
  // Reset Displays
  // Pulse Reset low for 10ms
  pinMode(DISPLAY_RST_PIN, OUTPUT);

  digitalWrite(DISPLAY_RST_PIN, HIGH);
  delay(1);
  digitalWrite(DISPLAY_RST_PIN, LOW);
  delay(10);
  digitalWrite(DISPLAY_RST_PIN, HIGH);

  // init Displays
  for (int i = 0; i < display_count; i++)
  {
    displays[i]->init();
    displays[i]->flipScreenVertically();
    displays[i]->setFont(ArialMT_Plain_24);
    display_vals[i] = -1;
  }
}
// ** ADC IC **
float adc_f = 0;
int adc_request_num = 0;
void setupAdcIc()
{
  ADS.begin();
  ADS.setGain(0);          // 6.144 volt
  ADS.setDataRate(4);      // 128 Samples Per Seconde. Wie ich das verstehe sampeled er trotzdem 860 pro sekunde aber nimmt dann den durchschnit. Siehe Datasheet 9.4.3
  adc_f = ADS.toVoltage(); // voltage factor
  adc_request_num = 0;
  ADS.requestADC(adc_request_num);
}

// ****************************** LoopRoutines ******************************

// ** ADC IC **
long adc_last_full_read_time = 0;
long adc_full_read_time = -1;
void loopAdcIC()
{
  if (ADS.isBusy() == false)
  {
    adc_values_v[adc_request_num] = ADS.getValue() * adc_f;
    if (adc_request_num == display_count - 1)
    {
      adc_request_num = 0;
      // calculate how long it takes to read all ports
      adc_full_read_time = millis() - adc_last_full_read_time;
      adc_last_full_read_time = millis();
    }
    else
    {
      adc_request_num++;
    }
    ADS.requestADC(adc_request_num); // request a new one
  }
}

// ** Displays **

// display power state
bool hasPowerStatsChanged(){
  if (abs(sense_usb_current-display_usb_current) > 0.01 || abs(sense_usb_voltage-display_usb_voltage) > 0.2 || 
    abs(sense_ch1_indent-display_ch1_indent) > 0.02){
    display_usb_current = sense_usb_current;
    display_usb_voltage = sense_usb_voltage;
    display_ch1_indent = sense_ch1_indent;
    return true;
  }
  return false;
}
void loopDisplay()
{
  // only sent update to display if values has changed:
  for (int i = 0; i < display_count; i++)
  {
    if (abs(display_vals[i] - adc_values_v[i]) > 0.002 || (i== 0 && hasPowerStatsChanged()))
    {
      display_vals[i] = adc_values_v[i];
      SH1106Spi *display = displays[i];
      display->clear();
      display->drawStringMaxWidth(0, 0, 128, String(display_vals[i], 3));
      display->drawStringMaxWidth(70, 0, 128, "V");

      //display stats on d1:
      if (i==0){
       displays[i]->setFont(ArialMT_Plain_10);
       display->drawStringMaxWidth(0, 22, 128, String(display_usb_voltage, 1)+"V");
       display->drawStringMaxWidth(0, 32, 128, String(display_usb_current, 2)+"A");
       display->drawStringMaxWidth(0, 42, 128, String(100/(3.3/display_ch1_indent-1), 2)+"kOhm");
       displays[i]->setFont(ArialMT_Plain_24);

      }

      display->display();
    }
  }
}

// ** Sensing **

long sense_loop_last = 0;
void senseLoop(){
  if (millis()-sense_loop_last > 100){
    sense_usb_voltage = 7/8.0 * sense_usb_voltage + 1/8.0 * analogRead(SENS_v_PIN)/4095.0*2.65*11.0;
    sense_ch1_indent = 9/10.0 * sense_ch1_indent + 1/10.0 * analogRead(SENS_INDENT_C1_PIN)/4095.0*2.65;
    sense_loop_last = millis();
  }
}

// ** Helper **
int loop_counter = 0;
long loop_last_time = 0;
void loopsPerSecLoop()
{
  loop_counter++;
  long delta_t = millis() - loop_last_time;
  if (delta_t >= 5000)
  {
    long loops_per_sec = loop_counter / (delta_t / 1000.0);
    Serial.print("Loops/sec: ");
    Serial.println(loops_per_sec);
    loop_counter = 0;
    loop_last_time = millis();
  }
}

// ****************** HELPER **************************

void printVoltage()
{
  Serial.print("Volt: ");
  for (int i = 0; i < display_count; i++)
  {
    Serial.print(adc_values_v[i], 3);
    Serial.print("V ");
  }
  Serial.print("ADC read:");
  Serial.print(adc_full_read_time);
  Serial.print(" ms");
  Serial.print("CH1: ");
  Serial.print(sense_usb_voltage);
  Serial.println("v");
}

// ****************** SETUP ***************************
void setup()
{
  Serial.begin(115200);
  analogReadResolution(12);
  setupDisplay();
  setupAdcIc();
}

float reading = 19.0;
long last_time = 0;

// ****************** LOOP ***************************
void loop()
{
  // do not block the fucking loop !!!
  loopsPerSecLoop();
  loopAdcIC();
  senseLoop();
  loopDisplay();

  long now = millis();
  if (now - last_time > 1000)
  {
    last_time = now;
    printVoltage();
  }
}
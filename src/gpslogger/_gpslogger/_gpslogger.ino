//************************************************
//  FILE    :_gpslogger.ino
//  DATE    :2026/10/23
//  DESCRIPTION :gps data logger
//  BOARD TYPE  :XIAO ESP32S3
//  AUTHER      :inteGN
//************************************************

/*
NOTE 1:
External UART input is uploaded as-is. Filter sensitive data if necessary.

NOTE 2:
Location Privacy (GPS Coordinate Handling) is here.
  The firmware supports optional location obfuscation to prevent accidental
  disclosure of real GPS coordinates when publishing logs or sharing code.
 
  Configuration:
    #define HIDE_LOCATION 1   // 1 = use fixed dummy coordinates
                              // 0 = use actual GPS readings

  When enabled, the firmware replaces latitude/longitude values with a
  predefined static coordinate pair. This prevents unintended exposure of
  sensitive location data when logs are uploaded to Firebase or shared
  publicly (e.g., GitHub, cloud dashboards).

NOTE 3:
PS_UV_SD, PS_UV_WIFI, PS_UV_HYSTERESIS and IIR_CONST are experimental configuration values.
Battery voltage characteristics fluctuate dynamically depending on execution loads, 
operating currents, and individual cell capacities. 
These values are provided solely as a reference from specific test observations 
and must be thoroughly reviewed and customized for your own operating environment.

NOTE 4:
LOOP_MONITOR_ENABLE, IDLE_MONITOR_ENABLE and TASK_WATCH_ENABLE are experimental features.
Enabling them change the normal loop timing and have not been verified for all operating conditions.
Their values are only a reference and should not be interpreted as CPU usage by itself.
*/


//// Includes
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_heap_caps.h"
#include "esp_freertos_hooks.h"
#include <esp_task_wdt.h>
#include <Wire.h>
#include <Dps3xx.h>
#define   EXT_CHAR_LENGTH       240       //set maximun Ext length, it should be less than LogPayload size
#define   EXT_SYNC_WORD         "EL"      //timer1sec synchronization trigger by Ext header, "" voids this function
#include "ramdata.h"


//// Configuration of this mcu
#define   MCU01     //or MCU02, MCU03 or MCU04


//// Location privacy configuration
#define   HIDE_LOCATION         1         //1=dummy location, 0=real location


//// Queue size configuration
#define   SD_QUEUE_SIZE         1024
#define   WIFI_QUEUE_SIZE       2048


//// Definitions
typedef enum {
    WIFI_CMD_CONNECT,
    WIFI_CMD_DISCONNECT
} WifiCommand_t;

#define   RX_EXT                D0        //XIAO <-- EXT
#define   VB_AD                 D2        //XIAO <-- VBAT
#define   SD_CS                 D3        //XIAO --> SD
#define   TX_GPS                D6        //XIAO --> GPS
#define   RX_GPS                D7        //XIAO <-- GPS
#define   TP_PIN                D1        //XIAO test pin
#define   BOOT_SW               0         //XIAO GPIO 0
#define   BOOT_SW_ON            true
#define   BOOT_SW_OFF           false
#define   TP_SET_HIGH           digitalWrite(TP_PIN, HIGH)
#define   TP_SET_LOW            digitalWrite(TP_PIN, LOW)
#define   EXT_BITRATE           19200

#if defined(MCU01)
  #define   DIV_VBAT            1.489f    // typical value 1.5f (10 kohm - 20 kohm), or appropliate value
#elif defined(MCU02)
  #define   DIV_VBAT            1.493f    // typical value 1.5f (10 kohm - 20 kohm), or appropliate value
#elif defined(MCU03)
  #define   DIV_VBAT            1.5f      // typical value 1.5f (10 kohm - 20 kohm), or appropliate value
#elif defined(MCU04)
  #define   DIV_VBAT            1.5f      // typical value 1.5f (10 kohm - 20 kohm), or appropliate value
#endif

#define   DPS_SAMPLE_TEMP       4         //DPS310 temperature oversamples, 27.6 ms (standard)
#define   DPS_SAMPLE_PRESS      7         //DPS310 pressure oversamples, 206.8 ms (best precision)
#define   DEV_THIS              0x14      //uart id (0x11 - 0x14) of this device
#define   IIR_CONST             8.0f      //experimental, should need to review
#define   PS_UV_SD              3.65f     //experimental, should need to review
#define   PS_UV_WIFI            3.60f     //experimental, should need to review
#define   PS_UV_HYSTERESIS      0.1f      //experimental, should need to review
#define   IDLEWATCH_CORE0       1         //1=enable, 0=disable
#define   IDLEWATCH_CORE1       0         //1=enable, 0=disable
#define   TIMEOUT_CORE1         3000      //core1 watchdog timeout, msec

#define   TP_ENABLE_1           0         //1=enable, 0=disable  respond while queue sending
#define   TP_ENABLE_2           0         //1=enable, 0=disable  respond at i2c communication from/to dps310
#define   TP_ENABLE_3           0         //1=enable, 0=disable  respond while sd flush and json push 
#define   STACK_WATCH_ENABLE    0         //1=enable, 0=disable
#define   LOOP_MONITOR_ENABLE   0         //1=enable(EXPERIMENTAL), 0=disable
#define   IDLE_MONITOR_ENABLE   0         //1=enable(EXPERIMENTAL), 0=disable
#define   TASK_WATCH_ENABLE     0         //1=enable(EXPERIMENTAL), 0=disable

//Quick-and-dirty implementation: Excessive serial logging may cause crashes or malfunctions.
#define   DEBUG_EV_ENABLE       0         //1=enable, 0=disable  generate_env() execution time watch
#define   DEBUG_C1_ENABLE       0         //1=enable, 0=disable  not used
#define   DEBUG_QU_ENABLE       0         //1=enable, 0=disable  dispatched queue watch
#define   DEBUG_SD_ENABLE       0         //1=enable, 0=disable
#define   DEBUG_WF_ENABLE       0         //1=enable, 0=disable
#define   DEBUG_FB_ENABLE       0         //1=enable, 0=disable

#if DEBUG_C1_ENABLE
  #define DBG_C1_PRINT(x)       Serial.print(x)
  #define DBG_C1_PRINTLN(x)     Serial.println(x)
  #define DBG_C1_PRINTF(...)    Serial.printf(__VA_ARGS__)
#else
  #define DBG_C1_PRINT(x)
  #define DBG_C1_PRINTLN(x)
  #define DBG_C1_PRINTF(...)
#endif
#if DEBUG_QU_ENABLE
  #define DBG_QU_PRINT(x)       Serial.print(x)
  #define DBG_QU_PRINTLN(x)     Serial.println(x)
  #define DBG_QU_PRINTF(...)    Serial.printf(__VA_ARGS__)
#else
  #define DBG_QU_PRINT(x)
  #define DBG_QU_PRINTLN(x)
  #define DBG_QU_PRINTF(...)
#endif
#if DEBUG_SD_ENABLE
  #define DBG_SD_PRINT(x)       Serial.print(x)
  #define DBG_SD_PRINTLN(x)     Serial.println(x)
  #define DBG_SD_PRINTF(...)    Serial.printf(__VA_ARGS__)
#else
  #define DBG_SD_PRINT(x)
  #define DBG_SD_PRINTLN(x)
  #define DBG_SD_PRINTF(...)
#endif
#if DEBUG_WF_ENABLE
  #define DBG_WF_PRINT(x)       Serial.print(x)
  #define DBG_WF_PRINTLN(x)     Serial.println(x)
  #define DBG_WF_PRINTF(...)    Serial.printf(__VA_ARGS__)
#else
  #define DBG_WF_PRINT(x)
  #define DBG_WF_PRINTLN(x)
  #define DBG_WF_PRINTF(...)
#endif
#if DEBUG_FB_ENABLE
  #define DBG_FB_PRINT(x)       Serial.print(x)
  #define DBG_FB_PRINTLN(x)     Serial.println(x)
  #define DBG_FB_PRINTF(...)    Serial.printf(__VA_ARGS__)
#else
  #define DBG_FB_PRINT(x)
  #define DBG_FB_PRINTLN(x)
  #define DBG_FB_PRINTF(...)
#endif


//// Objects
QueueHandle_t       sdQueue;
QueueHandle_t       wifiQueue;
QueueHandle_t       wifiCmdQueue;
Dps3xx              dps310sensor = Dps3xx();
HardwareSerial      gpsSerial(1);
HardwareSerial      extSerial(2);
hw_timer_t          *myTimer = NULL;


////Queue handling functions
//waste oldest at full queue
void push_to_queue_overwrite(QueueHandle_t queue, const LogPayload* payload, const char* name) {
  if (xQueueSend(queue, payload, 0) != pdTRUE) {
    LogPayload dummy;
    xQueueReceive(queue, &dummy, 0);        //drop oldest queue
    xQueueSend(queue, payload, 0);          //retry newest queuing
    DBG_QU_PRINTF("[Core 1 Warning] %s Queue Full -> Dropped OLDEST: %s\n", name, dummy.raw_line);
  }
}

//dispatch sentence
void dispatch_log_sentence(const char* sentence_str) {
#if TP_ENABLE_1
  TP_SET_HIGH;
#endif
  LogPayload payload;
  strncpy(payload.raw_line, sentence_str, sizeof(payload.raw_line) - 1);
  payload.raw_line[sizeof(payload.raw_line) - 1] = '\0';
  DBG_QU_PRINTF("[Core 1 Dispatch] %s\n", payload.raw_line);
  if (psOkSd && sdAvailable) {    
    push_to_queue_overwrite(sdQueue, &payload, "SD");
  }
  if (psOkWifi && wifiAvailable) {    
    push_to_queue_overwrite(wifiQueue, &payload, "WiFi");
  }
#if TP_ENABLE_1
  TP_SET_LOW;
#endif
}


//// Timer execution
void  IRAM_ATTR onTimer() {
  //synchronize to exact second
  if (countNoGps < 3000) {
    if (flagGpsSync && (timer1sec > 40) && (timer1sec < 960)) {timer1sec = 999;}
    flagGpsSync = false;
    flagExtSync = false;
  }
  else if (countNoExt < 3000) {
    if (flagExtSync && (timer1sec > 40) && (timer1sec < 960)) {timer1sec = 999;}
    flagGpsSync = false;
    flagExtSync = false;
  }
  countNoGps = countNoGps + 2;
  if (countNoGps > 10000) {countNoGps = 10000;}  
  countNoExt = countNoExt + 2;
  if (countNoExt > 10000) {countNoExt = 10000;}

//1 sec count and request
  timer1sec = timer1sec + 2;
  if (timer1sec > 999) {
    timer1sec = 0;
    flagDpsReq  = 1;
    flagVbatReq = 1;
    flagWatchReq = true;
  }
  flagAdcReq = true;
  flagSwReq =  true;

//2 sec count
  timer2sec = timer2sec + 2;
  if (timer2sec > 1999) {timer2sec = 0;}
  if ((timer2sec & 0x0001) == 0) {flagLedReq = true;}     //every 4 msec
}


//// Setup
void setup() {
//idle hook setup
#if IDLE_MONITOR_ENABLE
  esp_register_freertos_idle_hook_for_cpu(idleHookCore0, 0);
  esp_register_freertos_idle_hook_for_cpu(idleHookCore1, 1);
#endif

//port setup
  analogSetPinAttenuation(VB_AD, ADC_11db);
  vbatLoggerMod = (float)analogReadMilliVolts(VB_AD) / 1000.0f * DIV_VBAT;
  pinMode(BOOT_SW, INPUT_PULLUP);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  pinMode(TP_PIN, OUTPUT);
  TP_SET_LOW;
  analogWriteFrequency(LED_BUILTIN, 2000);  
  analogWrite(LED_BUILTIN, 0);       //active low
  Serial.begin(115200);
  Serial.setTxTimeoutMs(5);
  Wire.begin();
  Wire.setClock(400000);  
  Wire.setTimeout(10);
  dps310sensor.begin(Wire);
  dps310sensor.startMeasureTempOnce(DPS_SAMPLE_TEMP);
  delay(100);
  dps310sensor.getSingleResult(tempDps);
  delay(100);
  dps310sensor.startMeasurePressureOnce(DPS_SAMPLE_PRESS);
  init_gps();
  extSerial.begin(EXT_BITRATE, SERIAL_8N1, RX_EXT, -1);

//queue and task setup, PSRAM option of IDE should set to use 
  sdQueue   = xQueueCreateWithCaps(SD_QUEUE_SIZE,   sizeof(LogPayload), MALLOC_CAP_SPIRAM);
  wifiQueue = xQueueCreateWithCaps(WIFI_QUEUE_SIZE, sizeof(LogPayload), MALLOC_CAP_SPIRAM); 
  wifiCmdQueue = xQueueCreate(8, sizeof(WifiCommand_t));
  xTaskCreatePinnedToCore(task_sd_write,  "TaskSD",   8192, NULL, 2, NULL, 0);      //32 kByte, priority 2
  xTaskCreatePinnedToCore(task_rtdb_send, "TaskRTDB", 8192, NULL, 4, NULL, 0);      //32 kByte, priority 4
  xTaskCreatePinnedToCore(task_wifi_worker, "TaskWIFI", 4096, NULL, 6, NULL, 0);    //16 kByte, priority 6


//sd and wifi initialize
  init_sd();
  if (!isFlightMode) {
    connect_wifi();
    wifiAvailable = true;
  }
  if (sdAvailable) {
    for (uint8_t i=0;i<12;i++) {
      analogWrite(LED_BUILTIN, 255);    //off
      delay(300);
      analogWrite(LED_BUILTIN, 0);      //on
      delay(100);
    }
  }
  else {
    delay(5000); 
  }

//wdt setup
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = TIMEOUT_CORE1,
    .idle_core_mask = (IDLEWATCH_CORE0 << 0) | (IDLEWATCH_CORE1 << 1),
    .trigger_panic = true
  };
  esp_task_wdt_reconfigure(&wdt_config);
  esp_task_wdt_add(NULL);

//1 msec timer set
  myTimer = timerBegin(1000000);      //1 MHz clock
  timerAttachInterrupt(myTimer, &onTimer);

//startup initialize
  while (extSerial.available() > 0) {
    extSerial.read();
  }
  while (gpsSerial.available() > 0) {
    gpsSerial.read();
  }
  timer1sec =  999;
  timer2sec = 1999;
  timerAlarm(myTimer, 2000, true, 0);     //period 2 msec
}


//// Main loop
void loop() {
  uint8_t byte;
  uint16_t len;

//task monitoring
#if STACK_WATCH_ENABLE
  if (flagWatchReq) {printFreeStack();}
#endif
#if LOOP_MONITOR_ENABLE
  durationFirstHalf = (int16_t)(midTime - startTime);
  durationLastHalf  = (int16_t)(endTime - midTime);
  startTime = esp_timer_get_time();
  if (durationFirstHalf > peakFirstHalf) {peakFirstHalf = durationFirstHalf;}
  if (durationLastHalf  > peakLastHalf)  {peakLastHalf  = durationLastHalf;}
  if (flagWatchReq) {printLoopTime();}
#endif
#if IDLE_MONITOR_ENABLE
  if (flagWatchReq) {printIdleTime();}
#endif
#if TASK_WATCH_ENABLE
  if (flagWatchReq) {printTaskStatus();}
#endif
  flagWatchReq = false;

//env data generate
  generate_env();

//sw read and change flight mode
  if (flagSwReq) {
    swRead = (swRead << 1) + digitalRead(BOOT_SW);
    if (swRead == 0x00) {swResult = BOOT_SW_ON;}
    else if (swRead == 0xFF) {swResult = BOOT_SW_OFF;}
    if (swResult == BOOT_SW_ON) {countLongPress = countLongPress + 2;}
    else {countLongPress = 0;}
    if (countLongPress >= 3000) {
      countLongPress = 3000;       //upper limit
    }
    else if (countLongPress >= 2500) {
      flagModeChanged = true;
      countLongPress = 3000;       //detect done
    }
    if (flagModeChanged) {
      isFlightMode = !isFlightMode;
      timer2sec = 1999;     //atmic
      if (isFlightMode) {
        countWhileDisconnect = 2000;
      }
      else {
        connect_wifi();     //immediately connect request
        wifiAvailable = true;
      }
      flagModeChanged = false;
    }
    if (countWhileDisconnect > 0) {
      if (timer1sec > 950) {
        wifiAvailable = false;      //expect after send $ENV
      }
      if (countWhileDisconnect <= 2) {
        disconnect_wifi();      //delayed disconnect request
        wifiAvailable = false;
      }
      countWhileDisconnect = countWhileDisconnect - 2;
    }
    flagSwReq = false;
  }

//measure first half time
#if LOOP_MONITOR_ENABLE
  midTime = esp_timer_get_time();
#endif

//gps receive (0x0A termination)
  if (gpsSerial.available() > 0) {
    byte = gpsSerial.read();
    if (entryGpsBuf >= sizeof(bufferGps) - 1) {
      bufferGps[sizeof(bufferGps) - 1] = 0x00;
      entryGpsBuf = 0;
      isGpsDataRdy = true;
    }
    else if (byte == 0x0A) {
      bufferGps[entryGpsBuf] = 0x00;
      entryGpsBuf = 0;
      isGpsDataRdy = true;
      countNoGps = 0;       //atmic
    }
    else if (byte >= 0x20) {
      bufferGps[entryGpsBuf] = byte;
      entryGpsBuf++;
    }
  }

//ext receive (0x0D termination)
  if (extSerial.available() > 0) {
    byte = extSerial.read();
    if (byte == DEV_THIS) {
      allowExtAct = true;
    }
    else if ((byte >= 0x11) && (byte <= 0x14)) {
      allowExtAct = false;
    }
    else {
      if (allowExtAct) {
        if (entryExtBuf >= sizeof(bufferExt) - 1) {
          bufferExt[sizeof(bufferExt) - 1] = 0x00;
          memcpy(bufferExt, "$EXT,", 5);
          entryExtBuf = 5;
          isExtDataRdy = true;
        }
        else if (byte == 0x0D) {
          bufferExt[entryExtBuf] = 0x00;
          memcpy(bufferExt, "$EXT,", 5);
          entryExtBuf = 5;
          isExtDataRdy = true;
          countNoExt = 0;        //atmic
        }
        else if (byte >= 0x20) {
          bufferExt[entryExtBuf] = byte;
          entryExtBuf++;
        }
      }
    }
  }

//1 sec sync. source check
  if (isGpsDataRdy) {
    if (strlen(bufferGps) >= 19) {
      if (memcmp(&bufferGps[13], ".00,A,", 6) == 0) {     //GxRMC contains 'A' or 'V'
        noInterrupts();
        flagGpsSync = true;
        interrupts();
      }
    }
  }
  if (isExtDataRdy) {
    len = (uint16_t)strlen(syncToExt);
    if ((len > 0) && (strlen(bufferExt) >= len + 5)) {
      if (strncmp(&bufferExt[5], syncToExt, len) == 0) {
        noInterrupts();
        flagExtSync = true;
        interrupts();
      }
      else {isExtDataRdy = false;}      //ignore if header does not match
    }
    else {isExtDataRdy = false;}      //ignore if sentense is not comparable
  }

//send to queue
  if (isGpsDataRdy) {
  #if HIDE_LOCATION       //if send dummy data
    uint16_t i;
    uint16_t k1 = 0;
    uint16_t k2 = 0;
    char utc[16];  
    len = (uint16_t)strlen(bufferGps);
    for (i=0;i<len;i++)  {if (bufferGps[i] == ',') {k1 = i + 1; break;}}
    for (i=k1;i<len;i++) {if (bufferGps[i] == ',') {k2 = i;     break;}}
    if ((k1 + 9) == k2) {for (i=0;i<9;i++) {utc[i] = bufferGps[i + k1];}}
    if (memcmp(&bufferGps[3], "RMC", 3) == 0) {
      strncpy(bufferGps, "$GNRMC,000000.00,A,4000.00000,N,14000.00000,E,0.13,0.34,211121,,,A*40", sizeof(bufferGps) - 1);
    }
    else if (memcmp(&bufferGps[3], "GGA", 3) == 0) {
      strncpy(bufferGps, "$GNGGA,000000.00,4000.00000,N,14000.00000,E,1,08,0.9,-4.30,M,37.9981,M,,*51", sizeof(bufferGps) - 1);
    }
    bufferGps[sizeof(bufferGps) - 1] = '\0';
    if ((k1 + 9) == k2) {for (i=0;i<9;i++) {bufferGps[i + k1] = utc[i];}} 
  #endif
    dispatch_log_sentence(bufferGps);
    isGpsDataRdy = false;
  }
  if (isExtDataRdy) {
    dispatch_log_sentence(bufferExt);
    isExtDataRdy = false;
  }
  if (isEnvDataRdy) {
    dispatch_log_sentence(bufferEnv);
    isEnvDataRdy = false;
  }

//status led control
  if (flagLedReq) {
    static int16_t brightnessLed;
    if ((timer2sec % 5) == 0) {
      if (isFlightMode) {
        if (psOkSd && psOkWifi) {
          if (timer2sec < 1000) {brightnessLed = (timer2sec >> 2);}
          else {brightnessLed = ((1999 - timer2sec) >> 2);}
        }
        else {
          if (timer2sec < 200) {brightnessLed = timer2sec;}
          else if (timer2sec < 600) {brightnessLed = 600 - timer2sec;}
          else {brightnessLed = 0;}
        }
        brightnessLed = (brightnessLed > 255) ? 0 : (255 - brightnessLed);
        analogWrite(LED_BUILTIN, brightnessLed);
      }
      else {
        if (psOkSd && psOkWifi) {
          if (timer2sec < 1000) {analogWrite(LED_BUILTIN, 0);}
          else {analogWrite(LED_BUILTIN, 255);}
        }
        else {
          if (timer2sec < 100) {analogWrite(LED_BUILTIN, 0);}
          else {analogWrite(LED_BUILTIN, 255);}
        }
      }
    }
    flagLedReq = false;
  }

//measure last half time
#if LOOP_MONITOR_ENABLE
  endTime = esp_timer_get_time();
#endif

  delayMicroseconds(1);

}



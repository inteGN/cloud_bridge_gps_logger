//************************************************
//  FILE    :sdcard.ino
//  DATE    :2026/10/23
//  DESCRIPTION :sd task
//  BOARD TYPE  :XIAO ESP32S
//  AUTHER      :inteGN
//************************************************


//// Includes
#include <SPI.h>
#include <SD.h>


//// Definitions
#define   SD_FLUSHBYTE          4096


//// Objects and rams
          File        microSD;
          char        newfile[16];


//// Sd card handling
//file open
void  open_sd(void) {
  sdAvailable = false;
  microSD = SD.open(newfile, FILE_APPEND);
  if (microSD != false) {     //'if (microSD) {' does not work as expected
    sdAvailable = true;
  }
}

//file close (not used)
void  close_sd(void) {
  microSD.close();
  sdAvailable = false;
}

//file scan and make
void  scan_sd(void) {
  for (uint16_t i=0;i<1000;i++) {
    snprintf(newfile, sizeof(newfile), "/log%03u.txt", i);
    if (!SD.exists(newfile)) {
      break;
    }
  }
}


//// Initialize of sd card and file
void  init_sd(void) {
  SD.begin(SD_CS);
  scan_sd();
  open_sd();
}


//// Write sd card task (core 0)
void task_sd_write(void *pvParameters) {
  LogPayload  payload;
  uint32_t sdPrnCount = 0;
  bool flagFlushReq = false;
  uint32_t flushTime = 0;
#if STACK_WATCH_ENABLE
  uint32_t stackWatchTime = millis();
#endif

  while (1) {
//delayed flush
    if (flagFlushReq && sdAvailable && (sdPrnCount > 0)) {
      if (((timer1sec >= 500) && (timer1sec < 800)) || (sdPrnCount >= SD_FLUSHBYTE * 2)) {
        flushTime = millis();
#if TP_ENABLE_3
  TP_SET_HIGH;
#endif
        microSD.flush();
#if TP_ENABLE_3
  TP_SET_LOW;
#endif
        DBG_SD_PRINTF(" ◆◆◆  [Core 0 TaskSD] Flushed: %d\n", timer1sec);
        sdPrnCount = 0;
        flagFlushReq = false;
      }
    }

#if STACK_WATCH_ENABLE
//stack watch
  if (millis() > (uint32_t)(stackWatchTime + 907)) {
    freeStackSd = (uint32_t)uxTaskGetStackHighWaterMark(NULL) * 4;
    stackWatchTime = millis();
  }
#endif

//qeueu pickup
    if (xQueueReceive(sdQueue, &payload, pdMS_TO_TICKS(20)) == pdTRUE) {
      DBG_SD_PRINTF("  └─ [Core 0 TaskSD] Write: %s\n", payload.raw_line);
      if (sdAvailable) {
        microSD.println(payload.raw_line);
        sdPrnCount = sdPrnCount + strlen(payload.raw_line) + 2;
        if (sdPrnCount >= SD_FLUSHBYTE) {
          flagFlushReq = true;
          DBG_SD_PRINTLN("  └─ [Core 0 TaskSD] 4kbyte -> Flush Requested");
        }
        else if ((sdPrnCount > 0) && (millis() - flushTime > 120000)) {     //force flush when timeout
          flagFlushReq = true;
          DBG_SD_PRINTLN("  └─ [Core 0 TaskSD] Timeout(120s) -> Flush Requested");
        }
      }
    }
  }
}



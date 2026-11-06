//************************************************
//  FILE    :firebase.ino
//  DATE    :2026/10/23
//  DESCRIPTION :wifi and firebase task
//  BOARD TYPE  :XIAO ESP32S3
//  AUTHER      :inteGN
//************************************************


//// Includes
#include <WiFi.h>
#include <WiFiClientSecure.h>
#define  ENABLE_USER_AUTH       //allow user authentication macro
#define  ENABLE_DATABASE        //allow RTDB function macro
#include <FirebaseClient.h>
#include <FirebaseJson.h>
#include "firebase.h"


//// Definitions
#define   FB_EVENT_AUTH_START     "authenticating"
#define   FB_EVENT_AUTH_READY     "ready"
#define   AUTH_TIMEOUT            8000      //8 sec
#define   UPLOAD_WAIT              100      //100 msec
#define   FB_QUEUE_LIMIT             5      //Too large limit will cause error -118.
#define   AUTH_FORCE_PERIOD    2820000      //47 min. (EXPERIMENTAL)
                                            //^^^^^^  0=ignore this feature, and also automatically triggerd if larger than 55 min. set
#define   MAX_ARRAYDATASIZE       3600
#define   MAX_JSONDATASIZE        4096


//// Prototype
void asyncCB(AsyncResult &aResult);


//// Objects and rams
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);
UserAuth user_auth(API_KEY, USER_EMAIL, USER_PASSWORD);
FirebaseApp app;
RealtimeDatabase Database;

volatile  bool        isAuthenticating = false;
volatile  uint32_t    uploadStartTime = 0;
volatile  uint32_t    authStartTime   = 0;
volatile  int16_t     errCode = 0;
volatile  bool        flagAuthReq = false;
volatile  uint32_t    lastAuthTime;
volatile  uint32_t    lastConnTime;


//// Wifi wrapper function
//wifi connecting
void connect_wifi(void) {
  WifiCommand_t cmd = WIFI_CMD_CONNECT;
  xQueueSend(wifiCmdQueue, &cmd, 0);
}

//wifi disconnecting
void disconnect_wifi(void) {
  WifiCommand_t cmd = WIFI_CMD_DISCONNECT;
  xQueueSend(wifiCmdQueue, &cmd, 0);
}


//// Wifi connection task (core 0)
void task_wifi_worker(void *pvParameters) {
  WifiCommand_t cmd;
  const IPAddress   dns1(8, 8, 8, 8);     //primary DNS (example: Google Public DNS)
  const IPAddress   dns2(8, 8, 4, 4);     //secondary DNS (reserve)  

  while (1) {
    if (xQueueReceive(wifiCmdQueue, &cmd, portMAX_DELAY) == pdTRUE) {
      switch (cmd) {
        case WIFI_CMD_CONNECT:
          DBG_WF_PRINTLN("[Worker] Executing: CONNECT");
          WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, dns1, dns2);
          WiFi.setSleep(WIFI_PS_NONE);
          WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
          WiFi.setTxPower(WIFI_POWER_15dBm);
          lastConnTime = millis();
          break;
        case WIFI_CMD_DISCONNECT:
          DBG_WF_PRINTLN("[Worker] Executing: DISCONNECT");
          WiFi.disconnect(true);
          break;
        default:
          break;
      }
    }
  }
}


//// Callback function providing event and error information
void asyncCB(AsyncResult &aResult) {
  String currentUid = aResult.uid();
//lock-unlock while authenticaton 
  if (aResult.isEvent()) {
    DBG_FB_PRINTF("[Event] Task: %s, msg: %s\n", currentUid.c_str(), aResult.appEvent().message().c_str());
    if (currentUid == "auth_task") {
      if (strstr(aResult.appEvent().message().c_str(), FB_EVENT_AUTH_START) != NULL) {
        isAuthenticating = true;
        authStartTime = millis();
        DBG_FB_PRINTLN("[TaskWIFI] Auth started. Locking uploads.");
      }
      else if (strstr(aResult.appEvent().message().c_str(), FB_EVENT_AUTH_READY) != NULL) {
        isAuthenticating = false;
        DBG_FB_PRINTLN("[TaskWIFI] Auth success. Unlocking uploads.");
      }
    }
  }

//authentication or upload complete (payload from server response received)
  if (aResult.available()) {
    if (currentUid == "auth_task") {
      DBG_FB_PRINTF("[Success] Task: %s\n", currentUid.c_str());     //block authentic information
    }
    else{
      DBG_FB_PRINTF("[Success] Task: %s, Payload: %s\n", currentUid.c_str(), aResult.c_str());
      if ((AUTH_FORCE_PERIOD > 0) && (millis() > ((lastAuthTime + AUTH_FORCE_PERIOD) & 0xFFFFFFFF)) && (WiFi.status() == WL_CONNECTED)) {
        flagAuthReq = true;
      }
    }
  }

//error and debug output
  if (aResult.isError()) {
    DBG_FB_PRINTF("[Error] Task: %s, Msg: %s, Code: %d\n", currentUid.c_str(), aResult.error().message().c_str(), aResult.error().code());
    errCode = aResult.error().code();
  }
  if (aResult.isDebug()) {
    DBG_FB_PRINTF("[DEBUG] task: %s, msg: %s\n", currentUid.c_str(), aResult.debug().c_str());
  }
}


//// Initialize of Realtime Database
void  init_rtdb(void) {
  ssl_client.setInsecure();     //disable SSL certificate verification (for testing/development purposes)
  ssl_client.setTimeout(5000);
  ssl_client.setHandshakeTimeout(5);
  ssl_client.setConnectionTimeout(5000);
  ssl_client.setNoDelay(true);

  initializeApp(aClient, app, getAuth(user_auth), asyncCB, "auth_task");
  app.getApp<RealtimeDatabase>(Database);     //attach database object to applicaton
  Database.url(DATABASE_URL);
}


//// Firebase task (core 0)
void task_rtdb_send(void *pvParameters) {
  LogPayload payload;
  
  FirebaseJsonArray arr;
  String jsonStr;
  jsonStr.reserve(MAX_JSONDATASIZE);

  bool     isInitialized = false;
  uint32_t payloadSize;  
  uint32_t countArrayData = 0;
  uint32_t countJsonUpload = 0;
  int      currentErrno = errno;
  int      pastErrno = currentErrno;

//automatically extract device id from email address
  String emailStr = String(USER_EMAIL);
  String deviceId = "unknown";
  int atIndex = emailStr.indexOf('@');
  if (atIndex != -1) {
    deviceId = emailStr.substring(0, atIndex);      
  }

//firebaseclient background execution
  arr.clear();
  arr.setJsonArrayData("[]");
  authStartTime   = millis();
  uploadStartTime = authStartTime;  
  lastAuthTime    = authStartTime - AUTH_FORCE_PERIOD + 180000;     //set first trigger to 3 min.

#if STACK_WATCH_ENABLE
  uint32_t stackWatchTime = millis();
#endif

  while (1) {
    if (!isInitialized && (WiFi.status() == WL_CONNECTED)) {
      DBG_WF_PRINTLN("[TaskWIFI] Wi-Fi Connected. Initializing Firebase RTDB...");
      init_rtdb();
      isInitialized = true;
    }
    if (isInitialized) {

      uint32_t heapBefore = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
      uint32_t t0 = millis();

      app.loop();
      pastErrno = currentErrno;
      currentErrno = errno;       //it re-organizes complex situation(?)
      if (currentErrno != pastErrno) {
        DBG_FB_PRINTF("[ERRNO] lwIP_errno:%u\n", currentErrno);
      }

      uint32_t elapsed = millis() - t0;
      uint32_t heapAfter = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
      if (elapsed >= 10 || abs((int32_t)heapAfter -
                               (int32_t)heapBefore) >= 4096) {
        DBG_FB_PRINTF(
          "[APPLOOP] ms:%lu elapsed:%lu before:%lu after:%lu\n",
          millis(), elapsed, heapBefore, heapAfter
        );
      }
    }

#if STACK_WATCH_ENABLE
//stack watch
  if (millis() > ((stackWatchTime + 907) & 0xFFFFFFFF)) {
    freeStackWifi = (uint32_t)uxTaskGetStackHighWaterMark(NULL) * 4;
    stackWatchTime = millis();
  }
#endif

//timeout of authentication
    if (isAuthenticating && (millis() > ((authStartTime + AUTH_TIMEOUT) & 0xFFFFFFFF))) {
      DBG_WF_PRINTLN("[TaskWIFI] Authentication Timeout! Force resetting isAuthenticating flag.");
      isAuthenticating = false;
    }

//manually trigger re-authentication
    if (flagAuthReq && (WiFi.status() == WL_CONNECTED)) {
      DBG_FB_PRINTLN("[RE-AUTH] Time elapsed. Safeguarded Re-Authentication starting...");
      deinitializeApp(app);
      vTaskDelay(pdMS_TO_TICKS(100));
      initializeApp(aClient, app, getAuth(user_auth), asyncCB, "auth_task");
      lastAuthTime = millis();
      isAuthenticating = true;
      flagAuthReq = false;
    }

//skip if authenticating, while upload waiting, app or wifi not ready, or firebase queue limit
    if (isAuthenticating || (millis() < ((uploadStartTime + UPLOAD_WAIT) & 0xFFFFFFFF)) || !app.ready() ||
       (WiFi.status() != WL_CONNECTED) || ((unsigned int)aClient.taskCount() >= FB_QUEUE_LIMIT)) {

      if ((FB_QUEUE_LIMIT > 3) && (uint8_t)aClient.taskCount() >= (FB_QUEUE_LIMIT - 1)) {
        static uint32_t lastErrPrint = 0;
        if (millis() > ((lastErrPrint + 1000) & 0xFFFFFFFF)) {
          DBG_FB_PRINTF("[ERRNO] lwIP_errno:%u, taskCount:%d\n", 
                         currentErrno, (uint8_t)aClient.taskCount());
          lastErrPrint = millis();

/*
          *** Following code is comment-outed because dirty implemented and seems not effective. ***
          if (current_errno != 11 && current_errno != 118 && current_errno != 119) {
            DBG_FB_PRINTF("[NET_FIX] Dead socket detected (%d). Forcing ssl_client.stop()\n", current_errno);
            ssl_client.stop();      //github.com/mobizt/FirebaseClient/issues/333
          }
*/
/*
//        *** Following code is comment-outed because its effect is not evaluated. ***
          if (current_errno != 11 && current_errno != 118 && current_errno != 119) {
            if (millis() > ((lastConnTime + 3600000) & 0xFFFFFFFF)) {
              DBG_FB_PRINTF("[NET_FIX] Dead socket detected (%d). Forcing Wifi.recconect()\n", current_errno);
              WiFi.disconnect(false);
              vTaskDelay(pdMS_TO_TICKS(300));
              app.loop();
              WiFi.reconnect();
              vTaskDelay(pdMS_TO_TICKS(300));
              lastConnTime = millis();
            }
          }
*/

        }
      }

      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

//genenate and push new jsonstr
    if (xQueueReceive(wifiQueue, &payload, pdMS_TO_TICKS(10)) == pdTRUE) {
      DBG_WF_PRINTF("  └─ [Core 0 TaskWIFI] Buffered: %s\n", payload.raw_line);
      payloadSize = strlen(payload.raw_line);

      bool isEnv = (memcmp(&payload.raw_line[0], "$ENV", 4) == 0);
      if ((countArrayData + payloadSize < MAX_ARRAYDATASIZE) || isEnv) {
        arr.add(payload.raw_line);
        countArrayData += payloadSize;
      }

      if (isEnv || (countArrayData >= MAX_ARRAYDATASIZE)) {
        FirebaseJson data;
        data.set("device_id", deviceId);
        data.set("seq", countJsonUpload);
        data.set("lines", arr);
        String userPath = "/" + app.getUid();
        jsonStr.remove(0);      // clear for new jsonstr
        data.toString(jsonStr);
        countJsonUpload++;
        uploadStartTime = millis();

#if TP_ENABLE_3
  TP_SET_HIGH;
#endif
        ssl_client.connected();     //github.com/mobizt/FirebaseClient/discussions/326
        Database.push<object_t>(aClient, userPath, object_t(jsonStr.c_str()), asyncCB, "push_task");
#if TP_ENABLE_3
  TP_SET_LOW;
#endif

        DBG_WF_PRINTF("★★★★ [Core 0 TaskWIFI] Async Upload Triggered: %s\n", userPath.c_str());
        if ((uint8_t)aClient.taskCount() > 3) {
          DBG_FB_PRINTF(
            "[STATS] ms: %lu wifi: %d rssi: %d heap: %u minHeap: %u queue: %u\n",
            (unsigned long)millis(),
            (int)WiFi.status(),
            WiFi.RSSI(),
            (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
            (uint32_t)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
            (uint8_t)aClient.taskCount()
          );
        }
        arr.clear(); 
        arr.setJsonArrayData("[]");
        countArrayData = 0;        
      }
    }
    vTaskDelay(5);
  }

}





//************************************************
//  FILE    :ramdata.h
//  DATE    :2026/10/23
//  DESCRIPTION :data defininitions
//  BOARD TYPE  :XIAO ESP32S
//  AUTHER      :inteGN
//************************************************


//payload struct
          struct      LogPayload {
                        char  raw_line[256];
                      };

//timing control
volatile  uint16_t    timer1sec;
volatile  uint16_t    timer2sec;
volatile  bool        flagWatchReq;
volatile  uint8_t     flagDpsReq;
volatile  uint8_t     flagVbatReq;
volatile  bool        flagAdcReq;
volatile  bool        flagSwReq;
volatile  bool        flagLedReq;
volatile  bool        flagGpsSync;
volatile  bool        flagExtSync;
volatile  uint16_t    countNoGps = 10000;
volatile  uint16_t    countNoExt = 10000;
volatile  uint32_t    micros0;

//dps310 handling
          float       tempDps;
          float       floatPressAir;
          double      pressAir;
          bool        isEnvDataRdy;
          char        bufferEnv[64];

//battery voltage detect
          uint32_t    countVbatSum;
          uint32_t    sumVbatMv;
          float       vbatLogger;
          float       vbatLoggerMod;
          bool        psOkSd   = true;
          bool        psOkWifi = true;

//flight mode control
volatile  bool        isFlightMode  = false;
          bool        wifiAvailable = false;
          bool        flagModeChanged = false;
          bool        swResult = false;
          uint8_t     swRead = 0xFF;
          uint16_t    countLongPress = 0;
          uint16_t    countWhileDisconnect = 0;

//gps and ext handling
volatile  bool        sdAvailable = false;
          bool        isGpsDataRdy;
          bool        isExtDataRdy;
          bool        allowExtAct = true;
          uint16_t    entryGpsBuf = 0;
          uint16_t    entryExtBuf = 5;
          char        bufferGps[256];
          char        bufferExt[EXT_CHAR_LENGTH + 8];
const     char        syncToExt[] = EXT_SYNC_WORD;

//loop execution time monitoring
          int64_t     startTime;
          int64_t     midTime;
          int64_t     endTime;
          int16_t     durationFirstHalf;
          int16_t     durationLastHalf;
          int16_t     peakFirstHalf = 0;
          int16_t     peakLastHalf  = 0;

//stack monitoring
          uint32_t    freeStackSd;
          uint32_t    freeStackWifi;




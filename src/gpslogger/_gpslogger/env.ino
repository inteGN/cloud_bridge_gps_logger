//************************************************
//  FILE    :_gpslogger.ino
//  DATE    :2026/10/23
//  DESCRIPTION :gps data logger
//  BOARD TYPE  :XIAO ESP32S3
//  AUTHER      :inteGN
//************************************************


void  generate_env(void) {
#if DEBUG_EV_ENABLE
  uint32_t micros0 = micros();
#endif

//measure temperature and pressure
  if ((flagDpsReq == 1) && (timer1sec >= 200)) {
#if TP_ENABLE_2
  TP_SET_HIGH;
#endif
    if (dps310sensor.getSingleResult(floatPressAir) == 0) {
      if (floatPressAir >= 10000.0f) {pressAir = (double)floatPressAir;}
      else {pressAir = 0.0;}
    }
    else {pressAir = 0.0;}
    flagDpsReq= 2;
#if TP_ENABLE_2
  TP_SET_LOW;
#endif
  }

  else if ((flagDpsReq == 2) && (timer1sec >= 400)) {
#if TP_ENABLE_2
  TP_SET_HIGH;
#endif
    dps310sensor.startMeasureTempOnce(DPS_SAMPLE_TEMP);
    flagDpsReq= 3;
#if TP_ENABLE_2
  TP_SET_LOW;
#endif
  }

  else if ((flagDpsReq == 3) && (timer1sec >= 500)) {
#if TP_ENABLE_2
  TP_SET_HIGH;
#endif
    if (dps310sensor.getSingleResult(tempDps) != 0) {tempDps = - 99.0f;}
    flagDpsReq= 4;
#if TP_ENABLE_2
  TP_SET_LOW;
#endif
  }

  else if ((flagDpsReq == 4) && (timer1sec >= 900)) {
#if TP_ENABLE_2
  TP_SET_HIGH;
#endif
    dps310sensor.startMeasurePressureOnce(DPS_SAMPLE_PRESS);
    flagDpsReq = 5;
#if TP_ENABLE_2
  TP_SET_LOW;
#endif
  }

  else if ((flagDpsReq >=5) && (timer1sec >= 940)) {
    snprintf(bufferEnv, sizeof(bufferEnv), "$ENV,%.2f,%.4f,%.2f", tempDps, pressAir, vbatLogger);
    isEnvDataRdy = 1;
    flagDpsReq = 0;        //measure ended
    esp_task_wdt_reset();     //wdt reset
  }

//measure and check battery voltage
  if (flagAdcReq) {
    if ((flagVbatReq == 1) && (timer1sec >= 100)) {
      sumVbatMv = 0;
      countVbatSum = 0;
      flagVbatReq= 2;
    }
    else if (flagVbatReq >= 2) {
      if (timer1sec < 850) {
        sumVbatMv += analogReadMilliVolts(VB_AD);
        countVbatSum++;
      }
      else {
        if (countVbatSum > 0) {vbatLogger = (float)sumVbatMv / (float)countVbatSum / 1000.0f * DIV_VBAT;}
        vbatLoggerMod = vbatLoggerMod / IIR_CONST * (IIR_CONST - 1.0f) + vbatLogger / IIR_CONST;
        if (psOkSd) {if (vbatLoggerMod <= PS_UV_SD) {psOkSd = false;}}
        else        {if (vbatLoggerMod > (PS_UV_SD + PS_UV_HYSTERESIS)) {psOkSd = true;}}
        if (psOkWifi) {if (vbatLoggerMod <= PS_UV_WIFI) {psOkWifi = false;}}
        else          {if (vbatLoggerMod > (PS_UV_WIFI + PS_UV_HYSTERESIS)) {psOkWifi = true;}}
        flagVbatReq = 0;        //measure ended
      }
    }
    flagAdcReq = false;
  }

#if DEBUG_EV_ENABLE
  micros0 = micros() - micros0;
  if (micros0 > 2200) {
    Serial.printf("[Core 1 Warning] $ENV execution stretched: %d at %d\n", (int)micros0, timer1sec);
  } 
#endif
}



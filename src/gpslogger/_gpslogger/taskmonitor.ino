//************************************************
//  FILE    :taskmonitor.ino
//  DATE    :2026/10/23
//  DESCRIPTION :task monitor (experimantal)
//  BOARD TYPE  :XIAO ESP32S3
//  AUTHER      :inteGN
//************************************************

/*
There are experimental features.
Enabling them change the normal loop timing and have not been verified for all operating conditions.
Their values are only a reference and should not be interpreted as CPU usage by itself.
*/


//// Rams
volatile  uint32_t    idleCountCore0 = 0;
volatile  uint32_t    idleCountCore1 = 0;
          uint32_t    lastIdle0 = 0;
          uint32_t    lastIdle1 = 0;


////free stack watch
void printFreeStack(void) {
  Serial.printf("free stack watermarks   task_sd_write: %5lu byte    task_rtdb_send: %5lu byte\n", freeStackSd, freeStackWifi);
}


////loop() duration monitor
void printLoopTime(void) {
  Serial.printf("loop execution (peak)   First half: %5d usec  Last half: %5d usec\n", peakFirstHalf, peakLastHalf);
  peakFirstHalf = 0;
  peakLastHalf  = 0;
}


////Idle monitor
//idle result
void printIdleTime(void) {
  uint32_t idle0 = idleCountCore0 - lastIdle0;
  uint32_t idle1 = idleCountCore1 - lastIdle1;
  lastIdle0 = idleCountCore0;
  lastIdle1 = idleCountCore1;
  uint32_t totalTicks = 1000UL / portTICK_PERIOD_MS;        //period may be 1 msec
  float idlePercent0 = 100.0f * (float)idle0 / (float)totalTicks;
  if (idlePercent0 > 100.0f) {idlePercent0 = 100.0f;}
  float idlePercent1 = 100.0f * (float)idle1 / (float)totalTicks;
  if (idlePercent1 > 100.0f) {idlePercent1 = 100.0f;}
  Serial.printf("Core0 Idle: %6.2f %%   Core1 Idle: %6.2f %%\n", idlePercent0, idlePercent1);
}

//idle hook core 0
bool idleHookCore0(void) {
  idleCountCore0 = idleCountCore0 + 1;
  return true;
}

//idle hook core 1
bool idleHookCore1(void) {
  idleCountCore1 = idleCountCore1 + 1;
  return true;
}


//// Task watcher
void printTaskStatus(void) {
  char buffer[512];
  vTaskList(buffer);
  Serial.println("--- Task List ---");
  Serial.println("Name          State   Prio  Stack  Num");
  Serial.print(buffer);
  Serial.println("-----------------");
}



//************************************************
//  FILE    :firebase.h
//  DATE    :2026/10/23
//  DESCRIPTION :configulation header
//  BOARD TYPE  :XIAO ESP32S3
//  AUTHER      :inteGN
//************************************************


#pragma once

#include "secrets.h"

#ifndef WIFI_SSID
#error "You must define WIFI_SSID in secrets.h"
#endif

#ifndef WIFI_PASSWORD
#error "You must define WIFI_PASS in secrets.h"
#endif

#ifndef API_KEY
#error "You must define API_KEY in secrets.h"
#endif

#ifndef DATABASE_URL
#error "You must define DATABASE_URL in secrets.h"
#endif

#ifndef USER_EMAIL
#error "You must define USER_EMAIL in secrets.h"
#endif

#ifndef USER_PASSWORD
#error "You must define USER_PASSWORD in secrets.h"
#endif





# src/gpslogger

## 使用前の注意 / Before You Use This Code

このフォルダには、Firebase認証情報や WiFi設定を安全に扱うための  
**`secrets_example.h`（ダミー版）と注意書き（firebase_notes.h）** が含まれています。  
コードを使用する前に、必ず注意書きを読み必要な設定を行ってください。  

Before using this code, please read the included  
**`secrets_example.h` (dummy template) and `firebase_notes.h` (usage notes)**  
to understand how to safely configure Firebase credentials, Wi-Fi settings and optional location obfuscation.  
Make sure to update your own credentials before running the firmware.  

---

## 機能 / features

- GNSSデータ等のSDカード保存とFirebase Realtime Databaseアップロード
- 保存、アップロード間隔： 1sec
- 内蔵のDPS310気圧センサのデータ、UART経由の外部データを同時に保存・アップロード
- 機内モード切り替え付き（BOOTボタン長押しでトグル）
- バッテリ電圧を監視し、電圧低下時は保存とアップロードを停止
- 各種のデバッグ用ログ出力オプションあり

- SD card saving of GNSS data and uploading to Firebase Realtime Database
- Save and upload interval: 1 sec
- Concurrent saving and uploading of built-in DPS310 pressure sensor data and external data via UART
- Flight mode switching (toggled by long-pressing the BOOT button)
- Monitors battery voltage and stops data saving/uploading on low voltage
- Various debug log output options available

---
## 動作確認環境 / Dependencies

- MCU board    Seeed XIAO ESP32S3
- Arduino IDE    version 2.3.10
- ESP32 core library    version 3.3.12
- DPS310 library    DPS3xx.h 1.0.3
- SD library    sd.h 1.3.0
- Firebase library    FirebaseClient 2.2.13
- Firebasejson library    FirebaseJson 3.0.10

---

## 既知の問題 / Known issues

- 何らかの原因でアップロードが約30sec中断（現在調査中）
- アップロードが中断した時、JSONデータが1つロストする場合がある

- Uploads are interrupted for approximately 30 seconds due to an unknown cause (under investigation)
- A single JSON record may occasionally be lost when an upload is interrupted

---

## License
Copyright (c) 2026 inteGN - MIT License  


//************************************************
//  FILE    :firebase_notes.h
//  DATE    :2026/10/23
//  DESCRIPTION :note to use this project
//  BOARD TYPE  :
//  AUTHER      :inteGN
//************************************************


/*
 * Hints for configuring and handling the Firebase environment:
 *
 * 1. Generate a 32-character secure random password via Windows PowerShell:
 *    (Allows character repetition to maximize entropy, and excludes quotes to prevent string escaping issues)
 *
 *    $chars = ((97..122) + (65..90) + (48..57) + 95, 46 | ForEach-Object {[char]$_}); -join (1..32 | ForEach-Object { $chars | Get-Random }) | Set-Clipboard
 *
 *
 * 2. Authentication Credentials (Email/Password):
 *
 *    Firebase Authentication (Email/Password) is used to obtain a unique UID
 *    for each device. The UID determines the write path in RTDB:
 *
 *        String userPath = "/" + app.getUid();
 *
 *    This ensures that each device writes only to its own isolated namespace.
 *
 *    Important:
 *      - Email and password credentials must be stored exclusively in secrets.h.
 *      - Never embed credentials directly in source files such as main.cpp,
 *        firebase.ino, or firebase.h.
 *      - The repository includes a dummy secrets.h template as secrets_example.h;
 *        users must replace the placeholder values with their own credentials.
 *
 *    Example (secrets.h):
 *      #define USER_EMAIL    "your-email-address"
 *      #define USER_PASSWORD "your-secure-password"
 *      - The firmware will fail to build if these values are missing, preventing
 *        accidental publication of real credentials.
 *
 *
 * 3. An example security rules for Firebase Realtime Database (RTDB):
 *    (Enables per-MCU write isolation and read-only access for the Analyst PC)
 *
 *    {
 *      "rules": {
 *        "$uid": {
 *          // 1. Allow each MCU to write only to its own dedicated path, restricting Analyst access
 *          ".write": "auth != null && auth.uid === $uid && auth.uid !== 'ANALYST_A_UID_HERE' && auth.uid !== 'ANALYST_B_UID_HERE'",
 *
 *          // 2. Allow each MCU's path to be read exclusively by the specific Analyst PCs
 *          ".read": "auth != null && (auth.uid === 'ANALYST_A_UID_HERE' || auth.uid === 'ANALYST_B_UID_HERE')"
 *        }
 *      }
 *    }
 *
 *
 * 4. Firebase-ESP32 library Error Code Ranges and Meanings:
 *
 *   [Positive Values: 100 to 599] HTTP status codes returned from the Firebase server.
 *      - 400: Bad Request (Authentication failure, such as an incorrect password or malformed email format).
 *      - 401: Unauthorized (Invalid or expired ID token).
 *      - 403: Forbidden (Write/Read request rejected by RTDB "Security Rules").
 *      - 429: Too Many Requests (Rate limit or data volume quota exceeded).
 *
 *   [Negative Values: -1 to -200] Client-side network/communication layer errors.
 *      - -115: TCP disconnection or no internet connection.
 *      - -118: Connection timeout or automatic operation cancellation.
 *      - -127: Missing credentials (such as an unconfigured or malformed database URL).
 *
 *
 * 5. lwIP (TCP/IP Network Stack) Error Codes (errno) and Meanings:
 *
 *   [Positive Values: 1 to 150] Platform/OS level socket and network layer errors.
 *      - 4:   EINTR (Interrupted system call). Temporary OS interruption, safe to retry.
 *      - 9:   EBADF (Bad file descriptor). Socket handle is invalid or already closed.
 *      - 11:  EAGAIN / EWOULDBLOCK (Resource temporarily unavailable). TX buffer full.
 *      - 104: ECONNRESET (Connection reset by peer). Disconnected by router or server.
 *      - 110: ETIMEDOUT (Connection timed out). No response/ACK from target server.
 *      - 113: EHOSTUNREACH (No route to host). Routing failure or internet disconnected.
 *      - 114: ENETUNREACH (Network is unreachable). Entire network path is lost.
 *      - 118: EHOSTDOWN (Host is down). Transient route state or host unavailable.
 *      - 119: EINPROGRESS / EALREADY (Operation in progress). Socket is connecting.
 *      - 128: ENOTCONN (Socket is not connected). TCP link is broken (zombie socket).
 *
 */




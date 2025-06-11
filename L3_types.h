#ifndef L3_TYPES_H
#define L3_TYPES_H

#include "mbed.h"
#include "L3_msg.h"

// 사용자 정보 구조체
typedef struct {
    uint8_t userId;           // 사용자 ID
    uint8_t isRegistered;     // 등록 여부 플래그
    uint8_t isActive;         // 활성(체험 중) 여부 플래그
    uint8_t waitingNumber;    // 대기 순번
    uint32_t checkInTime;     // 체크인(등록) 시각
    uint32_t sessionStartTime;// 세션 시작 시각
} User_t;

// 스캔용 부스 정보 구조체 (신규)
typedef struct {
    uint8_t boothId;          // 부스 ID
    int16_t rssi;             // 수신 신호 세기(RSSI)
    uint8_t currentCount;     // 현재 부스 이용자 수
    uint8_t capacity;         // 부스 정원(최대 수용 인원)
    uint8_t waitingCount;     // 대기열 인원 수
    uint32_t lastSeenTime;    // 마지막으로 스캔된 시각(타임아웃 용)
    uint8_t isValid;          // 유효 항목 여부 플래그
} BoothScanInfo_t;

// 부스 정보 구조체
typedef struct {
    uint8_t boothId;                      // 부스 ID
    uint8_t capacity;                     // 부스 정원
    uint8_t currentCount;                 // 현재 이용자 수
    uint8_t waitingCount;                 // 대기열 인원 수
    char description[50];                 // 부스 설명 문자열
    User_t registeredList[MAX_USERS];     // 등록된 사용자 정보 목록(영구 기록)
    uint8_t registeredUserIds[MAX_USERS]; // 등록된 사용자 ID 배열
    User_t activeList[MAX_BOOTH_CAPACITY];// 활성 사용자 정보 목록
    User_t waitingQueue[MAX_USERS];       // 대기열 사용자 정보 목록
} Booth_t;

#endif // L3_TYPES_H

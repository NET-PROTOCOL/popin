#ifndef L3_TYPES_H
#define L3_TYPES_H

#include "mbed.h"
#include "L3_msg.h"

// User structure
typedef struct {
    uint8_t userId;
    uint8_t isRegistered;
    uint8_t isActive;
    uint8_t waitingNumber;
    uint32_t checkInTime;
    uint32_t sessionStartTime;
} User_t;

// Booth structure
typedef struct {
    uint8_t boothId;
    uint8_t capacity;
    uint8_t currentCount;
    uint8_t waitingCount;
    char description[50];
    User_t registeredList[MAX_USERS];  // Permanent record
    uint8_t registeredUserIds[MAX_USERS];  // Simple array for registered IDs
    User_t activeList[MAX_BOOTH_CAPACITY];
    User_t waitingQueue[MAX_USERS];
} Booth_t;

#endif // L3_TYPES_H
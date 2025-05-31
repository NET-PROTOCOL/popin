#include "mbed.h"
#include "L3_msg.h"
#include <string.h>

// Get message type from buffer
uint8_t L3_msg_getType(uint8_t* msg) {
    return msg[L3_MSG_OFFSET_TYPE];
}

// Get pointer to message data
uint8_t* L3_msg_getData(uint8_t* msg) {
    return &msg[L3_MSG_OFFSET_DATA];
}

// Encode USER_INFO_REQUEST message
uint8_t L3_msg_encodeUserInfoRequest(uint8_t* msg) {
    msg[L3_MSG_OFFSET_TYPE] = MSG_TYPE_USER_INFO_REQUEST;
    return 1; // Total message size
}

// Encode CONNECT_REQUEST message
uint8_t L3_msg_encodeConnectRequest(uint8_t* msg, uint8_t userId) {
    msg[L3_MSG_OFFSET_TYPE] = MSG_TYPE_CONNECT_REQUEST;
    msg[L3_MSG_OFFSET_DATA] = userId;
    return 2; // Total message size
}

// Encode BOOTH_INFO message
uint8_t L3_msg_encodeBoothInfo(uint8_t* msg, uint8_t currentCount, uint8_t capacity, 
                                uint8_t waitingCount, const char* description) {
    msg[L3_MSG_OFFSET_TYPE] = MSG_TYPE_BOOTH_INFO;
    msg[L3_MSG_OFFSET_DATA] = currentCount;
    msg[L3_MSG_OFFSET_DATA + 1] = capacity;
    msg[L3_MSG_OFFSET_DATA + 2] = waitingCount;
    
    uint8_t descLen = strlen(description);
    memcpy(&msg[L3_MSG_OFFSET_DATA + 3], description, descLen + 1); // Include null terminator
    
    return L3_MSG_OFFSET_DATA + 3 + descLen + 1; // Total message size
}

// Encode REGISTER_RESPONSE message
uint8_t L3_msg_encodeRegisterResponse(uint8_t* msg, uint8_t success, uint8_t reason) {
    msg[L3_MSG_OFFSET_TYPE] = MSG_TYPE_REGISTER_RESPONSE;
    msg[L3_MSG_OFFSET_DATA] = success;
    msg[L3_MSG_OFFSET_DATA + 1] = reason;
    return 3; // Total message size
}

// Encode QUEUE_INFO message
uint8_t L3_msg_encodeQueueInfo(uint8_t* msg, uint8_t queueNumber, uint8_t totalWaiting) {
    msg[L3_MSG_OFFSET_TYPE] = MSG_TYPE_QUEUE_INFO;
    msg[L3_MSG_OFFSET_DATA] = queueNumber;
    msg[L3_MSG_OFFSET_DATA + 1] = totalWaiting;
    return 3; // Total message size
}

// Encode BOOTH_ANNOUNCE message
uint8_t L3_msg_encodeBoothAnnounce(uint8_t* msg, uint8_t boothId, uint8_t currentCount, 
                                   uint8_t capacity, uint8_t waitingCount) {
    msg[L3_MSG_OFFSET_TYPE] = MSG_TYPE_BOOTH_ANNOUNCE;
    msg[L3_MSG_OFFSET_DATA] = boothId;
    msg[L3_MSG_OFFSET_DATA + 1] = currentCount;
    msg[L3_MSG_OFFSET_DATA + 2] = capacity;
    msg[L3_MSG_OFFSET_DATA + 3] = waitingCount;
    return 5; // Total message size
}

// Encode ADMIN_MESSAGE message
uint8_t L3_msg_encodeAdminMessage(uint8_t* msg, const char* message) {
    msg[L3_MSG_OFFSET_TYPE] = MSG_TYPE_ADMIN_MESSAGE;
    uint8_t msgLen = strlen(message);
    if (msgLen > 100) msgLen = 100;  // Limit message length
    memcpy(&msg[L3_MSG_OFFSET_DATA], message, msgLen + 1); // Include null terminator
    return L3_MSG_OFFSET_DATA + msgLen + 1; // Total message size
}
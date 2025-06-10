#ifndef L3_MSG_H
#define L3_MSG_H

#include "mbed.h"

// Message Types according to Pop-in Protocol
#define MSG_TYPE_USER_INFO_REQUEST      0x01
#define MSG_TYPE_CONNECT_REQUEST        0x02
#define MSG_TYPE_CONNECT_RESPONSE       0x03
#define MSG_TYPE_BOOTH_INFO             0x04
#define MSG_TYPE_USER_RESPONSE          0x05
#define MSG_TYPE_REGISTER_REQUEST       0x06
#define MSG_TYPE_REGISTER_RESPONSE      0x07
#define MSG_TYPE_QUEUE_JOIN_REQUEST     0x08
#define MSG_TYPE_QUEUE_INFO             0x09
#define MSG_TYPE_EXIT_REQUEST           0x0A
#define MSG_TYPE_TIMEOUT_ALERT          0x0B
#define MSG_TYPE_ADMIN_MESSAGE          0x0C
#define MSG_TYPE_CHAT_MESSAGE           0x0D
#define MSG_TYPE_BOOTH_SCAN             0x0E
#define MSG_TYPE_BOOTH_ANNOUNCE         0x0F
#define MSG_TYPE_EXIT_RESPONSE          0x10
#define MSG_TYPE_QUEUE_READY            0x11  // 입장 준비 알림
#define MSG_TYPE_QUEUE_READY_ACK        0x12  // 입장 준비 확인 (예약)
#define MSG_TYPE_QUEUE_UPDATE           0x13  // 대기 순번 업데이트
#define MSG_TYPE_QUEUE_LEAVE            0x14  // 대기열 이탈 요청

// User Response Values
#define USER_RESPONSE_YES               1
#define USER_RESPONSE_NO                0

// Register Response Reasons
#define REGISTER_REASON_SUCCESS         0
#define REGISTER_REASON_ALREADY_USED    1
#define REGISTER_REASON_FULL_WAITING    2

// Special IDs
#define BROADCAST_ID                    255
#define ADMIN_ID_START                  1
#define ADMIN_ID_END                    3

// Capacity and Limits
#define MAX_BOOTH_CAPACITY              2     // 테스트를 위해 2명으로 설정
#define MAX_USERS                       20
#define MAX_BOOTHS                      3

// Message structure offsets
#define L3_MSG_OFFSET_TYPE              0
#define L3_MSG_OFFSET_DATA              1

// Message encoding/decoding functions
uint8_t L3_msg_getType(uint8_t* msg);
uint8_t* L3_msg_getData(uint8_t* msg);

// Message encoding functions
uint8_t L3_msg_encodeUserInfoRequest(uint8_t* msg);
uint8_t L3_msg_encodeConnectRequest(uint8_t* msg, uint8_t userId);
uint8_t L3_msg_encodeBoothInfo(uint8_t* msg, uint8_t currentCount, uint8_t capacity, 
                                uint8_t waitingCount, const char* description);
uint8_t L3_msg_encodeUserResponse(uint8_t* msg, uint8_t response);
uint8_t L3_msg_encodeRegisterResponse(uint8_t* msg, uint8_t success, uint8_t reason);
uint8_t L3_msg_encodeQueueInfo(uint8_t* msg, uint8_t queueNumber, uint8_t totalWaiting);
uint8_t L3_msg_encodeBoothAnnounce(uint8_t* msg, uint8_t boothId, uint8_t currentCount, 
                                   uint8_t capacity, uint8_t waitingCount);
uint8_t L3_msg_encodeAdminMessage(uint8_t* msg, const char* message);
uint8_t L3_msg_encodeChatMessage(uint8_t* msg, const char* message);
uint8_t L3_msg_encodeChatMessageWithSender(uint8_t* msg, uint8_t senderId, const char* message);

#endif // L3_MSG_H
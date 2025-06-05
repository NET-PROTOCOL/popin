#ifndef L3_MSG_H
#define L3_MSG_H

#include "mbed.h"

// Message Types according to Pop-in Protocol
#define MSG_TYPE_USER_INFO_REQUEST      0x01  // 
#define MSG_TYPE_CONNECT_REQUEST        0x02  // 연결 요청
#define MSG_TYPE_CONNECT_RESPONSE       0x03  // 연결 응답
#define MSG_TYPE_BOOTH_INFO             0x04  // 부스 상세 정보 응답
#define MSG_TYPE_USER_RESPONSE          0x05  // 사용자 응답
#define MSG_TYPE_REGISTER_REQUEST       0x06  // 등록 요청
#define MSG_TYPE_REGISTER_RESPONSE      0x07  // 등록 결과
#define MSG_TYPE_QUEUE_JOIN_REQUEST     0x08  // 대기열 진입 요청
#define MSG_TYPE_QUEUE_INFO             0x09  // 대기 정보
#define MSG_TYPE_EXIT_REQUEST           0x0A  // 퇴장 요청
#define MSG_TYPE_TIMEOUT_ALERT          0x0B  // 타임 아웃 알림
#define MSG_TYPE_ADMIN_MESSAGE          0x0C  // 관리자 메시지
#define MSG_TYPE_CHAT_MESSAGE           0x0D  // 사용자 채팅
#define MSG_TYPE_BOOTH_SCAN             0x0E  // 부스 스캔 요청
#define MSG_TYPE_BOOTH_ANNOUNCE         0x0F  // 부스 정보 응답
#define MSG_TYPE_EXIT_RESPONSE          0x10  // exit
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
#define MAX_BOOTH_CAPACITY              1
#define MAX_USERS                       20
#define MAX_BOOTHS                      3

// Message structure offsets
#define L3_MSG_OFFSET_TYPE              0
#define L3_MSG_OFFSET_DATA              1

// Message encoding/decoding functions (to be implemented in L3_msg.cpp)
uint8_t L3_msg_getType(uint8_t* msg);
uint8_t* L3_msg_getData(uint8_t* msg);

// Message encoding functions
uint8_t L3_msg_encodeUserInfoRequest(uint8_t* msg);
uint8_t L3_msg_encodeConnectRequest(uint8_t* msg, uint8_t userId);
uint8_t L3_msg_encodeBoothInfo(uint8_t* msg, uint8_t currentCount, uint8_t capacity, 
                                uint8_t waitingCount, const char* description);
uint8_t L3_msg_encodeUserResponse(uint8_t* msg, uint8_t response);  // Add this
uint8_t L3_msg_encodeRegisterResponse(uint8_t* msg, uint8_t success, uint8_t reason);
uint8_t L3_msg_encodeQueueInfo(uint8_t* msg, uint8_t queueNumber, uint8_t totalWaiting);
uint8_t L3_msg_encodeBoothAnnounce(uint8_t* msg, uint8_t boothId, uint8_t currentCount, 
                                   uint8_t capacity, uint8_t waitingCount);
uint8_t L3_msg_encodeAdminMessage(uint8_t* msg, const char* message);  // Admin message function

#endif // L3_MSG_H
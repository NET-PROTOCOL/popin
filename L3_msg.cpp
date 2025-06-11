#include "mbed.h"
#include "L3_msg.h"
#include <string.h>

// 버퍼에서 메시지 타입을 읽어 반환 (첫 번째 바이트)
uint8_t L3_msg_getType(uint8_t* msg) {
    return msg[L3_MSG_OFFSET_TYPE];
}

// 버퍼에서 메시지 데이터가 시작되는 위치의 포인터 반환 (두 번째 바이트 주소)
uint8_t* L3_msg_getData(uint8_t* msg) {
    return &msg[L3_MSG_OFFSET_DATA];
}

// USER_INFO_REQUEST 메시지 인코딩
//   - 타입 필드만 설정
//   - 전체 메시지 크기: 1바이트
uint8_t L3_msg_encodeUserInfoRequest(uint8_t* msg) {
    msg[L3_MSG_OFFSET_TYPE] = MSG_TYPE_USER_INFO_REQUEST;
    return 1;
}

// CONNECT_REQUEST 메시지 인코딩
//   - 타입 + 사용자 ID 포함
//   - 전체 메시지 크기: 2바이트
uint8_t L3_msg_encodeConnectRequest(uint8_t* msg, uint8_t userId) {
    msg[L3_MSG_OFFSET_TYPE] = MSG_TYPE_CONNECT_REQUEST;
    msg[L3_MSG_OFFSET_DATA] = userId;
    return 2;
}

// USER_RESPONSE 메시지 인코딩
//   - 타입 + 예(1) 또는 아니오(0) 응답 포함
//   - 전체 메시지 크기: 2바이트
uint8_t L3_msg_encodeUserResponse(uint8_t* msg, uint8_t response) {
    msg[L3_MSG_OFFSET_TYPE] = MSG_TYPE_USER_RESPONSE;
    msg[L3_MSG_OFFSET_DATA] = response;
    return 2;
}

// BOOTH_INFO 메시지 인코딩
//   - 타입 + 현재 사용자 수 + 정원 + 대기열 수 + 설명 문자열
//   - 문자열은 널 종료 포함 복사
uint8_t L3_msg_encodeBoothInfo(uint8_t* msg, uint8_t currentCount, uint8_t capacity, 
                                uint8_t waitingCount, const char* description) {
    msg[L3_MSG_OFFSET_TYPE] = MSG_TYPE_BOOTH_INFO;
    msg[L3_MSG_OFFSET_DATA] = currentCount;
    msg[L3_MSG_OFFSET_DATA + 1] = capacity;
    msg[L3_MSG_OFFSET_DATA + 2] = waitingCount;
    
    uint8_t descLen = strlen(description);
    memcpy(&msg[L3_MSG_OFFSET_DATA + 3], description, descLen + 1); // 널 종료 포함 복사
    
    return L3_MSG_OFFSET_DATA + 3 + descLen + 1; // 전체 메시지 크기 반환
}

// REGISTER_RESPONSE 메시지 인코딩
//   - 타입 + 성공 여부 + 실패 이유 코드 포함
//   - 전체 메시지 크기: 3바이트
uint8_t L3_msg_encodeRegisterResponse(uint8_t* msg, uint8_t success, uint8_t reason) {
    msg[L3_MSG_OFFSET_TYPE] = MSG_TYPE_REGISTER_RESPONSE;
    msg[L3_MSG_OFFSET_DATA] = success;
    msg[L3_MSG_OFFSET_DATA + 1] = reason;
    return 3;
}

// QUEUE_INFO 메시지 인코딩
//   - 타입 + 내 대기 순번 + 전체 대기 인원
//   - 전체 메시지 크기: 3바이트
uint8_t L3_msg_encodeQueueInfo(uint8_t* msg, uint8_t queueNumber, uint8_t totalWaiting) {
    msg[L3_MSG_OFFSET_TYPE] = MSG_TYPE_QUEUE_INFO;
    msg[L3_MSG_OFFSET_DATA] = queueNumber;
    msg[L3_MSG_OFFSET_DATA + 1] = totalWaiting;
    return 3;
}

// BOOTH_ANNOUNCE 메시지 인코딩
//   - 타입 + 부스 ID + 이용 중 사용자 수 + 정원 + 대기열 수
//   - 전체 메시지 크기: 5바이트
uint8_t L3_msg_encodeBoothAnnounce(uint8_t* msg, uint8_t boothId, uint8_t currentCount, 
                                   uint8_t capacity, uint8_t waitingCount) {
    msg[L3_MSG_OFFSET_TYPE] = MSG_TYPE_BOOTH_ANNOUNCE;
    msg[L3_MSG_OFFSET_DATA] = boothId;
    msg[L3_MSG_OFFSET_DATA + 1] = currentCount;
    msg[L3_MSG_OFFSET_DATA + 2] = capacity;
    msg[L3_MSG_OFFSET_DATA + 3] = waitingCount;
    return 5;
}

// ADMIN_MESSAGE 메시지 인코딩
//   - 타입 + 관리자 메시지 문자열 (최대 100자 + 널 종료)
//   - 문자열 길이 제한 적용
uint8_t L3_msg_encodeAdminMessage(uint8_t* msg, const char* message) {
    msg[L3_MSG_OFFSET_TYPE] = MSG_TYPE_ADMIN_MESSAGE;
    uint8_t msgLen = strlen(message);
    if (msgLen > 100) msgLen = 100;  // 메시지 길이 제한
    memcpy(&msg[L3_MSG_OFFSET_DATA], message, msgLen + 1); // 널 종료 포함
    return L3_MSG_OFFSET_DATA + msgLen + 1;
}

// CHAT_MESSAGE 인코딩 (사용자가 Admin에게 보낼 때)
//   - 타입 + 메시지 문자열
uint8_t L3_msg_encodeChatMessage(uint8_t* msg, const char* message) {
    msg[L3_MSG_OFFSET_TYPE] = MSG_TYPE_CHAT_MESSAGE;
    uint8_t msgLen = strlen(message);
    if (msgLen > 100) msgLen = 100;  // 길이 제한
    memcpy(&msg[L3_MSG_OFFSET_DATA], message, msgLen + 1); // 널 종료 포함
    return L3_MSG_OFFSET_DATA + msgLen + 1;
}

// CHAT_MESSAGE 인코딩 (관리자가 중계할 때)
//   - 타입 + 발신자 ID + 메시지 문자열
//   - 발신자 ID 자리 하나 확보, 문자열 최대 99자로 제한
uint8_t L3_msg_encodeChatMessageWithSender(uint8_t* msg, uint8_t senderId, const char* message) {
    msg[L3_MSG_OFFSET_TYPE] = MSG_TYPE_CHAT_MESSAGE;
    msg[L3_MSG_OFFSET_DATA] = senderId;  // 첫 번째 데이터 바이트는 발신자 ID
    
    uint8_t msgLen = strlen(message);
    if (msgLen > 99) msgLen = 99;  // ID 바이트 공간 제외
    memcpy(&msg[L3_MSG_OFFSET_DATA + 1], message, msgLen + 1); // 널 종료 포함
    
    return L3_MSG_OFFSET_DATA + 1 + msgLen + 1; // 전체 메시지 크기 반환
}
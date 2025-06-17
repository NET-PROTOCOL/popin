#ifndef L3_MSG_H
#define L3_MSG_H

#include "mbed.h"

// Pop-in 프로토콜에 따른 메시지 타입 정의
#define MSG_TYPE_USER_INFO_REQUEST      0x01  // 사용자 정보 요청
#define MSG_TYPE_CONNECT_REQUEST        0x02  // 부스 연결 요청
#define MSG_TYPE_CONNECT_RESPONSE       0x03  // 부스 연결 응답
#define MSG_TYPE_BOOTH_INFO             0x04  // 부스 정보 전달
#define MSG_TYPE_USER_RESPONSE          0x05  // 사용자 y/n 응답
#define MSG_TYPE_REGISTER_REQUEST       0x06  // 등록(체험) 요청
#define MSG_TYPE_REGISTER_RESPONSE      0x07  // 등록 응답
#define MSG_TYPE_QUEUE_JOIN_REQUEST     0x08  // 대기열 합류 요청
#define MSG_TYPE_QUEUE_INFO             0x09  // 대기열 정보(순번/총 대기자)
#define MSG_TYPE_EXIT_REQUEST           0x0A  // 부스 퇴장 요청
#define MSG_TYPE_TIMEOUT_ALERT          0x0B  // 세션 시간 만료 알림
#define MSG_TYPE_ADMIN_MESSAGE          0x0C  // 관리자 방송 메시지
#define MSG_TYPE_CHAT_MESSAGE           0x0D  // 채팅 메시지
#define MSG_TYPE_BOOTH_SCAN             0x0E  // 부스 스캔 요청
#define MSG_TYPE_BOOTH_ANNOUNCE         0x0F  // 부스 방송(정보) 메시지
#define MSG_TYPE_EXIT_RESPONSE          0x10  // 퇴장 응답
#define MSG_TYPE_QUEUE_READY            0x11  // 입장 준비 알림
#define MSG_TYPE_QUEUE_READY_ACK        0x12  // 입장 준비 확인(예약)
#define MSG_TYPE_QUEUE_UPDATE           0x13  // 대기 순번 업데이트
#define MSG_TYPE_QUEUE_LEAVE            0x14  // 대기열 이탈 요청
#define ALREADY_WAITING                 0x15  // 이미 대기열 있음

// 사용자 응답 값
#define USER_RESPONSE_YES               1     // y
#define USER_RESPONSE_NO                0     // n

// 등록 응답 사유 코드
#define REGISTER_REASON_SUCCESS         0     // 등록 성공
#define REGISTER_REASON_ALREADY_USED    1     // 이미 체험함
#define REGISTER_REASON_FULL_WAITING    2     // 만원으로 대기열 등록됨

// 특별 ID
#define BROADCAST_ID                    255   // 전체 방송 대상 ID
#define ADMIN_ID_START                  1     // 관리자 ID 시작
#define ADMIN_ID_END                    3     // 관리자 ID 끝

// 용량 및 제한
#define MAX_BOOTH_CAPACITY              1     // 부스 정원
#define MAX_USERS                       20    // 최대 사용자 수
#define MAX_BOOTHS                      3     // 최대 부스 수

// 메시지 구조 내 오프셋
#define L3_MSG_OFFSET_TYPE              0     // 타입 필드 위치
#define L3_MSG_OFFSET_DATA              1     // 데이터 시작 위치

// 메시지 디코딩 함수
uint8_t  L3_msg_getType(uint8_t* msg);     // 버퍼에서 타입 읽기
uint8_t* L3_msg_getData(uint8_t* msg);     // 버퍼에서 데이터 시작 주소 얻기

// 메시지 인코딩 함수 선언
uint8_t L3_msg_encodeUserInfoRequest(uint8_t* msg);
uint8_t L3_msg_encodeConnectRequest(uint8_t* msg, uint8_t userId);
uint8_t L3_msg_encodeBoothInfo(uint8_t* msg,
                               uint8_t currentCount,
                               uint8_t capacity,
                               uint8_t waitingCount,
                               const char* description);
uint8_t L3_msg_encodeUserResponse(uint8_t* msg, uint8_t response);
uint8_t L3_msg_encodeRegisterResponse(uint8_t* msg,
                                      uint8_t success,
                                      uint8_t reason);
uint8_t L3_msg_encodeQueueInfo(uint8_t* msg,
                               uint8_t queueNumber,
                               uint8_t totalWaiting);
uint8_t L3_msg_encodeBoothAnnounce(uint8_t* msg,
                                   uint8_t boothId,
                                   uint8_t currentCount,
                                   uint8_t capacity,
                                   uint8_t waitingCount);
uint8_t L3_msg_encodeAdminMessage(uint8_t* msg, const char* message);
uint8_t L3_msg_encodeChatMessage(uint8_t* msg, const char* message);
uint8_t L3_msg_encodeChatMessageWithSender(uint8_t* msg,
                                           uint8_t senderId,
                                           const char* message);

#endif // L3_MSG_H
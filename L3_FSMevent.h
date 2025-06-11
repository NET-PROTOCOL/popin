typedef enum L3_event
{
    L3_event_msgRcvd = 2,
    L3_event_dataToSend = 4,
    L3_event_dataSendCnf = 5,
    L3_event_recfgSrcIdCnf = 6,
    L3_event_keyboardInput = 7,
    L3_event_boothSelectionTimeout = 8,  // RSSI 기반 최적 부스 선택
    L3_event_connectTimeout = 9,          // 연결 응답 타임아웃
    L3_event_registerTimeout = 10,        // 등록 응답 타임아웃
    L3_event_sessionTimeout = 11,         // 세션 타이머 만료
    L3_event_queueReadyTimeout = 12,      // 큐 준비 응답 타임아웃
    L3_event_exitRequest = 13,            // 사용자 퇴장 요청 (키보드 'e')
    L3_event_chatRequest = 14,            // 채팅 시작 요청 (키보드 'c')
    L3_event_userResponseYes = 15,        // 사용자 YES 응답
    L3_event_userResponseNo = 16,         // 사용자 NO 응답
    L3_event_adminBroadcast = 17,         // 관리자 방송 타이머
    L3_event_boothInfoReceived = 18,      // 부스 정보 수신 완료
    L3_event_registerResponseReceived = 19, // 등록 응답 수신 완료
    L3_event_queueInfoReceived = 20       // 큐 정보 수신 완료
} L3_event_e;

void L3_event_setEventFlag(L3_event_e event);
void L3_event_clearEventFlag(L3_event_e event);
void L3_event_clearAllEventFlag(void);
int  L3_event_checkEventFlag(L3_event_e event);
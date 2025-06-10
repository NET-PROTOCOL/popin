// L3_FSMmain.cpp
#include "L3_FSMevent.h"
#include "L3_msg.h"
#include "L3_types.h"
#include "L3_timer.h"
#include "L3_LLinterface.h"
#include "protocol_parameters.h"
#include "mbed.h"

// FSM 상태 정의
#define L3STATE_SCANNING 0
#define L3STATE_CONNECTED 1
#define L3STATE_WAITING 2
#define L3STATE_IN_USE 3

// 세션 타이머 설정
#define SESSION_DURATION_MS 100000    // 세션 당 100초 (변경 가능)
#define TIMER_CHECK_INTERVAL 1000000 // 1초마다 타이머 확인
#define QUEUE_READY_TIMEOUT_MS 10000 // 큐 준비 응답 대기 10초

// 상태 변수
static uint8_t main_state = L3STATE_SCANNING;  // 현재 FSM 상태
static uint8_t prev_state = main_state;       // 이전 FSM 상태 (디버깅 용)

// 사용자 및 부스 관리 (대기열 중복 메시지 필터링을 위한 변수)
static uint8_t lastQueuePosition = 0;  // 마지막 받은 대기열 위치
static uint8_t lastTotalWaiting = 0;   // 마지막 받은 총 대기 사용자 수

// User and Booth Management
static uint8_t myId;                  // 현재 사용자 또는 관리자의 ID
static uint8_t currentBoothId = 0;    // 현재 연결된 부스 ID (0 : 미연결)
static uint8_t isAdmin = 0;           // 모드 구분 (1: 관리자, 0: 일반 사용자)
static Booth_t myBooth;               // 관리자용 부스 정보 구조체
static uint32_t scanningTimer = 0;    // 관리자 부스 방송 주기 타이머
static uint8_t waitingNumber = 0;     // 사용자 대기열 번호
static uint8_t registeredCount = 0;   // 총 등록된 사용자 수 (관리자 측)
static uint8_t quietMode = 0;         // 관리자 방송 최소화 모드 (ON: 자동 방송 중지)

// 세션 타이머 관련 변수
static uint32_t sessionTimerCounter = 0; // 관리자 세션 타이머 체크 카운터
static uint32_t sessionStartTime = 0;    // 사용자 세션 시작 시간 기록 (ms)
static uint8_t isSessionActive = 0;      // 사용자 측 세션 활성 상태 플래그

// 대기열 관리 변수
static uint8_t pendingUserId = 0;        // 큐 준비 응답을 기다리는 사용자 ID
static uint32_t queueReadyStartTime = 0; // 큐 준비 타이머 시작 시각 (ms)
static uint8_t isQueueReadyTimerActive = 0; // 큐 준비 타이머 활성화 여부
static uint8_t myWaitingNumber = 0;      // 사용자 대기열 순번 (사용자 측)
static uint8_t totalWaitingUsers = 0;    // 대기 중인 총 사용자 수 (사용자 측)

static uint8_t connectRetryCount = 0;    // 연결 재시도 카운터 (사용자 측)
static uint32_t connectRequestTime = 0;  // 연결 요청 시각 기록 (ms)
static uint8_t isWaitingForBoothInfo = 0; // 부스 정보 응답 대기 중인지 플래그

// 등록 요청 관련 변수 추가
static uint8_t registerRetryCount = 0;    // 등록 재시도 카운터
static uint32_t registerRequestTime = 0;  // 등록 요청 시각 기록 (ms)
static uint8_t isWaitingForRegisterResponse = 0; // 등록 응답 대기 중인지 플래그

#define CONNECT_RETRY_MAX 3               // 부스 연결 재시도 최대 횟수
#define CONNECT_TIMEOUT_MS 3000           // 부스 연결 응답 타임아웃 (3초)
#define REGISTER_RETRY_MAX 3              // 등록 재시도 최대 횟수
#define REGISTER_TIMEOUT_MS 3000          // 등록 응답 타임아웃 (3초)

// RSSI 기반 부스 스캔
static BoothScanInfo_t scannedBooths[MAX_BOOTHS]; // 스캔된 부스 정보 리스트
static uint8_t isScanning = 0;                    // 스캔 중 여부 플래그
static uint8_t scanResponseCount = 0;              // 스캔 응답 받은 개수

// 채팅 기능 관련 변수 추가
static uint8_t isTypingChat = 0;          // 채팅 입력 중 여부
static char chatBuffer[101];              // 채팅 메시지 버퍼 (최대 100자)
static uint8_t chatIndex = 0;             // 채팅 버퍼 인덱스

// 메시지 버퍼
static uint8_t txBuffer[L3_MAXDATASIZE];
static uint8_t rxBuffer[L3_MAXDATASIZE];

// 시리얼 포트 인터페이스
static Serial pc(USBTX, USBRX);

// 함수 프로토타입 (코드 본문에서 자세히 설명)
static void sendMessage(uint8_t msgType, uint8_t *data, uint8_t dataLen, uint8_t destId);
static void handleConnectRequest(uint8_t srcId);
static void handleBoothInfo(uint8_t *data, uint8_t size);
static void handleRegisterResponse(uint8_t *data);
static void handleQueueInfo(uint8_t *data);
static uint8_t checkUserInList(User_t *list, uint8_t listSize, uint8_t userId);
static uint8_t checkUserInRegisteredList(uint8_t userId);
static uint8_t addUserToList(User_t *list, uint8_t *listSize, uint8_t userId);
static void removeUserFromList(User_t *list, uint8_t *count, uint8_t userId);
static void removeUserFromActiveList(uint8_t userId);
static void displayBoothInfo(void);
static void scanForBooths(void);
static void L3service_processKeyboardInput(void);
static void L3admin_processKeyboardInput(void);      // 관리자 키보드 입력 처리 함수
static void startSessionTimer(uint8_t userId);       // 사용자 세션 타이머 시작
static void checkSessionTimer(void);                 // 관리자 측 세션 타이머 확인
static void endUserSession(uint8_t userId);          // 사용자 세션 종료 처리
static void admitNextWaitingUser(void);              // 다음 대기 사용자 입장 알림
static void removeFromWaitingQueue(uint8_t userId);  // 대기 큐에서 사용자 제거
static void updateAllWaitingUsers(void);             // 모든 대기 사용자에게 순번 업데이트
static void checkQueueReadyTimeout(void);            // 큐 준비 시간 초과 확인
static void handleQueueReadyTimeout(uint8_t userId); // 큐 준비 시간 초과 처리
static void handleChatMessage(uint8_t srcId, char* message);  // 채팅 메시지 처리
static void broadcastChatToActiveUsers(uint8_t senderId, const char* message); // 채팅 브로드캐스트

// RSSI 기반 선택 함수 프로토타입
static void initializeBoothScanList(void);
static void updateBoothScanInfo(uint8_t boothId, int16_t rssi, uint8_t currentCount,
                                uint8_t capacity, uint8_t waitingCount);
static uint8_t selectOptimalBooth(void);
static void displayScannedBooths(void);

// Initialize FSM
void L3_initFSM(uint8_t id)
{
    myId = id;
    isAdmin = (id >= ADMIN_ID_START && id <= ADMIN_ID_END) ? 1 : 0; // ID 범위 내면 관리자 모드

    // 부스 스캔 목록 초기화 (RSSI 스캔 데이터 모두 초기화)
    initializeBoothScanList();

    if (isAdmin)
    {
        myBooth.boothId = id;
        myBooth.capacity = MAX_BOOTH_CAPACITY; // 최대 수용 인원 설정
        myBooth.currentCount = 0;              // 현재 부스 이용자 수 초기화
        myBooth.waitingCount = 0;              // 부스 대기열 인원 초기화
        sprintf(myBooth.description, "Booth %d - Pop-up Store Experience", id);

        // 등록된 사용자 ID 배열 초기화
        for (uint8_t i = 0; i < MAX_USERS; i++)
        {
            myBooth.registeredUserIds[i] = 0;          // 등록 목록 초기화
            myBooth.activeList[i].sessionStartTime = 0; // 활성 목록 세션 시간 초기화
        }

        pc.printf("\n=== ADMIN MODE: Booth %d ===\n", id);
        pc.printf("Booth Description: %s\n", myBooth.description);
        pc.printf("Max Capacity: %d\n", myBooth.capacity);
        pc.printf("Session Duration: %d seconds\n", SESSION_DURATION_MS / 1000);
        pc.printf("\nAdmin Commands:\n");
        pc.printf("  'm' - Send custom broadcast message\n");
        pc.printf("  's' - Show booth status\n");
        pc.printf("  'a' - Send announcement\n");
        pc.printf("  'q' - Toggle quiet mode (reduce auto broadcasts)\n");
        pc.printf("  't' - Show session timers\n"); // 세션 타이머 상태 확인
        pc.printf("  'w' - Show waiting queue\n");  // 대기 큐 상태 확인
        pc.printf("  'h' - Show help\n\n");

        main_state = L3STATE_IN_USE; // 관리자는 항상 IN_USE 상태에서 대기

        // 키보드 인터럽트 설정: 관리자가 명령 입력 시 처리
        pc.attach(&L3admin_processKeyboardInput, Serial::RxIrq);

        // 초기 부스 방송 전송 (BROADCAST_ID를 통해 전체 사용자에게)
        uint8_t announceData[10];
        uint8_t msgSize = L3_msg_encodeBoothAnnounce(announceData, myBooth.boothId,
                                                     myBooth.currentCount, myBooth.capacity,
                                                     myBooth.waitingCount);

        pc.printf("Booth initialized. Waiting for users...\n");
        pc.printf("Sending initial broadcast...\n");
        L3_LLI_dataReqFunc(announceData, msgSize, BROADCAST_ID);
    }
    else
    {
        pc.printf("\n=== USER MODE: ID %d ===\n", id);
        pc.printf("Starting RSSI-based booth scanning...\n");
        pc.printf("Press 'e' to exit booth when inside\n");
        pc.printf("Press 'c' to chat when inside booth\n");  // 채팅 안내 추가
        pc.printf("Session limit: %d seconds per booth\n", SESSION_DURATION_MS / 1000);
        main_state = L3STATE_SCANNING; // 초기 상태: SCANNING

        // 키보드 인터럽트 설정: 사용자가 'e'를 눌러 부스에서 나갈 수 있도록
        pc.attach(&L3service_processKeyboardInput, Serial::RxIrq);

        // 초기 스캔 시작: 부스 탐색 요청 전송
        pc.printf("Initiating booth discovery...\n");
        isScanning = 1;
        scanResponseCount = 0;

        // 관리자로 설정된 모든 ID 범위에 스캔 메시지 전송
        for (uint8_t i = ADMIN_ID_START; i <= ADMIN_ID_END; i++)
        {
            uint8_t scanMsg[1] = {MSG_TYPE_BOOTH_SCAN};
            L3_LLI_dataReqFunc(scanMsg, 1, i);
        }

        // 부스 선택 타이머 시작 (일정 시간 후 가장 좋은 부스 선택)
        L3_timer_boothSelectionStart();
    }

    L3_event_clearAllEventFlag(); // 이벤트 플래그 초기화
}

// Main FSM Run
void L3_FSMrun(void)
{
    if (prev_state != main_state)
    {
        // 상태 전이 로그 (디버그용)
        debug_if(DBGMSG_L3, "[L3] State transition from %i to %i\n", prev_state, main_state);
        prev_state = main_state;
    }

    // 관리자 측 세션 타이머 및 대기 큐 타이머 주기적으로 확인
    if (isAdmin)
    {
        sessionTimerCounter++;
        if (sessionTimerCounter > TIMER_CHECK_INTERVAL)
        {
            sessionTimerCounter = 0;
            checkSessionTimer(); // 활성 사용자 세션 시간 체크

            // 대기 큐 준비 시간 초과 확인
            if (isQueueReadyTimerActive)
            {
                checkQueueReadyTimeout();
            }
        }
    }

    // 관리자 자동 부스 방송 (quietMode가 꺼져 있으면)
    if (isAdmin && !quietMode)
    { // quietMode가 OFF인 경우
        scanningTimer++;
        if (scanningTimer > 5000000)
        { // 기존 1초(1000000)에서 5초(5000000)로 느리게 설정
            scanningTimer = 0;
            uint8_t announceData[10];
            uint8_t msgSize = L3_msg_encodeBoothAnnounce(announceData, myBooth.boothId,
                                                         myBooth.currentCount, myBooth.capacity,
                                                         myBooth.waitingCount);

            pc.printf("\n[Admin] Broadcasting booth info (Users: %d/%d)...\n",
                      myBooth.currentCount, myBooth.capacity);
            L3_LLI_dataReqFunc(announceData, msgSize, BROADCAST_ID);
        }
    }

    // 부스 선택 타임아웃 처리 (사용자 측)
    if (L3_event_checkEventFlag(L3_event_boothSelectionTimeout))
    {
        if (isScanning && !isAdmin)
        {
            isScanning = 0;
            pc.printf("\n[User] Booth selection timeout. Processing RSSI data...\n");

            // 스캔된 모든 부스 정보 출력 (디버그)
            displayScannedBooths();

            // RSSI를 기반으로 최적 부스 선택
            uint8_t selectedBoothId = selectOptimalBooth();

            if (selectedBoothId != 0)
            {
                currentBoothId = selectedBoothId;
                pc.printf("\nSelected Booth %d as optimal choice\n", currentBoothId);
                pc.printf("Connecting to Booth %d...\n", currentBoothId);

                // 연결 시도 상태 초기화 (재시도 카운터, 대기 플래그)
                connectRetryCount = 0;
                isWaitingForBoothInfo = 1;
                connectRequestTime = us_ticker_read() / 1000; // 연결 요청 시각 기록

                sendMessage(MSG_TYPE_CONNECT_REQUEST, NULL, 0, currentBoothId);
                main_state = L3STATE_CONNECTED; // 상태 전이: CONNECTED
            }
            else
            {
                pc.printf("\nNo booths found. Continuing scan...\n");
                // 스캔 목록 초기화 후 재스캔
                initializeBoothScanList();
            }
        }
        L3_event_clearEventFlag(L3_event_boothSelectionTimeout); // 플래그 클리어
    }

    // 수신된 메시지 처리
    if (L3_event_checkEventFlag(L3_event_msgRcvd))
    {
        uint8_t *dataPtr = L3_LLI_getMsgPtr(); // 메시지 데이터 포인터
        uint8_t size = L3_LLI_getSize();      // 메시지 길이
        uint8_t srcId = L3_LLI_getSrcId();    // 발신자 ID
        uint8_t msgType = L3_msg_getType(dataPtr); // 메시지 타입
        int16_t rssi = L3_LLI_getRssi();   // RSSI 값 (dBm)

        // 모든 메시지 수신 로그 출력 (관리자 또는 일반 사용자)
        if (!isAdmin || msgType != MSG_TYPE_BOOTH_ANNOUNCE)
        {
            pc.printf("\n[%s] RX: Type=0x%02X, Src=%d, Size=%d, RSSI=%d\n",
                      isAdmin ? "Admin" : "User", msgType, srcId, size, rssi);
        }

        // 스캔 중 아닌 상태에서 부스 방송 메시지 필터링 (일반 사용자)
        if (msgType == MSG_TYPE_BOOTH_ANNOUNCE && !isAdmin && main_state != L3STATE_SCANNING)
        {
            L3_event_clearEventFlag(L3_event_msgRcvd);
            return;
        }

        // 주기적 메시지가 아닌 경우 디버그 출력
        if (msgType != MSG_TYPE_BOOTH_ANNOUNCE || !isAdmin)
        {
            pc.printf("[DEBUG] Received message type 0x%02X from ID %d (RSSI: %d dBm, size: %d)\n",
                      msgType, srcId, rssi, size);
        }

        switch (msgType)
        {
        case MSG_TYPE_CHAT_MESSAGE: // 채팅 메시지 처리
            if (!isAdmin && main_state == L3STATE_IN_USE)
            {
                // 사용자가 부스 내에서 채팅 메시지 수신
                uint8_t *msgData = L3_msg_getData(dataPtr);
                uint8_t originalSenderId;
                char *chatMsg;
                
                // 메시지 포맷 확인 (첫 바이트가 유효한 사용자 ID인지)
                if (msgData[0] >= 4 && msgData[0] <= 254) {
                    // Admin이 중계한 메시지 (발신자 ID 포함)
                    originalSenderId = msgData[0];
                    chatMsg = (char*)(msgData + 1);
                } else {
                    // 직접 수신한 메시지 (구 형식)
                    originalSenderId = srcId;
                    chatMsg = (char*)msgData;
                }
                
                // 채팅 입력 중이면 줄바꿈 후 메시지 표시
                if (isTypingChat)
                {
                    pc.printf("\n");
                }
                
                // 자신이 보낸 메시지는 이미 표시했으므로 스킵
                if (originalSenderId != myId)
                {
                    pc.printf("[CHAT] User %d: %s\n", originalSenderId, chatMsg);
                }
                
                // 채팅 입력 중이었다면 프롬프트 다시 표시
                if (isTypingChat)
                {
                    pc.printf("Continue typing: ");
                    // 현재까지 입력한 내용 다시 표시
                    for (uint8_t i = 0; i < chatIndex; i++)
                    {
                        pc.putc(chatBuffer[i]);
                    }
                }
            }
            else if (isAdmin)
            {
                // 관리자가 채팅 메시지 수신 및 브로드캐스트
                char *chatMsg = (char *)L3_msg_getData(dataPtr);
                pc.printf("\n[CHAT] User %d: %s\n", srcId, chatMsg);
                
                // 해당 사용자가 activeList에 있는지 확인
                if (checkUserInList(myBooth.activeList, myBooth.currentCount, srcId))
                {
                    // 같은 부스의 다른 활성 사용자들에게 브로드캐스트
                    broadcastChatToActiveUsers(srcId, chatMsg);
                }
                else
                {
                    pc.printf("[Admin] Warning: User %d not in active list, chat rejected\n", srcId);
                }
            }
            break;

        case MSG_TYPE_TIMEOUT_ALERT: // 타이머 만료 알림 처리 (사용자 측)
            if (!isAdmin)
            {
                pc.printf("\n[ALERT] Your session time has expired!\n");
                pc.printf("You are being automatically exited from the booth.\n");
                pc.printf("Thank you for your experience!\n");

                // 세션 만료 시 스캔 상태로 복귀 (타임아웃 알림 받으면 자동으로 나감)
                main_state = L3STATE_SCANNING;
                currentBoothId = 0;
                isSessionActive = 0;
                sessionStartTime = 0;

                // 새로운 스캔 준비
                initializeBoothScanList();
                pc.printf("\nReturning to booth scanning mode...\n");
            }
            break;

        case MSG_TYPE_BOOTH_SCAN: // 부스 스캔 요청 수신 (관리자 측)
            if (isAdmin)
            {
                pc.printf("\n[Admin] Received scan request from User %d (RSSI: %d dBm)\n", srcId, rssi);
                pc.printf("[Admin] Responding to scan request...\n");

                // 부스 정보(현재 이용자 수, 정원, 대기 인원) 응답 전송
                uint8_t announceData[10];
                uint8_t msgSize = L3_msg_encodeBoothAnnounce(announceData, myBooth.boothId,
                                                             myBooth.currentCount, myBooth.capacity,
                                                             myBooth.waitingCount);
                L3_LLI_dataReqFunc(announceData, msgSize, srcId);

                pc.printf("[Admin] Sent booth announce to User %d\n", srcId);
            }
            break;

        case MSG_TYPE_USER_RESPONSE: // 사용자가 부스 체험 여부 응답 (관리자 측)
            if (isAdmin)
            {
                uint8_t *msgData = L3_msg_getData(dataPtr);
                uint8_t response = msgData[0];

                pc.printf("\n[Admin] Received USER_RESPONSE from User %d: %s\n", 
                         srcId, response == USER_RESPONSE_YES ? "YES" : "NO");

                if (response == USER_RESPONSE_YES)
                {
                    pc.printf("[Admin] User %d wants to experience the booth\n", srcId);
                    // 이어서 REGISTER_REQUEST 메시지를 수신할 예정
                }
                else
                {
                    // 사용자가 NO 응답 시 (큐 준비 대상이었으면 대기 큐에서 제거)
                    if (pendingUserId == srcId && isQueueReadyTimerActive)
                    {
                        pc.printf("[Admin] Queue ready response received from User %d\n", srcId);
                        // 큐 준비 타이머 중지
                        isQueueReadyTimerActive = 0;
                        pendingUserId = 0;

                        // 대기 큐에서 제거 및 남은 사용자 업데이트
                        removeFromWaitingQueue(srcId);
                        if (myBooth.waitingCount > 0)
                        {
                            updateAllWaitingUsers();
                        }
                    }
                    pc.printf("\n[Admin] User %d declined booth experience\n", srcId);
                }
            }
            break;

        case MSG_TYPE_ADMIN_MESSAGE: // 관리자 방송 메시지 수신 (사용자 측)
            if (!isAdmin)
            {
                char *adminMsg = (char *)L3_msg_getData(dataPtr);
                pc.printf("\n[ADMIN BROADCAST] %s\n", adminMsg);
            }
            break;

        case MSG_TYPE_QUEUE_LEAVE: // 사용자가 대기열 탈퇴 요청 (관리자 측)
            if (isAdmin)
            {
                pc.printf("\n[Admin] User %d leaving waiting queue\n", srcId);

                // 대기 큐에서 해당 사용자 제거
                removeFromWaitingQueue(srcId);

                // 남은 대기 사용자 순번 업데이트
                updateAllWaitingUsers();

                pc.printf("[Admin] Updated waiting queue. Remaining: %d users\n", myBooth.waitingCount);
            }
            break;

        case MSG_TYPE_QUEUE_READY: // 대기 사용자에게 입장 알림 (사용자 측)
            if (!isAdmin && main_state == L3STATE_WAITING)
            {
                pc.printf("\n[Alert] Your turn has arrived!\n");
                pc.printf("You have %d seconds to enter the booth.\n", QUEUE_READY_TIMEOUT_MS / 1000);
                pc.printf("Attempting to enter booth...\n");

                // 자동으로 REGISTER_REQUEST 전송 (사용자가 입장 의사 표시)
                sendMessage(MSG_TYPE_REGISTER_REQUEST, NULL, 0, currentBoothId);
            }
            else if (!isAdmin)
            {
                pc.printf("\n[DEBUG] Received QUEUE_READY but not in WAITING state (current state: %d)\n", main_state);
            }
            break;

        case MSG_TYPE_QUEUE_UPDATE: // 대기열 위치/총 대기 사용자 수 업데이트 (사용자 측)
            if (!isAdmin && main_state == L3STATE_WAITING)
            {
                uint8_t *msgData = L3_msg_getData(dataPtr);
                myWaitingNumber = msgData[0];
                totalWaitingUsers = msgData[1];

                // 대기열에서 제거된 경우: 순번 0 또는 총 대기 인원 0
                if (myWaitingNumber == 0 || totalWaitingUsers == 0)
                {
                    pc.printf("\n[Notice] You have been removed from the waiting queue.\n");
                    pc.printf("Returning to scanning mode...\n");
                    main_state = L3STATE_SCANNING;
                    currentBoothId = 0;
                    initializeBoothScanList(); // 스캔 목록 초기화
                }
                else
                {
                    pc.printf("\n[Queue Update] Your position: %d/%d\n", myWaitingNumber, totalWaitingUsers);
                }
            }
            break;

        case MSG_TYPE_EXIT_REQUEST: // 사용자가 부스 퇴장 요청 (관리자 측)
            if (isAdmin)
            {
                pc.printf("\n[Admin] Received exit request from User %d\n", srcId);

                // 사용자가 activeList에 있는지 확인
                if (checkUserInList(myBooth.activeList, myBooth.currentCount, srcId))
                {
                    // 사용자 세션 종료 처리
                    endUserSession(srcId);

                    // EXIT_RESPONSE 전송 (성공)
                    uint8_t exitResp[2];
                    exitResp[0] = MSG_TYPE_EXIT_RESPONSE;
                    exitResp[1] = 1; // 성공
                    L3_LLI_dataReqFunc(exitResp, 2, srcId);

                    // 다음 대기 사용자 입장 시도
                    admitNextWaitingUser();
                }
                else
                {
                    pc.printf("[Admin] Warning: User %d not in active list\n", srcId);
                }
            }
            break;

        case MSG_TYPE_EXIT_RESPONSE: // 부스 퇴장 확인 응답 (사용자 측)
            if (!isAdmin)
            {
                pc.printf("\n[User] Exit confirmed by booth.\n");
            }
            break;

        case MSG_TYPE_BOOTH_ANNOUNCE: // 부스 방송 메시지 수신 (사용자 측)
            if (!isAdmin && main_state == L3STATE_SCANNING && isScanning)
            {
                uint8_t *msgData = L3_msg_getData(dataPtr);
                uint8_t boothId = msgData[0];
                uint8_t currentUsers = msgData[1];
                uint8_t capacity = msgData[2];
                uint8_t waitingUsers = msgData[3];

                pc.printf("\nBooth %d detected! RSSI: %d dBm (Users: %d/%d, Waiting: %d)\n",
                          boothId, rssi, currentUsers, capacity, waitingUsers);

                // 스캔 목록에 RSSI 정보 업데이트
                updateBoothScanInfo(boothId, rssi, currentUsers, capacity, waitingUsers);
                scanResponseCount++;
            }
            break;

        case MSG_TYPE_CONNECT_REQUEST: // 부스 연결 요청 수신 (관리자 측)
            if (isAdmin)
            {
                pc.printf("\n[Admin] Received connect request from User %d\n", srcId);
                pc.printf("[Admin] Processing connection request...\n");

                // 즉시 handleConnectRequest 호출하여 부스 정보 전송
                handleConnectRequest(srcId);

                // 전송 확인 로그
                pc.printf("[Admin] Booth info sent to User %d successfully\n", srcId);
            }
            break;

        case MSG_TYPE_BOOTH_INFO: // 부스 정보 수신 (사용자 측, CONNECTED 상태)
            if (!isAdmin && main_state == L3STATE_CONNECTED)
            {
                pc.printf("\n[User] Received booth info from Booth %d\n", srcId);

                // 연결 성공 처리: 정보 플래그 초기화 및 재시도 카운터 초기화
                isWaitingForBoothInfo = 0;
                connectRetryCount = 0;

                handleBoothInfo(L3_msg_getData(dataPtr), size - 1);
            }
            break;

        case MSG_TYPE_REGISTER_REQUEST: // 사용자 등록 요청 수신 (관리자 측)
            if (isAdmin)
            {
                pc.printf("\n[Admin] Received registration request from User %d\n", srcId);
                pc.printf("[DEBUG] Checking registration status...\n");

                uint8_t response[3];
                uint8_t msgSize;

                // 이미 등록된 사용자인지 확인 (C2 조건)
                uint8_t isRegistered = checkUserInRegisteredList(srcId);

                if (isRegistered)
                {
                    // !C2: 이미 등록됨 -> 거부
                    msgSize = L3_msg_encodeRegisterResponse(response, 0, REGISTER_REASON_ALREADY_USED);
                    pc.printf("User %d registration rejected - already experienced this booth!\n", srcId);
                    L3_LLI_dataReqFunc(response, msgSize, srcId);
                }
                else if (myBooth.currentCount >= myBooth.capacity)
                {
                    // C1: 현재 입장 인원 초과 (만원) -> 대기 큐 등록
                    if (!checkUserInList(myBooth.waitingQueue, myBooth.waitingCount, srcId))
                    {   
                        // 대기 큐에 사용자 추가 (C3 만족)
                        addUserToList(myBooth.waitingQueue, &myBooth.waitingCount, srcId);
                        myBooth.waitingQueue[myBooth.waitingCount - 1].waitingNumber = myBooth.waitingCount;
                        
                        // REGISTER_RESPONSE(대기 큐) 메시지 먼저 전송
                        msgSize = L3_msg_encodeRegisterResponse(response, 0, REGISTER_REASON_FULL_WAITING);
                        L3_LLI_dataReqFunc(response, msgSize, srcId);
                        pc.printf("User %d registration response sent (waiting queue)\n", srcId);
                        
                        // 약간의 지연 후 QUEUE_INFO 전송
                        wait_ms(100);
                        
                        // QUEUE_INFO 메시지 전송 (대기 순번, 총 대기 인원)
                        uint8_t queueData[3];
                        uint8_t queueMsgSize = L3_msg_encodeQueueInfo(queueData, 
                                                                    myBooth.waitingCount,  
                                                                    myBooth.waitingCount);
                        L3_LLI_dataReqFunc(queueData, queueMsgSize, srcId);
                        
                        pc.printf("User %d added to waiting queue (position: %d/%d)\n", 
                                srcId, myBooth.waitingCount, myBooth.waitingCount); //position:대기 순번/총 대기 인원
                    }
                    else
                    {   // 이미 대기열에 있는 사용자가 다시 등록 요청을 보내면 -> 재등록 요청 무시 (!C3)
                        pc.printf("[Admin] User %d already in waiting queue, ignoring duplicate request\n", srcId);
                        // ** 네트워크 재전송, 타이밍 문제로 인해 추가함 **
                    }
                }
                else
                {
                    // !C1 & C2: 정상 등록 가능 (빈 자리 있음, 등록 목록에 없음)
                    // 대기 큐 준비 중인 사용자인지 확인 후 처리
                    if (pendingUserId == srcId && isQueueReadyTimerActive)
                    {
                        pc.printf("[Admin] Queue ready response received from User %d\n", srcId);
                        // 큐 준비 타이머 중지
                        isQueueReadyTimerActive = 0;
                        pendingUserId = 0;

                        // 대기 큐에서 제거 및 남은 사용자 업데이트
                        removeFromWaitingQueue(srcId);
                        if (myBooth.waitingCount > 0)
                        {
                            updateAllWaitingUsers();
                        }
                    }
                    // registeredList에 사용자 추가
                    myBooth.registeredUserIds[registeredCount] = srcId;
                    myBooth.registeredList[registeredCount].userId = srcId;
                    myBooth.registeredList[registeredCount].isRegistered = 1;
                    registeredCount++;

                    // activeList에 사용자 추가 및 세션 타이머 시작
                    addUserToList(myBooth.activeList, &myBooth.currentCount, srcId);
                    startSessionTimer(srcId); // 세션 시작 시간 기록

                    msgSize = L3_msg_encodeRegisterResponse(response, 1, REGISTER_REASON_SUCCESS);
                    pc.printf("User %d successfully registered and entered booth\n", srcId);
                    pc.printf("Session timer started (%d seconds)\n", SESSION_DURATION_MS / 1000);
                    pc.printf("Current booth status: %d/%d users (Total registered: %d)\n",
                              myBooth.currentCount, myBooth.capacity, registeredCount);

                    L3_LLI_dataReqFunc(response, msgSize, srcId);
                }
            }
            break;

        case MSG_TYPE_REGISTER_RESPONSE: // 등록 응답 수신 (사용자 측)
            if (!isAdmin)
            {
                pc.printf("\n[User] Received REGISTER_RESPONSE\n");
                
                // 등록 응답 대기 플래그 해제
                isWaitingForRegisterResponse = 0;
                registerRetryCount = 0;
                
                handleRegisterResponse(L3_msg_getData(dataPtr));
            }
            break;

        case MSG_TYPE_QUEUE_INFO: // 대기열 정보 수신 (사용자 측)
            if (!isAdmin)
            {
                uint8_t *msgData = L3_msg_getData(dataPtr);
                uint8_t newPosition = msgData[0];
                uint8_t newTotal = msgData[1];

                // 중복 메시지 필터링
                if (newPosition != lastQueuePosition || newTotal != lastTotalWaiting)
                {
                    lastQueuePosition = newPosition;
                    lastTotalWaiting = newTotal;
                    
                    // handleQueueInfo가 myWaitingNumber와 totalWaitingUsers를 설정함
                    handleQueueInfo(msgData);
                    
                    // ** QUEUE_INFO만 받고 REGISTER_RESPONSE를 못받은 경우 대비 **
                    // QUEUE_INFO를 받았다는 것은 대기열에 있다는 것을 의미함
                    // 어떤 상태에서든 WAITING으로 전환
                    if (main_state == L3STATE_CONNECTED) {
                        pc.printf("[DEBUG] Transitioning to WAITING state due to QUEUE_INFO\n");
                        main_state = L3STATE_WAITING;
                    }
                }
            }
            break;
        }

        L3_event_clearEventFlag(L3_event_msgRcvd); // 메시지 수신 플래그 초기화
    }

    // User-specific state machine: 일반 사용자 모드
    if (!isAdmin)
    {
        static uint32_t userScanTimer = 0;
        static uint32_t sessionDisplayTimer = 0;
        static uint32_t connectTimeoutTimer = 0;
        static uint32_t registerTimeoutTimer = 0;

        switch (main_state)
        {
        case L3STATE_SCANNING:
            // 주기적 스캔: 이미 스캔 중이 아니면 주기마다 재스캔 수행
            userScanTimer++;
            if (userScanTimer > 800000 && !isScanning)
            { // 일정 주기마다 스캔 재시작 (디버깅용 카운터 설정)
                userScanTimer = 0;
                pc.printf("\n[User] Starting new RSSI-based scan cycle...\n");

                // 스캔 목록 초기화 및 RSSI 스캔 재시작
                initializeBoothScanList();
                isScanning = 1;
                scanResponseCount = 0;

                // 모든 관리 부스 ID로 스캔 요청 전송
                for (uint8_t i = ADMIN_ID_START; i <= ADMIN_ID_END; i++)
                {
                    sendMessage(MSG_TYPE_BOOTH_SCAN, NULL, 0, i);
                }

                // 부스 선택 타이머 재시작
                L3_timer_boothSelectionStart();
            }
            break;

        case L3STATE_CONNECTED:
            // BOOTH_INFO 응답을 기다리는 중 타임아웃 처리
            if (isWaitingForBoothInfo)
            {
                connectTimeoutTimer++;
                if (connectTimeoutTimer > 3000000)
                { // 3초 타임아웃
                    connectTimeoutTimer = 0;

                    if (connectRetryCount < CONNECT_RETRY_MAX)
                    {
                        connectRetryCount++;
                        pc.printf("\n[User] No response from booth. Retrying... (%d/%d)\n",
                                  connectRetryCount, CONNECT_RETRY_MAX);

                        // 재전송 요청
                        sendMessage(MSG_TYPE_CONNECT_REQUEST, NULL, 0, currentBoothId);
                    }
                    else
                    {
                        pc.printf("\n[User] Failed to connect to booth after %d attempts.\n",
                                  CONNECT_RETRY_MAX);
                        pc.printf("Returning to scanning mode...\n");

                        // 초기화 후 스캔 모드로 복귀
                        main_state = L3STATE_SCANNING;
                        currentBoothId = 0;
                        connectRetryCount = 0;
                        isWaitingForBoothInfo = 0;
                        initializeBoothScanList();
                    }
                }
            }
            
            // REGISTER_RESPONSE 응답을 기다리는 중 타임아웃 처리
            if (isWaitingForRegisterResponse)
            {
                registerTimeoutTimer++;
                if (registerTimeoutTimer > 3000000)
                { // 3초 타임아웃
                    registerTimeoutTimer = 0;

                    if (registerRetryCount < REGISTER_RETRY_MAX)
                    {
                        registerRetryCount++;
                        pc.printf("\n[User] No registration response. Retrying... (%d/%d)\n",
                                  registerRetryCount, REGISTER_RETRY_MAX);

                        // USER_RESPONSE와 REGISTER_REQUEST 재전송
                        uint8_t responseMsg[2];
                        L3_msg_encodeUserResponse(responseMsg, USER_RESPONSE_YES);
                        L3_LLI_dataReqFunc(responseMsg, 2, currentBoothId);
                        
                        wait_ms(100); // 짧은 대기
                        
                        sendMessage(MSG_TYPE_REGISTER_REQUEST, NULL, 0, currentBoothId);
                        registerRequestTime = us_ticker_read() / 1000;
                    }
                    else
                    {
                        pc.printf("\n[User] Failed to register after %d attempts.\n",
                                  REGISTER_RETRY_MAX);
                        pc.printf("Returning to scanning mode...\n");

                        // 초기화 후 스캔 모드로 복귀
                        main_state = L3STATE_SCANNING;
                        currentBoothId = 0;
                        registerRetryCount = 0;
                        isWaitingForRegisterResponse = 0;
                        initializeBoothScanList();
                    }
                }
            }
            break;

        case L3STATE_WAITING:
            // 대기열 상태 표시
            static uint32_t waitingDotTimer = 0;
            static uint32_t waitingAnimTimer = 0;
            static uint8_t animIndex = 0;

            waitingAnimTimer++;
            if (waitingAnimTimer > 1000000)
            {
                waitingAnimTimer = 0;
                pc.printf("\r Waiting... Position: %d/%d   ",
                          myWaitingNumber, totalWaitingUsers);
            }
            break;

        case L3STATE_IN_USE:
            // 부스 사용 중 남은 시간 주기적 표시
            sessionDisplayTimer++;
            if (sessionDisplayTimer > 5000000 && isSessionActive)
            { // 5초마다 남은 시간 표시
                sessionDisplayTimer = 0;
                uint32_t currentTime = us_ticker_read() / 1000;
                uint32_t elapsedTime = currentTime - sessionStartTime;
                
                // ** 남은 시간 계산 시 언더플로우 방지를 위해 elapsedTime < SESSION_DURATION_MS 체크 필요
                if (elapsedTime < SESSION_DURATION_MS)
                {
                    uint32_t remainingTime = (SESSION_DURATION_MS - elapsedTime) / 1000;
                    pc.printf("\n[Info] Time remaining: %d seconds\n", remainingTime);
                    pc.printf("Press 'c' to chat, 'e' to exit\n");  // 채팅 안내 추가
                }
                else
                {
                    // 세션 만료됨(서버 측 타임아웃 알림 대기 중)
                    pc.printf("\n[Info] Session expired. Waiting for server timeout...\n");
                    isSessionActive = 0; // 더 이상 남은 시간 표시 안 함
                }
            }
            // 사용자 측 세션 타이머 체크 (백업 메커니즘) - 기존 시간 계산 재사용
            if (isSessionActive == 0) // 세션이 만료된 상태라면
            {
                uint32_t currentTime = us_ticker_read() / 1000;
                uint32_t elapsedTime = currentTime - sessionStartTime;

                // 세션 시간 + 5초 여유시간 후 강제 전환
                if (elapsedTime > (SESSION_DURATION_MS + 5000))
                {
                    pc.printf("\n[Warning] Session timeout detected on client side!\n");
                    pc.printf("Automatically returning to scanning mode...\n");

                    // 서버 퇴장 요청 전송 (선택 사항)
                    sendMessage(MSG_TYPE_EXIT_REQUEST, NULL, 0, currentBoothId);

                    // 강제로 SCANNING 상태로 전환
                    main_state = L3STATE_SCANNING;
                    currentBoothId = 0;
                    isSessionActive = 0;
                    sessionStartTime = 0;

                    // 스캔 목록 초기화
                    initializeBoothScanList();

                    pc.printf("Returned to scanning mode due to session timeout.\n");
                    pc.printf("[DEBUG] State transition: IN_USE -> SCANNING (client timeout)\n");
                }
            }
            break;
        }
    }
}

// Start session timer for a user (관리자 측)
static void startSessionTimer(uint8_t userId)
{
    // 활성 사용자 목록에서 해당 사용자 찾아 세션 시작 시각 기록
    for (uint8_t i = 0; i < myBooth.currentCount; i++)
    {
        if (myBooth.activeList[i].userId == userId)
        {
            myBooth.activeList[i].sessionStartTime = us_ticker_read() / 1000; // ms 단위 저장
            pc.printf("[Admin] Session timer started for User %d\n", userId);
            break;
        }
    }
}

// Check session timers for all active users (관리자 측)
static void checkSessionTimer(void)
{
    uint32_t currentTime = us_ticker_read() / 1000; // ms 단위

    for (uint8_t i = 0; i < myBooth.currentCount; i++)
    {
        uint32_t sessionTime = currentTime - myBooth.activeList[i].sessionStartTime;

        if (sessionTime >= SESSION_DURATION_MS)
        {
            uint8_t userId = myBooth.activeList[i].userId;
            pc.printf("\n[Admin] Session timeout for User %d!\n", userId);

            // TIMEOUT_ALERT 메시지 전송 (사용자 세션 만료)
            uint8_t timeoutMsg[2];
            timeoutMsg[0] = MSG_TYPE_TIMEOUT_ALERT;
            timeoutMsg[1] = 0; // 예약 필드
            L3_LLI_dataReqFunc(timeoutMsg, 2, userId);

            // 사용자 세션 종료 처리
            endUserSession(userId);

            // 약간의 지연 후 다음 대기 사용자 입장 (메시지 전송 안정성)
            wait_ms(500); // 500ms 지연

            // 다음 대기 사용자 입장 시도
            admitNextWaitingUser();

            // 배열이 변경되었으므로 루프 재시작
            break;
        }
    }
}

// End user session (관리자 측, activeList에서만 제거)
static void endUserSession(uint8_t userId)
{
    // 활성 목록에서 사용자 찾아 세션 지속 시간 계산 (로그용)
    for (uint8_t i = 0; i < myBooth.currentCount; i++)
    {
        if (myBooth.activeList[i].userId == userId)
        {
            uint32_t sessionDuration = (us_ticker_read() / 1000) - myBooth.activeList[i].sessionStartTime;
            pc.printf("[Admin] User %d session ended. Duration: %d seconds\n",
                      userId, sessionDuration / 1000);
            break;
        }
    }

    // 활성 목록에서 사용자 제거 (등록 목록은 유지)
    removeUserFromActiveList(userId);

    pc.printf("[Admin] User %d has exited. Current users: %d/%d\n",
              userId, myBooth.currentCount, myBooth.capacity);
}

// Admit next waiting user (pull 기반 구현, 관리자 측)
static void admitNextWaitingUser(void)
{
    if (myBooth.waitingCount > 0 && !isQueueReadyTimerActive)
    {
        // 대기 큐의 첫 번째 사용자 ID 가져오기 (FIFO)
        uint8_t nextUserId = myBooth.waitingQueue[0].userId;

        pc.printf("\n[Admin] Preparing to notify User %d (waiting queue position 1)\n", nextUserId);
        pc.printf("[Admin] Current waiting queue size: %d\n", myBooth.waitingCount);

        // QUEUE_READY 메시지 전송: 차례가 된 사용자에게 입장 알림
        uint8_t readyMsg[2];
        readyMsg[0] = MSG_TYPE_QUEUE_READY;
        readyMsg[1] = 0; // 예약 필드

        pc.printf("[Admin] Sending QUEUE_READY (0x%02X) to User %d\n", MSG_TYPE_QUEUE_READY, nextUserId);
        L3_LLI_dataReqFunc(readyMsg, 2, nextUserId);

        // 큐 준비 타이머 시작: 일정 시간 안에 응답이 없으면 제거
        pendingUserId = nextUserId;
        queueReadyStartTime = us_ticker_read() / 1000; // ms 단위 저장
        isQueueReadyTimerActive = 1;

        pc.printf("[Admin] Queue ready timer started for User %d (%d seconds timeout)\n",
                  nextUserId, QUEUE_READY_TIMEOUT_MS / 1000);
    }
    else
    {
        pc.printf("[Admin] No users in waiting queue to admit\n");
    }
}

// NEW: Remove user from waiting queue (관리자 측)
static void removeFromWaitingQueue(uint8_t userId)
{
    uint8_t found = 0;

    // 대기 큐에서 사용자 찾기 및 제거
    for (uint8_t i = 0; i < myBooth.waitingCount; i++)
    {
        if (myBooth.waitingQueue[i].userId == userId)
        {
            found = 1;
            // 뒤쪽 사용자 앞으로 한 칸씩 이동
            for (uint8_t j = i; j < myBooth.waitingCount - 1; j++)
            {
                myBooth.waitingQueue[j] = myBooth.waitingQueue[j + 1];
            }
            myBooth.waitingCount--;
            break;
        }
    }

    if (found)
    {
        pc.printf("[Admin] Removed User %d from waiting queue\n", userId);
    }
}

// Update all waiting users with new positions (관리자 측)
static void updateAllWaitingUsers(void)
{
    for (uint8_t i = 0; i < myBooth.waitingCount; i++)
    {
        uint8_t updateMsg[3];
        updateMsg[0] = MSG_TYPE_QUEUE_UPDATE;
        updateMsg[1] = i + 1;                // 1부터 시작하는 순번
        updateMsg[2] = myBooth.waitingCount; // 전체 대기 인원

        L3_LLI_dataReqFunc(updateMsg, 3, myBooth.waitingQueue[i].userId);

        pc.printf("[Admin] Updated User %d: Position %d/%d\n",
                  myBooth.waitingQueue[i].userId, i + 1, myBooth.waitingCount);
    }
}

// Check queue ready timeout (관리자 측)
static void checkQueueReadyTimeout(void)
{
    if (isQueueReadyTimerActive)
    {
        uint32_t currentTime = us_ticker_read() / 1000;
        uint32_t elapsedTime = currentTime - queueReadyStartTime;

        if (elapsedTime >= QUEUE_READY_TIMEOUT_MS)
        {
            handleQueueReadyTimeout(pendingUserId); // 시간 초과 처리 함수 호출
        }
    }
}

// Handle queue ready timeout (관리자 측)
static void handleQueueReadyTimeout(uint8_t userId)
{
    pc.printf("\n[Admin] User %d did not respond in time. Moving to next...\n", userId);

    // 타이머 중지
    isQueueReadyTimerActive = 0;
    pendingUserId = 0;

    // 해당 사용자에게 대기열에서 제거 알림 전송: 위치=0, 총 대기=0
    uint8_t removalMsg[3];
    removalMsg[0] = MSG_TYPE_QUEUE_UPDATE;
    removalMsg[1] = 0; // 순번 0 = 제거됨 표시
    removalMsg[2] = 0; // 총 대기 인원 0 = 제거됨 표시
    L3_LLI_dataReqFunc(removalMsg, 3, userId);

    // 대기 큐에서 사용자 제거
    removeFromWaitingQueue(userId);

    // 남은 대기 사용자 순번 업데이트
    if (myBooth.waitingCount > 0)
    {
        updateAllWaitingUsers();
    }

    // 다음 대기 사용자 입장 시도
    admitNextWaitingUser();
}

// 채팅 메시지를 같은 부스의 활성 사용자들에게 브로드캐스트
static void broadcastChatToActiveUsers(uint8_t senderId, const char* message)
{
    uint8_t chatMsg[110];
    uint8_t msgSize = L3_msg_encodeChatMessageWithSender(chatMsg, senderId, message);
    
    // 같은 부스의 모든 활성 사용자에게 전송 (발신자 제외)
    for (uint8_t i = 0; i < myBooth.currentCount; i++)
    {
        uint8_t targetUserId = myBooth.activeList[i].userId;
        
        // 발신자 제외하고 전송
        if (targetUserId != senderId)
        {
            L3_LLI_dataReqFunc(chatMsg, msgSize, targetUserId);
            pc.printf("[Admin] Forwarded chat to User %d\n", targetUserId);
        }
    }
    
    // 발신자에게도 에코백 (선택사항 - 자신의 메시지 확인용)
    // 주석 처리: 사용자는 이미 자신의 메시지를 봤으므로 불필요
    // L3_LLI_dataReqFunc(chatMsg, msgSize, senderId);
}

// Initialize booth scan list (RSSI 스캔 초기화)
static void initializeBoothScanList(void)
{
    for (uint8_t i = 0; i < MAX_BOOTHS; i++)
    {
        scannedBooths[i].boothId = 0;
        scannedBooths[i].rssi = -127; // 초기 RSSI 최소값 설정
        scannedBooths[i].currentCount = 0;
        scannedBooths[i].capacity = 0;
        scannedBooths[i].waitingCount = 0;
        scannedBooths[i].lastSeenTime = 0;
        scannedBooths[i].isValid = 0; // 유효하지 않은 상태로 초기화
    }
}

// Update booth scan info with RSSI (RSSI 스캔 정보 갱신)
static void updateBoothScanInfo(uint8_t boothId, int16_t rssi, uint8_t currentCount,
                                uint8_t capacity, uint8_t waitingCount)
{
    // 기존 목록에서 해당 부스를 찾거나 새 항목으로 추가
    for (uint8_t i = 0; i < MAX_BOOTHS; i++)
    {
        if (scannedBooths[i].boothId == boothId || !scannedBooths[i].isValid)
        {
            scannedBooths[i].boothId = boothId;
            scannedBooths[i].rssi = rssi;
            scannedBooths[i].currentCount = currentCount;
            scannedBooths[i].capacity = capacity;
            scannedBooths[i].waitingCount = waitingCount;
            scannedBooths[i].isValid = 1; // 유효 플래그 설정
            break;
        }
    }
}

// Select optimal booth based on RSSI and availability (부스 선택 로직)
static uint8_t selectOptimalBooth(void)
{
    uint8_t bestBoothId = 0;
    int16_t bestScore = -1000; // 초기 점수: 매우 낮은 값

    pc.printf("\n[User] Analyzing booth options...\n");

    for (uint8_t i = 0; i < MAX_BOOTHS; i++)
    {
        if (scannedBooths[i].isValid)
        {
            // 점수 계산 로직:
            // 1. RSSI (신호 강도) - 주요 요인
            // 2. 가용 공간 (capacity - currentCount) - 보너스 점수
            // 3. 대기열 크기 (waitingCount) - 페널티 점수

            int16_t score = scannedBooths[i].rssi; // 기본 점수 = RSSI

            // 가용 공간 보너스 부여
            uint8_t availableSpace = scannedBooths[i].capacity - scannedBooths[i].currentCount;
            if (availableSpace > 0)
            {
                score += 20; // 빈 자리가 있을 때 보너스
            }

            // 대기열이 많을수록 점수 차감
            score -= (scannedBooths[i].waitingCount * 10);

            pc.printf("  Booth %d: RSSI=%d, Score=%d (Available=%d/%d, Queue=%d)\n",
                      scannedBooths[i].boothId, scannedBooths[i].rssi, score,
                      availableSpace, scannedBooths[i].capacity,
                      scannedBooths[i].waitingCount);

            if (score > bestScore)
            {
                bestScore = score;
                bestBoothId = scannedBooths[i].boothId;
            }
        }
    }

    return bestBoothId; // 최적 부스 ID 반환 (없으면 0)
}

// Display all scanned booths (디버그용 스캔 결과 출력)
static void displayScannedBooths(void)
{
    pc.printf("\n=== SCANNED BOOTHS (RSSI-based) ===\n");
    uint8_t foundCount = 0;

    for (uint8_t i = 0; i < MAX_BOOTHS; i++)
    {
        if (scannedBooths[i].isValid)
        {
            pc.printf("Booth %d: RSSI=%d dBm, Users=%d/%d, Waiting=%d\n",
                      scannedBooths[i].boothId,
                      scannedBooths[i].rssi,
                      scannedBooths[i].currentCount,
                      scannedBooths[i].capacity,
                      scannedBooths[i].waitingCount);
            foundCount++;
        }
    }

    if (foundCount == 0)
    {
        pc.printf("No booths detected.\n");
    }
    pc.printf("===================================\n");
}

// Helper Functions
static void sendMessage(uint8_t msgType, uint8_t *data, uint8_t dataLen, uint8_t destId)
{
    txBuffer[0] = msgType;
    if (data && dataLen > 0)
    {
        memcpy(txBuffer + 1, data, dataLen);
    }

    // 주기적 메시지가 아닌 경우 디버그 출력
    if (msgType != MSG_TYPE_BOOTH_ANNOUNCE)
    {
        pc.printf("[DEBUG] Sending message type 0x%02X to ID %d (size: %d)\n", msgType, destId, dataLen + 1);
    }

    L3_LLI_dataReqFunc(txBuffer, dataLen + 1, destId);
}

static void handleConnectRequest(uint8_t srcId)
{
    // 부스 정보 전송: 현재 사용자 수, 정원, 대기 인원, 설명 포함
    uint8_t infoData[60];
    uint8_t msgSize = L3_msg_encodeBoothInfo(infoData, myBooth.currentCount, myBooth.capacity,
                                             myBooth.waitingCount, myBooth.description);

    pc.printf("[Admin] Sending booth info to user %d\n", srcId);
    L3_LLI_dataReqFunc(infoData, msgSize, srcId); // 부스 정보 송신
}

static void handleBoothInfo(uint8_t *data, uint8_t size)
{
    // 수신된 부스 정보 파싱: 현재 이용자 수, 정원, 대기 인원, 설명
    uint8_t currentUsers = data[0];
    uint8_t capacity = data[1];
    uint8_t waitingUsers = data[2];
    char *description = (char *)(data + 3);

    pc.printf("\n=== BOOTH INFORMATION ===\n");
    pc.printf("Description: %s\n", description);
    pc.printf("Current users: %d/%d\n", currentUsers, capacity);
    pc.printf("Waiting users: %d\n", waitingUsers);
    pc.printf("Session duration: %d seconds\n", SESSION_DURATION_MS / 1000); // 신규: 세션 정보 안내
    pc.printf("\nWould you like to experience this booth? (y/n): ");

    // 사용자 입력 대기: Y/N 응답에 따라 다음 상태로 전이
    // C1, C2 조건에 따라 CONNECTED → SCANNING/WAITING/IN_USE 전이
    L3_event_setEventFlag(L3_event_keyboardInput);
    // 키보드 입력 핸들러에서 상태 전이 처리
}

static void handleRegisterResponse(uint8_t *data)
{
    uint8_t success = data[0];
    uint8_t reason = data[1];

    if (success)
    {
        pc.printf("\nRegistration successful! Welcome to the booth!\n");
        pc.printf("Your experience session has started.\n");
        pc.printf("Session duration: %d seconds\n", SESSION_DURATION_MS / 1000); // 신규: 세션 정보 안내
        pc.printf("Press 'e' to exit the booth early.\n");
        pc.printf("Press 'c' to chat with other users in the booth.\n");  // 채팅 안내 추가
        main_state = L3STATE_IN_USE; // CONNECTED → IN_USE (!C1 & C2 조건 만족)

        // 사용자 측 세션 타이머 시작
        isSessionActive = 1;
        sessionStartTime = us_ticker_read() / 1000; // ms 단위 저장
    }
    else
    {
        if (reason == REGISTER_REASON_ALREADY_USED)
        {
            pc.printf("\nRegistration failed: You have already experienced this booth.\n");
            pc.printf("Each user can only experience a booth once.\n");
            pc.printf("Returning to scanning mode...\n");

            // 이미 등록된 사용자는 재등록 불가 (C2 위반), CONNECTED → SCANNING 전이
            main_state = L3STATE_SCANNING;
            currentBoothId = 0;

            // 새로운 스캔 준비
            initializeBoothScanList();
        }
        else if (reason == REGISTER_REASON_FULL_WAITING)
        {
            pc.printf("\nBooth is full. You have been added to the waiting queue.\n");
            // 부스가 만원(C1)이고 대기열에 등록된 상태(C3) → CONNECTED → WAITING 전이
            main_state = L3STATE_WAITING;
            // 곧 QUEUE_INFO 메시지를 통해 순번 정보를 수신함
        }
    }
}

static void handleQueueInfo(uint8_t *data)
{
    // 대기열 순번 및 총 대기 사용자 수 수신 (QUEUE_INFO 메시지)
    myWaitingNumber = data[0];
    totalWaitingUsers = data[1];

    pc.printf("\nYou are in the waiting queue.\n");
    pc.printf("Your position: %d/%d\n", myWaitingNumber, totalWaitingUsers);
    pc.printf("Please stay near the booth to enter when your turn arrives.\n");
    pc.printf("Press 'e' to leave the queue.\n");
}

static uint8_t checkUserInList(User_t *list, uint8_t listSize, uint8_t userId)
{
    // 주어진 리스트(list)에서 userId 존재 여부 확인
    for (uint8_t i = 0; i < listSize; i++)
    {
        if (list[i].userId == userId)
        {
            return 1; // 리스트에 있음
        }
    }
    return 0; // 리스트에 없음
}

static uint8_t addUserToList(User_t *list, uint8_t *count, uint8_t userId)
{
    // 리스트에 이미 있는지 중복 확인
    for (uint8_t i = 0; i < *count; i++)
    {
        if (list[i].userId == userId)
        {
            return 1; // 이미 리스트에 있음
        }
    }

    // 새 사용자 추가 (리스트가 가득 차지 않았다면)
    if (*count < MAX_USERS)
    {
        list[*count].userId = userId;
        list[*count].isActive = 1;
        (*count)++;
        return 1;
    }
    return 0;
}

static void removeUserFromList(User_t *list, uint8_t *count, uint8_t userId)
{
    // 리스트(list)에서 userId 제거
    for (uint8_t i = 0; i < *count; i++)
    {
        if (list[i].userId == userId)
        {
            // 뒤의 요소들을 앞으로 한 칸씩 이동
            for (uint8_t j = i; j < *count - 1; j++)
            {
                list[j] = list[j + 1];
            }
            (*count)--;
            break;
        }
    }
}

// 사용자가 registered list에 있는지 확인
static uint8_t checkUserInRegisteredList(uint8_t userId)
{
    for (uint8_t i = 0; i < registeredCount; i++)
    {
        if (myBooth.registeredUserIds[i] == userId)
        {
            pc.printf("[DEBUG] User %d found in registered list at index %d\n", userId, i);
            return 1;
        }
    }
    pc.printf("[DEBUG] User %d not found in registered list (count: %d)\n", userId, registeredCount);
    return 0;
}

// 사용자 정보를 active list에서 제거
static void removeUserFromActiveList(uint8_t userId)
{
    removeUserFromList(myBooth.activeList, &myBooth.currentCount, userId);
}

// 사용자 명령을 처리하는 키보드 입력 핸들러
static void L3service_processKeyboardInput(void)
{
    char c = pc.getc();

    // 채팅 입력 중인 경우 - 모든 키 입력을 채팅으로 처리
    if (isTypingChat)
    {
        if (c == '\r' || c == '\n')
        {
            // 채팅 메시지 전송
            chatBuffer[chatIndex] = '\0';
            
            if (chatIndex > 0)
            {
                pc.printf("\n");
                
                // 채팅 메시지 전송
                uint8_t chatMsg[110];
                uint8_t msgSize = L3_msg_encodeChatMessage(chatMsg, chatBuffer);
                L3_LLI_dataReqFunc(chatMsg, msgSize, currentBoothId);
                
                pc.printf("[Chat] You: %s\n", chatBuffer);
            }
            else
            {
                pc.printf("\n");  // 빈 메시지인 경우 줄바꿈만
            }
            
            // 채팅 입력 상태 초기화
            isTypingChat = 0;
            chatIndex = 0;
        }
        else if (c == '\b' || c == 127)
        {
            // 백스페이스 처리
            if (chatIndex > 0)
            {
                chatIndex--;
                pc.printf("\b \b");
            }
        }
        else if (c == 27)  // ESC 키로 채팅 취소
        {
            pc.printf("\n[Chat cancelled]\n");
            isTypingChat = 0;
            chatIndex = 0;
        }
        else if (chatIndex < 100)
        {
            // 채팅 버퍼에 문자 추가 (e 포함 모든 문자)
            chatBuffer[chatIndex++] = c;
            pc.putc(c);
        }
        return;  // 채팅 모드에서는 다른 명령 처리 안함
    }

    // 부스 선택 응답 처리 (CONNECTED 상태에서)
    if (L3_event_checkEventFlag(L3_event_keyboardInput) && main_state == L3STATE_CONNECTED)
    {
        if (c == 'y' || c == 'Y')
        {
            pc.printf("y\n");

            // USER_RESPONSE YES 전송 (부스 체험 의사 표시)
            uint8_t responseMsg[2];
            L3_msg_encodeUserResponse(responseMsg, USER_RESPONSE_YES);
            L3_LLI_dataReqFunc(responseMsg, 2, currentBoothId);

            // REGISTER_REQUEST 전송 (부스에 등록 요청)
            pc.printf("Sending registration request...\n");
            sendMessage(MSG_TYPE_REGISTER_REQUEST, NULL, 0, currentBoothId);
            
            // 등록 응답 대기 상태 설정
            registerRetryCount = 0;
            isWaitingForRegisterResponse = 1;
            registerRequestTime = us_ticker_read() / 1000;
            
            L3_event_clearEventFlag(L3_event_keyboardInput);
        }
        else if (c == 'n' || c == 'N')
        {
            pc.printf("n\n");

            // USER_RESPONSE NO 전송 (부스 체험 거부)
            uint8_t responseMsg[2];
            L3_msg_encodeUserResponse(responseMsg, USER_RESPONSE_NO);
            L3_LLI_dataReqFunc(responseMsg, 2, currentBoothId);

            pc.printf("Declined. Returning to scanning mode...\n");
            main_state = L3STATE_SCANNING; // CONNECTED → SCANNING
            currentBoothId = 0;
            L3_event_clearEventFlag(L3_event_keyboardInput);

            // 새로운 스캔 준비
            initializeBoothScanList();
        }
        return;
    }

    // 일반 명령 처리 (채팅 모드가 아닐 때만)
    
    // 채팅 시작 ('c' 키) - IN_USE 상태에서만 가능
    if ((c == 'c' || c == 'C') && main_state == L3STATE_IN_USE && !isTypingChat)
    {
        pc.printf("\nEnter chat message (max 100 chars, ESC to cancel): ");
        isTypingChat = 1;
        chatIndex = 0;
        return;
    }

    // 종료 명령 처리 ('e' 키): 부스에서 나가기 또는 대기열 탈퇴
    if ((c == 'e' || c == 'E') && !isTypingChat)  // 채팅 모드가 아닐 때만
    {
        if (!isAdmin && main_state == L3STATE_IN_USE)
        {
            // 세션 종료 시간 계산 및 표시 (사용자 측)
            if (isSessionActive)
            {
                uint32_t sessionDuration = (us_ticker_read() / 1000) - sessionStartTime;
                pc.printf("\nExiting booth... Session duration: %d seconds\n", sessionDuration / 1000);
                isSessionActive = 0;
                sessionStartTime = 0;
            }

            // EXIT_REQUEST 전송 (관리자에게 퇴장 요청)
            sendMessage(MSG_TYPE_EXIT_REQUEST, NULL, 0, currentBoothId);

            // 즉시 SCANNING 상태로 복귀
            pc.printf("Exited the booth.\n");
            pc.printf("Returning to RSSI-based scanning mode...\n");
            main_state = L3STATE_SCANNING; // IN_USE → SCANNING
            currentBoothId = 0;

            // 새로운 스캔 준비
            initializeBoothScanList();
        }
        else if (!isAdmin && main_state == L3STATE_WAITING)
        {
            // 대기열 탈퇴 요청
            pc.printf("\nLeaving waiting queue...\n");

            sendMessage(MSG_TYPE_QUEUE_LEAVE, NULL, 0, currentBoothId);

            pc.printf("Left the queue. Returning to scanning mode...\n");
            main_state = L3STATE_SCANNING; // WAITING → SCANNING
            currentBoothId = 0;
            myWaitingNumber = 0;
            totalWaitingUsers = 0;

            // 새로운 스캔 준비
            initializeBoothScanList();
        }
    }
}

// 관리자 명령을 처리하는 키보드 입력 핸들러
static void L3admin_processKeyboardInput(void)
{
    static char msgBuffer[101]; // 커스텀 메시지 버퍼 (최대 100자)
    static uint8_t msgIndex = 0;
    static uint8_t isTypingMessage = 0;

    char c = pc.getc();

    if (isTypingMessage)
    {
        // 사용자가 커스텀 메시지 입력 중
        if (c == '\r' || c == '\n')
        {
            // 메시지 입력 완료
            msgBuffer[msgIndex] = '\0';

            if (msgIndex > 0)
            {
                // 활성 사용자에게만 메시지 방송 (현재 activeList에 있는 사용자)
                pc.printf("\nBroadcasting to active users: \"%s\"\n", msgBuffer);
                
                uint8_t adminMsg[110];
                uint8_t msgSize = L3_msg_encodeAdminMessage(adminMsg, msgBuffer);
                
                if (myBooth.currentCount > 0)
                {
                    for (uint8_t i = 0; i < myBooth.currentCount; i++)
                    {
                        L3_LLI_dataReqFunc(adminMsg, msgSize, myBooth.activeList[i].userId);
                        pc.printf("Message sent to User %d\n", myBooth.activeList[i].userId);
                    }
                    pc.printf("Total %d active user(s) received the message.\n\n", myBooth.currentCount);
                }
                else
                {
                    pc.printf("No active users in booth.\n\n");
                }
            }

            // 입력 상태 초기화
            isTypingMessage = 0;
            msgIndex = 0;
        }
        else if (c == '\b' || c == 127)
        {
            // 백스페이스 입력 처리
            if (msgIndex > 0)
            {
                msgIndex--;
                pc.printf("\b \b"); // 터미널에서 문자 지우기
            }
        }
        else if (msgIndex < 100)
        {
            // 버퍼에 문자 추가 및 에코 출력
            msgBuffer[msgIndex++] = c;
            pc.putc(c);
        }
    }
    else
    {
        // 명령 모드: 단일 문자 입력으로 명령 처리
        switch (c)
        {
        case 'm':
        case 'M':
            pc.printf("\nEnter broadcast message (max 100 chars): ");
            isTypingMessage = 1;
            msgIndex = 0;
            break;

        case 's':
        case 'S':
            // 부스 상태 정보 출력 (관리자 측)
            pc.printf("\n=== BOOTH STATUS ===\n");
            pc.printf("Booth ID: %d\n", myBooth.boothId);
            pc.printf("Current Users: %d/%d\n", myBooth.currentCount, myBooth.capacity);
            pc.printf("Waiting Queue: %d users\n", myBooth.waitingCount);
            pc.printf("Total Registered: %d users\n", registeredCount);
            pc.printf("Session Duration: %d seconds\n", SESSION_DURATION_MS / 1000);

            if (myBooth.currentCount > 0)
            {
                pc.printf("\nActive Users:\n");
                uint32_t currentTime = us_ticker_read() / 1000;
                for (uint8_t i = 0; i < myBooth.currentCount; i++)
                {
                    uint32_t sessionTime = (currentTime - myBooth.activeList[i].sessionStartTime) / 1000;
                    uint32_t remainingTime = (SESSION_DURATION_MS / 1000) - sessionTime;
                    pc.printf("  - User %d (Time remaining: %d sec)\n",
                              myBooth.activeList[i].userId, remainingTime);
                }
            }

            if (registeredCount > 0)
            {
                pc.printf("\nRegistered Users (all-time):\n");
                for (uint8_t i = 0; i < registeredCount; i++)
                {
                    pc.printf("  - User %d\n", myBooth.registeredUserIds[i]);
                }
            }
            pc.printf("==================\n\n");
            break;

        case 't': // Show timer status (관리자 측 세션 타이머 확인)
        case 'T':
            pc.printf("\n=== SESSION TIMERS ===\n");
            if (myBooth.currentCount > 0)
            {
                uint32_t currentTime = us_ticker_read() / 1000;
                for (uint8_t i = 0; i < myBooth.currentCount; i++)
                {
                    uint32_t elapsedTime = (currentTime - myBooth.activeList[i].sessionStartTime) / 1000;
                    uint32_t remainingTime = (SESSION_DURATION_MS / 1000) - elapsedTime;

                    pc.printf("User %d: ", myBooth.activeList[i].userId);
                    if (remainingTime > 0)
                    {
                        pc.printf("%d seconds remaining", remainingTime);
                        if (remainingTime < 10)
                        {
                            pc.printf(" EXPIRING SOON!");
                        }
                    }
                    else
                    {
                        pc.printf("EXPIRED!");
                    }
                    pc.printf("\n");
                }
            }
            else
            {
                pc.printf("No active users.\n");
            }
            pc.printf("====================\n\n");
            break;

        case 'a':
        case 'A':
        {
            // 부스 정보 재방송 (관리자 측)
            pc.printf("\nSending booth announcement...\n");
            uint8_t announceData[10];
            uint8_t msgSize = L3_msg_encodeBoothAnnounce(announceData, myBooth.boothId,
                                                         myBooth.currentCount, myBooth.capacity,
                                                         myBooth.waitingCount);
            L3_LLI_dataReqFunc(announceData, msgSize, BROADCAST_ID);
            pc.printf("Announcement sent!\n\n");
            break;
        }

        case 'q':
        case 'Q':
            // 조용 모드 토글: 자동 방송 중지/재개
            quietMode = !quietMode;
            pc.printf("\nQuiet mode: %s\n", quietMode ? "ON (auto-broadcasts disabled)" : "OFF (auto-broadcasts enabled)");
            pc.printf("Manual broadcasts with 'a' still work in quiet mode.\n\n");
            break;

        case 'w':
        case 'W':
            // 현재 대기 큐 상태 출력 (관리자 측)
            pc.printf("\n=== WAITING QUEUE ===\n");
            if (myBooth.waitingCount > 0)
            {
                pc.printf("Waiting users: %d\n", myBooth.waitingCount);
                for (uint8_t i = 0; i < myBooth.waitingCount; i++)
                {
                    pc.printf("  %d. User %d", i + 1, myBooth.waitingQueue[i].userId);
                    if (i == 0 && isQueueReadyTimerActive)
                    {
                        uint32_t elapsed = (us_ticker_read() / 1000 - queueReadyStartTime) / 1000;
                        uint32_t remaining = (QUEUE_READY_TIMEOUT_MS / 1000) - elapsed;
                        pc.printf(" (Notified - %d sec remaining)", remaining);
                    }
                    pc.printf("\n");
                }
            }
            else
            {
                pc.printf("No users waiting.\n");
            }
            pc.printf("===================\n\n");
            break;

        case 'h':
        case 'H':
            // 관리자 도움말 출력
            pc.printf("\n=== ADMIN COMMANDS ===\n");
            pc.printf("  'm' - Send custom broadcast message\n");
            pc.printf("  's' - Show booth status\n");
            pc.printf("  'a' - Send announcement\n");
            pc.printf("  'q' - Toggle quiet mode\n");
            pc.printf("  't' - Show session timers\n");
            pc.printf("  'w' - Show waiting queue\n");
            pc.printf("  'h' - Show this help\n");
            pc.printf("====================\n\n");
            break;
        }
    }
}
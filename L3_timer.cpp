#include "mbed.h"
#include "L3_FSMevent.h"
#include "protocol_parameters.h"

// ARQ 재전송 타이머
static Timeout timer;                       
static uint8_t timerStatus = 0;

// 부스 선택 타이머
static Timeout boothSelectionTimer;
static uint8_t boothSelectionTimerStatus = 0;


// 타이머 만료 핸들러: ARQ 타임아웃
void L3_timer_timeoutHandler(void) 
{
    timerStatus = 0;
    //L3_event_setEventFlag(L3_event_arqTimeout);  // 필요 시 활성화
}

// 타이머 만료 핸들러: 부스 선택 타임아웃 (신규)
void L3_timer_boothSelectionHandler(void)
{
    boothSelectionTimerStatus = 0;
    L3_event_setEventFlag(L3_event_boothSelectionTimeout);
}

// ARQ 재전송 타이머 시작
void L3_timer_startTimer()
{
    uint8_t waitTime = 1;  // 1초 대기
    timer.attach(L3_timer_timeoutHandler, waitTime);
    timerStatus = 1;
}

// ARQ 재전송 타이머 중지
void L3_timer_stopTimer()
{
    timer.detach();
    timerStatus = 0;
}

// ARQ 재전송 타이머 상태 조회
uint8_t L3_timer_getTimerStatus()
{
    return timerStatus;
}

// 부스 선택 타이머 시작
void L3_timer_boothSelectionStart()
{
    boothSelectionTimer.attach(L3_timer_boothSelectionHandler, 2.0);  // 2초 후 타임아웃
    boothSelectionTimerStatus = 1;
}

// 부스 선택 타이머 중지
void L3_timer_boothSelectionStop()
{
    boothSelectionTimer.detach();
    boothSelectionTimerStatus = 0;
}

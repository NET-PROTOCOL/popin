// L3_timer.cpp - Enhanced timer implementation
#include "mbed.h"
#include "L3_FSMevent.h"
#include "protocol_parameters.h"

// ARQ retransmission timer
static Timeout timer;                       
static uint8_t timerStatus = 0;

// Booth selection timer (new)
static Timeout boothSelectionTimer;
static uint8_t boothSelectionTimerStatus = 0;

// Timer event: ARQ timeout
void L3_timer_timeoutHandler(void) 
{
    timerStatus = 0;
    //L3_event_setEventFlag(L3_event_arqTimeout);
}

// Timer event: Booth selection timeout (new)
void L3_timer_boothSelectionHandler(void)
{
    boothSelectionTimerStatus = 0;
    L3_event_setEventFlag(L3_event_boothSelectionTimeout);
}

// Timer related functions
void L3_timer_startTimer()
{
    uint8_t waitTime = 1;
    timer.attach(L3_timer_timeoutHandler, waitTime);
    timerStatus = 1;
}

void L3_timer_stopTimer()
{
    timer.detach();
    timerStatus = 0;
}

uint8_t L3_timer_getTimerStatus()
{
    return timerStatus;
}

// Booth selection timer functions (new)
void L3_timer_boothSelectionStart()
{
    boothSelectionTimer.attach(L3_timer_boothSelectionHandler, 2.0);  // 2 second timeout
    boothSelectionTimerStatus = 1;
}

void L3_timer_boothSelectionStop()
{
    boothSelectionTimer.detach();
    boothSelectionTimerStatus = 0;
}
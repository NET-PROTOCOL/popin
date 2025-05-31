// L3_LLinterface.h - Add RSSI getter
#ifndef L3_LLINTERFACE_H
#define L3_LLINTERFACE_H

#include "mbed.h"

extern void (*L3_LLI_dataReqFunc)(uint8_t* msg, uint8_t size, uint8_t destId);

void L3_LLI_dataInd(uint8_t* dataPtr, uint8_t srcId, uint8_t size, int8_t snr, int16_t rssi);
uint8_t* L3_LLI_getMsgPtr();
uint8_t L3_LLI_getSize();
uint8_t L3_LLI_getSrcId();
int16_t L3_LLI_getRssi();  // Add RSSI getter
int8_t L3_LLI_getSnr();     // Add SNR getter
void L3_LLI_setDataReqFunc(void (*funcPtr)(uint8_t*, uint8_t, uint8_t));
void L3_LLI_setReconfigSrcIdReqFunc(void (*funcPtr)(uint8_t));
void L3_LLI_dataCnf(uint8_t res);
void L3_LLI_reconfigSrcIdCnf(uint8_t res);

#endif // L3_LLINTERFACE_H
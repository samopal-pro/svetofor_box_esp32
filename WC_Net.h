#pragma once
#include "MyConfig.h"
#include "src/Slib/SHTTPClient.h"
#include "WC_Config.h"
#include "WC_Task.h"
#include "WC_HttpSend.h"

extern bool isAP;
extern bool isSTA;
extern bool isSendNet;
extern bool isWiFiConnected;

void taskWiFiManager(void *pvParameters);
void taskHttpSender(void *pvParameters);
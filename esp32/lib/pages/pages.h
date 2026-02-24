#ifndef PAGES_H
#define PAGES_H

#include <Arduino.h>

void updateStatusBar(const String& page);

void showBootPage();
void setBootStatus(const String& status);

void showUserPage();
void showConsumePage(const String& username);
void showPurchasePage(const String& username);
void showLoggingPage(const String& type, const String& username, float amount);
void showLoggedPage(const String& type, const String& username, float newBalance, float amount);
bool showAccountPage(const String& username);
void showBalancePage(const String& username);
bool showInventoryPage();
void showInfoPage();

#endif

#pragma once

#include <Arduino.h>

void webAuthBegin();
bool webAuthHasPassword();
bool webAuthSetPassword(const String &password);
bool webAuthCheckPassword(const String &password);
String webAuthIssueToken();
bool webAuthTokenValid(const String &token);
bool webAuthRevokeToken(const String &token);
void webAuthInvalidateTokens();

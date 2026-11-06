#pragma once
#include <string>
struct AuthState {bool allowed=false,busy=false,ready=false;std::string message;};
void Auth_Init();
bool Auth_Allowed();
AuthState Auth_Get();
std::string Auth_Clipboard();
void Auth_Login(std::string key);
std::string Auth_SavedKey();
void Auth_ForgetKey();

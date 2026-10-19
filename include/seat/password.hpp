#pragma once
#include <string>
namespace seat::auth {
std::string hash_password(const std::string &password);
bool verify_password(const std::string &password, const std::string &hash);
std::string random_token();
std::string digest(const std::string &text);
} // namespace seat::auth

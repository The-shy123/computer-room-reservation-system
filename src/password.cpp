#include "seat/password.hpp"
#include "seat/domain.hpp"
#include <array>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <sstream>
#include <vector>
namespace seat::auth {
namespace {
std::string hex(const unsigned char *p, std::size_t n) {
    const char *digits = "0123456789abcdef";
    std::string out;
    out.reserve(2 * n);
    for (std::size_t i = 0; i < n; ++i) {
        out += digits[p[i] >> 4];
        out += digits[p[i] & 15];
    }
    return out;
}
std::vector<unsigned char> unhex(const std::string &s) {
    if (s.size() % 2 != 0)
        throw Error(Code::Storage, "malformed password hash");
    auto nibble = [](char c) {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        throw Error(Code::Storage, "malformed password hash");
    };
    std::vector<unsigned char> out;
    for (std::size_t i = 0; i < s.size(); i += 2)
        out.push_back(static_cast<unsigned char>(16 * nibble(s[i]) + nibble(s[i + 1])));
    return out;
}
std::array<unsigned char, 32> derive(const std::string &p, const std::vector<unsigned char> &salt) {
    std::array<unsigned char, 32> out{};
    if (PKCS5_PBKDF2_HMAC(p.data(), static_cast<int>(p.size()), salt.data(),
                          static_cast<int>(salt.size()), 600000, EVP_sha256(),
                          static_cast<int>(out.size()), out.data()) != 1)
        throw Error(Code::Storage, "password derivation failed");
    return out;
}
} // namespace
std::string hash_password(const std::string &p) {
    if (p.size() < 8 || p.size() > 128)
        throw Error(Code::InvalidInput, "password length must be 8-128 bytes");
    std::vector<unsigned char> salt(16);
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1)
        throw Error(Code::Storage, "random generator failed");
    auto key = derive(p, salt);
    return "pbkdf2-sha256$600000$" + hex(salt.data(), salt.size()) + "$" +
           hex(key.data(), key.size());
}
bool verify_password(const std::string &p, const std::string &encoded) {
    if (p.size() > 128)
        return false;
    const std::string prefix = "pbkdf2-sha256$600000$";
    if (encoded.substr(0, prefix.size()) != prefix ||
        encoded.size() != prefix.size() + 32 + 1 + 64 || encoded[prefix.size() + 32] != '$')
        throw Error(Code::Storage, "unsupported password hash");
    auto salt = unhex(encoded.substr(prefix.size(), 32)),
         expected = unhex(encoded.substr(prefix.size() + 33));
    auto actual = derive(p, salt);
    return CRYPTO_memcmp(actual.data(), expected.data(), actual.size()) == 0;
}
std::string random_token() {
    std::array<unsigned char, 32> out{};
    if (RAND_bytes(out.data(), static_cast<int>(out.size())) != 1)
        throw Error(Code::Storage, "random generator failed");
    return hex(out.data(), out.size());
}
std::string digest(const std::string &s) {
    std::array<unsigned char, 32> out{};
    unsigned int n = 0;
    if (EVP_Digest(s.data(), s.size(), out.data(), &n, EVP_sha256(), nullptr) != 1 || n != 32)
        throw Error(Code::Storage, "digest failed");
    return hex(out.data(), out.size());
}
} // namespace seat::auth

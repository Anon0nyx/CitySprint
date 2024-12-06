#include "Utilities.h"
#include "Logger.h"
#include <iostream>
#include <string>
#include <openssl/sha.h>
#include <atomic>

void log(const std::string& message) {
  Logger::getInstance().log(message);
}

int generateUniqueId() {
  static std::atomic<int> idCounter(1);
  int id = idCounter++;
  log("Generated unique ID: " + std::to_string(id));
  return id;
}

std::string encodeWebSocketFrame(const std::string& message) {
  std::string frame;
  frame.push_back(0x81); // Text frame
  if (message.size() <= 125) {
    frame.push_back(static_cast<char>(message.size()));
  }
  else if (message.size() <= 65535) {
    frame.push_back(126);
    frame.push_back((message.size() >> 8) & 0xFF);
    frame.push_back(message.size() & 0xFF);
  }
  else {
    frame.push_back(127);
    for (int i = 7; i >= 0; i--) {
      frame.push_back((message.size() >> (8 * i)) & 0xFF);
    }
  }
  frame.append(message);
  return frame;
}

std::string decodeWebSocketFrame(const std::string& frame) {
  size_t payloadStart = 2;
  size_t payloadLength = frame[1] & 0x7F;

  if (payloadLength == 126) {
    payloadStart = 4;
    payloadLength = (frame[2] << 8) | frame[3];
  }
  else if (payloadLength == 127) {
    payloadStart = 10;
    payloadLength = 0;
    for (int i = 0; i < 8; i++) {
      payloadLength |= (static_cast<size_t>(frame[2 + i]) << (56 - 8 * i));
    }
  }

  std::string decoded;
  if (frame[1] & 0x80) { // Masked
    char masks[4] = { frame[payloadStart], frame[payloadStart + 1], frame[payloadStart + 2], frame[payloadStart + 3] };
    size_t i = payloadStart + 4;
    size_t j = 0;
    while (j < payloadLength) {
      decoded.push_back(frame[i] ^ masks[j % 4]);
      i++;
      j++;
    }
  }
  else {
    decoded = frame.substr(payloadStart, payloadLength);
  }
  return decoded;
}

std::string base64Encode(const unsigned char* input, int length) {
  static const char base64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string encoded;
  for (int i = 0; i < length; i += 3) {
    int val = (input[i] << 16) + (i + 1 < length ? (input[i + 1] << 8) : 0) + (i + 2 < length ? input[i + 2] : 0);
    encoded.push_back(base64Chars[(val >> 18) & 0x3F]);
    encoded.push_back(base64Chars[(val >> 12) & 0x3F]);
    encoded.push_back(i + 1 < length ? base64Chars[(val >> 6) & 0x3F] : '=');
    encoded.push_back(i + 2 < length ? base64Chars[val & 0x3F] : '=');
  }
  return encoded;
}

std::string generateWebSocketAcceptKey(const std::string& key) {
  std::string acceptKey = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  unsigned char hash[SHA_DIGEST_LENGTH];
  SHA1(reinterpret_cast<const unsigned char*>(acceptKey.c_str()), acceptKey.size(), hash);
  return base64Encode(hash, SHA_DIGEST_LENGTH);
}

double squareRoot(int initialNum) {
  double numCopy = static_cast<double>(initialNum);

  if (numCopy < 2)
    return numCopy;

  double z = (numCopy + (initialNum / numCopy)) / 2;

  while (abs(numCopy - z) >= 0.00001) {
    numCopy = z;
    z = (numCopy + (initialNum / numCopy)) / 2;
  }
  return z;
}

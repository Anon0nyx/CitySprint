#include "Logger.h"
#include <iostream>

Logger::Logger() {
  logFile.open("./log/log.txt", std::ios::out | std::ios::app);
}

Logger::~Logger() {
  if (logFile.is_open()) {
    logFile.close();
  }
}

Logger& Logger::getInstance() {
  static Logger instance;
  return instance;
}

void Logger::log(const std::string& message) {
  std::scoped_lock<std::mutex> lock(logMutex);
  std::cout << message << std::endl;
  logFile << message << std::endl;
}

void Logger::close() {
  std::scoped_lock<std::mutex> lock(logMutex);
  if (logFile.is_open()) {
    logFile.close();
  }
}

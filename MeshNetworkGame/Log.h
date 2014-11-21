#ifndef __LOG_H__
#define __LOG_H__
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>
#include <iomanip>

std::string getCurrentTime();
void Log(const std::string message);

#endif  //__LOG_H__
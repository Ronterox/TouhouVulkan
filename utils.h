#pragma once

#define __FILE_LINE__ __FILE__ << ':' << __LINE__ << ": "

#include <iostream>
#define LOG(x) std::cout << __FILE_LINE__ << x << std::endl
#define LOGE(x) std::cerr << __FILE_LINE__ << x << std::endl
#define ERROR(x) throw std::runtime_error(x)

#include <vector>
template<typename T>
using list = std::vector<T>;
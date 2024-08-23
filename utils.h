#pragma once

#define __FILE_LINE__ __FILE__ << ':' << __LINE__ << ": "

#include <iostream>
#ifdef NDEBUG
#define LOG(x)
#define LOGE(x)
#define ERROR(x)
#else
#define LOG(x) std::cout << __FILE_LINE__ << x << std::endl
#define LOGE(x) std::cerr << __FILE_LINE__ << x << std::endl
#define ERROR(x) throw std::runtime_error(x)
#endif

#include <vector>
template <typename T> using list = std::vector<T>;

#define IS_STR_EQUAL(a, b) (strcmp(a, b) == 0)

#define VK_CHECK(x, y)                                                                                                 \
	if (x != VK_SUCCESS) {                                                                                             \
		LOGE("VK_CHECK failed:" << y);                                                                                 \
		ERROR(y);                                                                                                      \
	}
#define VALIDATE(x, y)                                                                                                 \
	if (!(x)) {                                                                                                        \
		LOGE("VALIDATE failed: " << y);                                                                                \
		ERROR(y);                                                                                                      \
	}

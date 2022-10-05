#ifndef UTILS_H_
#define UTILS_H_

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <vulkan/vulkan.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#ifndef LOG_TAG
#define LOG_TAG "log"
#endif

#define ii(...) printf("\033[32m(ii)\033[0m " LOG_TAG " " __VA_ARGS__)
#define ww(...) printf("\033[33m(ww)\033[0m " LOG_TAG " " __VA_ARGS__)
#undef ee
#define ee(...) {\
	int errno__ = errno;\
	fprintf(stderr, "\033[31m(ee)\033[0m " LOG_TAG " " __VA_ARGS__);\
	fprintf(stderr, "\33[31m(ee)\033[0m " LOG_TAG " ^^ %s:%d | %s\n",\
	 __func__, __LINE__, __FILE__);\
	if (errno__) {\
		fprintf(stderr, "\033[31m(ee)\033[0m " LOG_TAG " %s (%d)\n",\
		 strerror(errno__), errno__);\
	}\
	errno = errno__;\
}

#define nn(...) ;

#define dd(...) {\
	printf("\033[2m(dd)\033[0m " LOG_TAG __VA_ARGS__);\
	printf("\033[2m(dd)\033[0m ^^ %s:%d | %s\n", __func__, __LINE__,\
	 __FILE__);\
}

static inline const char *vk_strerror(VkResult err)
{
	switch (err) {
#define case_err(r) case VK_ ##r: return #r
	case_err(NOT_READY);
	case_err(TIMEOUT);
	case_err(EVENT_SET);
	case_err(EVENT_RESET);
	case_err(INCOMPLETE);
	case_err(ERROR_OUT_OF_HOST_MEMORY);
	case_err(ERROR_OUT_OF_DEVICE_MEMORY);
	case_err(ERROR_INITIALIZATION_FAILED);
	case_err(ERROR_DEVICE_LOST);
	case_err(ERROR_MEMORY_MAP_FAILED);
	case_err(ERROR_LAYER_NOT_PRESENT);
	case_err(ERROR_EXTENSION_NOT_PRESENT);
	case_err(ERROR_FEATURE_NOT_PRESENT);
	case_err(ERROR_INCOMPATIBLE_DRIVER);
	case_err(ERROR_TOO_MANY_OBJECTS);
	case_err(ERROR_FORMAT_NOT_SUPPORTED);
	case_err(ERROR_SURFACE_LOST_KHR);
	case_err(ERROR_NATIVE_WINDOW_IN_USE_KHR);
	case_err(SUBOPTIMAL_KHR);
	case_err(ERROR_OUT_OF_DATE_KHR);
	case_err(ERROR_INCOMPATIBLE_DISPLAY_KHR);
	case_err(ERROR_VALIDATION_FAILED_EXT);
	case_err(ERROR_INVALID_SHADER_NV);
#undef case_err
	default: return "UNKNOWN_ERROR";
	}
}

#define vk_call(err, f) {\
	err = (f);\
	if (err != VK_SUCCESS) {\
		fprintf(stderr, "(ee) %s | %s:%d | %s\n",\
		 vk_strerror(err), __func__, __LINE__, __FILE__);\
	}\
}

#endif /* UTILS_H_ */

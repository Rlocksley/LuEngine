#pragma once


#define LU_DEBUG

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

//C
#include <string.h>
#include <time.h>

//C++
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <array>
#include <queue>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <bitset>
#include <memory>
#include <thread>
#include <random>
#include <functional>
#include <typeinfo>
#include <algorithm>
#include <variant>
#include <mutex>
#include <typeindex>
#include <chrono>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "glm/gtc/quaternion.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#define LU_LOGI(Class, Function, Message)\
{\
    std::cout << "Info@LU: " << Class << " :: " << Function << " :: " << Message << std::endl;\
}

#define LU_LOGE(Class, Function, Message)\
{\
    std::cerr << "Error@LU: " << Class << " :: " << Function << " :: " << Message << std::endl;\
    exit(1);\
}

#define LU_CHECK_VULKAN(VulkanExpression, Function, VulkanFunction)\
{\
    VkResult vkRes;\
    if((vkRes = VulkanExpression) != VK_SUCCESS)\
    {\
        std::cerr << "Error@LU: " << Function << " :: " << VulkanFunction << std::endl;\
        std::cerr << "Vulkan Error Code: " << vkRes << std::endl;\
        exit(1);\
    }\
}


#define LU_ASSERT(Expression, Class, Funktion, Message)\
{\
    if(!(Expression))\
    {\
        LU_LOGE(Class, Funktion, Message)\
    }\
}

#define init_random() srand(static_cast<unsigned int>(time(0)))
#define random(lower, upper) (static_cast<float>((static_cast<float>(rand())/static_cast<float>(RAND_MAX))*((upper)-(lower)) + (lower)))


template<class... Ts> struct variant_match : Ts... { using Ts::operator()...; };
template<class... Ts> variant_match(Ts...) -> variant_match<Ts...>;
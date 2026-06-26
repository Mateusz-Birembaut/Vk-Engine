cmake_minimum_required(VERSION 3.23)

project(VkEngine)

add_executable(VkEngine)

target_sources( VkEngine
	PRIVATE
		src/main.cpp

	PUBLIC
		FILE_SET HEADERS
		BASE_DIRS
			include
		FILES


)
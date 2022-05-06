cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED On)

project(GeodeInstaller VERSION 0.1.0)
set(PROJECT_VERSION_COMMA 0,1,0)

file(READ "EULA" GEODE_EULA)
configure_file(
	${CMAKE_SOURCE_DIR}/src/include/eula.hpp.in
	${CMAKE_SOURCE_DIR}/src/include/eula.hpp
)

file(GLOB_RECURSE SOURCES
	src/*.cpp
)

file(GLOB_RECURSE HEADERS
	src/*.hpp
	src/*.h
)

find_package(wxWidgets REQUIRED COMPONENTS base core net)
include(${wxWidgets_USE_FILE})

if (WIN32)
	configure_file(
		${CMAKE_SOURCE_DIR}/${PROJECT_NAME}.exe.manifest.in
		${PROJECT_NAME}.exe.manifest
	)

	add_executable(${PROJECT_NAME} WIN32 ${SOURCES})
	
	target_precompile_headers(${PROJECT_NAME} PUBLIC ${HEADERS})
	
	add_custom_command(
		TARGET ${PROJECT_NAME}
		POST_BUILD
		COMMAND "mt.exe" -manifest \"${CMAKE_CURRENT_BINARY_DIR}\\${PROJECT_NAME}.exe.manifest\" -outputresource:"$<TARGET_FILE:${PROJECT_NAME}>"\;\#1
		COMMENT "Adding manifest..."
	)
else()
	file(GLOB_RECURSE OBJC_SOURCES
		src/*.mm
	)

	add_executable(${PROJECT_NAME} ${SOURCES} ${OBJC_SOURCES})
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC ${wxWidgets_LIBRARIES})

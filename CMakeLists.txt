cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED On)

project(GeodeInstaller VERSION 0.3.1)
set(PROJECT_VERSION_COMMA 0,3,1)

file(READ "EULA" GEODE_EULA)
configure_file(
	${CMAKE_SOURCE_DIR}/src/include/eula.hpp.in
	${CMAKE_SOURCE_DIR}/src/include/eula.hpp
)
configure_file(
	${CMAKE_SOURCE_DIR}/src/include/info.hpp.in
	${CMAKE_SOURCE_DIR}/src/include/info.hpp
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
	configure_file(
		${CMAKE_SOURCE_DIR}/info.rc.in
		info.rc
	)

	add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${CMAKE_BINARY_DIR}/info.rc)
	
	target_precompile_headers(${PROJECT_NAME} PUBLIC ${HEADERS})

	target_link_libraries(${PROJECT_NAME} PUBLIC imagehlp)
else()
	file(GLOB_RECURSE OBJC_SOURCES
		src/*.mm
	)

	add_executable(${PROJECT_NAME} ${SOURCES} ${OBJC_SOURCES})
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC ${wxWidgets_LIBRARIES})

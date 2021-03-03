cmake_minimum_required(VERSION 2.6)

PROJECT(nanojson)

OPTION(BUILD_SHARED_LIBRARY "Build shared library" OFF)
OPTION(BUILD_TESTS "Build tests application" ON)
OPTION(JSON_FLOATS "Enabled support of floating point numbers" OFF)
OPTION(JSON_64BITS_INTEGERS "Enable support of 64 bits integers" OFF)
OPTION(JSON_HEX_NUMBERS "Enabled support of 0x integers" OFF)
OPTION(JSON_PACKED "use packed json item structure" OFF)
OPTION(JSON_SHORT_NEXT "use short type for next field of jsn_t" OFF)


OPTION(HOST_DEBUG "Log to console" OFF)


ADD_DEFINITIONS(-pipe --std=gnu99 -ftabstop=4 -Wno-unused-function)
ADD_DEFINITIONS(-Wall -Wmissing-declarations -Winit-self -Wswitch-enum -Wundef)
ADD_DEFINITIONS(-Wmissing-field-initializers -Wredundant-decls -Wpointer-arith)

ADD_DEFINITIONS(-D_GNU_SOURCE)

SET(static_library_target nanojson_static)
SET(shared_library_target nanojson)

CONFIGURE_FILE(nano/json.h.in nano/json.h @ONLY)

IF(BUILD_TESTS)
	ADD_EXECUTABLE(tests tests.c)
	TARGET_LINK_LIBRARIES(tests ${static_library_target})
ENDIF(BUILD_TESTS)

ADD_LIBRARY(${static_library_target} STATIC parser.c methods.c stringify.c)

IF(BUILD_SHARED_LIBRARY)
	ADD_LIBRARY(${shared_library_target} SHARED parser.c methods.c stringify.c)
	SET_TARGET_PROPERTIES(${shared_library_target} PROPERTIES PUBLIC_HEADER "nano/json.h")
ENDIF(BUILD_SHARED_LIBRARY)


IF(JSON_FLOATS)
	TARGET_LINK_LIBRARIES(tests m)
ENDIF()

IF(HOST_DEBUG)
	ADD_DEFINITIONS(-O0 -g3)
ELSE()
	ADD_DEFINITIONS(-Os -Werror -ffast-math -fmerge-all-constants -ffunction-sections -fdata-sections -fno-asynchronous-unwind-tables)
	IF(BUILD_TESTS)
		SET_TARGET_PROPERTIES(tests PROPERTIES COMPILE_FLAGS -fvisibility=hidden)
		SET_TARGET_PROPERTIES(tests PROPERTIES LINK_FLAGS -Wl,--gc-sections,--build-id=none)
	ENDIF(BUILD_TESTS)
	SET_TARGET_PROPERTIES(${static_library_target} PROPERTIES LINK_FLAGS -Wl,--gc-sections,--build-id=none)
	IF(BUILD_SHARED_LIBRARY)
		SET_TARGET_PROPERTIES(${shared_library_target} PROPERTIES LINK_FLAGS -Wl,--gc-sections,--build-id=none)
	ENDIF()
ENDIF()

IF(BUILD_TESTS)
	INSTALL(TARGETS tests
		ARCHIVE DESTINATION lib
		LIBRARY DESTINATION lib
		RUNTIME DESTINATION bin
	)
ENDIF(BUILD_TESTS)

INSTALL(TARGETS ${static_library_target}
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
)

IF(BUILD_SHARED_LIBRARY)
	INSTALL(TARGETS ${shared_library_target}
		ARCHIVE DESTINATION lib
		LIBRARY DESTINATION lib
		RUNTIME DESTINATION bin
		PUBLIC_HEADER DESTINATION include/nano
	)
ENDIF()

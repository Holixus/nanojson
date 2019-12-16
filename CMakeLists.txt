cmake_minimum_required(VERSION 2.6)

PROJECT(nanojson)

OPTION(JSON_FLOATS "Enabled support of floating point numbers" OFF)
OPTION(JSON_64BITS_INTEGERS "Enable support of 64 bits integers" OFF)
OPTION(JSON_HEX_NUMBERS "Enabled support of 0x integers" OFF)

ADD_DEFINITIONS(-pipe --std=gnu99 -ftabstop=4 -Wno-unused-function)
ADD_DEFINITIONS(-Wall -Wmissing-declarations -Winit-self -Wswitch-enum -Wundef)
ADD_DEFINITIONS(-Wmissing-field-initializers -Wredundant-decls -Wpointer-arith)

ADD_DEFINITIONS(-D_GNU_SOURCE)

configure_file(json.h.in json.h @ONLY)

ADD_EXECUTABLE(tests tests.c)
ADD_LIBRARY(${CMAKE_PROJECT_NAME} STATIC json.c)

TARGET_LINK_LIBRARIES(tests ${CMAKE_PROJECT_NAME})

IF(HOST_DEBUG)
	ADD_DEFINITIONS(-O0 -g3)
ELSE()
	ADD_DEFINITIONS(-Os -Werror -fvisibility=hidden -ffast-math -fmerge-all-constants -ffunction-sections -fdata-sections)
	SET_TARGET_PROPERTIES(tests PROPERTIES LINK_FLAGS -Wl,--gc-sections)
	SET_TARGET_PROPERTIES(${CMAKE_PROJECT_NAME} PROPERTIES LINK_FLAGS -Wl,--gc-sections)
ENDIF()

INSTALL(TARGETS ${CMAKE_PROJECT_NAME} tests
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
)


#ADD_CUSTOM_TARGET(romfs
#	COMMAND $(ROMFSINST) /bin/sysmond
#	COMMAND $(ROMFSINST) /bin/sysmon
#	COMMAND [ \"$(CONFIG_APP_SUPERD)\" = \"y\" ] && echo "super watch sysmond /bin/sysmond -f" > $(DIR_ROMFS)/etc/init.d/startup/2-sysmond
#	COMMAND [ \"$(CONFIG_APP_SUPERD)\" = \"y\" ] || echo "/bin/sysmond" > $(DIR_ROMFS)/etc/init.d/startup/2-sysmond
#)

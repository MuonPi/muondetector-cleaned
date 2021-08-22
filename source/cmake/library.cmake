set(MUONDETECTOR_LIBRARY_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/library/src")
set(MUONDETECTOR_LIBRARY_HEADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/library/include")
set(MUONDETECTOR_LIBRARY_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/library/config")
set(LIBRARY_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/library/include/")


find_package(Qt5 COMPONENTS Network REQUIRED)


configure_file(
    "${MUONDETECTOR_LIBRARY_CONFIG_DIR}/version.h"
    "${MUONDETECTOR_LIBRARY_HEADER_DIR}/version.h"
    )

set(MUONDETECTOR_LIBRARY_SOURCE_FILES
    "${MUONDETECTOR_LIBRARY_SRC_DIR}/config.cpp"
    "${MUONDETECTOR_LIBRARY_SRC_DIR}/tcpconnection.cpp"
    "${MUONDETECTOR_LIBRARY_SRC_DIR}/tcpmessage.cpp"
    "${MUONDETECTOR_LIBRARY_SRC_DIR}/histogram.cpp"
    )

set(MUONDETECTOR_LIBRARY_HEADER_FILES
    "${MUONDETECTOR_LIBRARY_HEADER_DIR}/muondetector_shared_global.h"
    "${MUONDETECTOR_LIBRARY_HEADER_DIR}/gpio_pin_definitions.h"
    "${MUONDETECTOR_LIBRARY_HEADER_DIR}/histogram.h"
    "${MUONDETECTOR_LIBRARY_HEADER_DIR}/tcpconnection.h"
    "${MUONDETECTOR_LIBRARY_HEADER_DIR}/tcpmessage.h"
    "${MUONDETECTOR_LIBRARY_HEADER_DIR}/tcpmessage_keys.h"
    "${MUONDETECTOR_LIBRARY_HEADER_DIR}/ublox_messages.h"
    "${MUONDETECTOR_LIBRARY_HEADER_DIR}/ubx_msg_key_name_map.h"
    "${MUONDETECTOR_LIBRARY_HEADER_DIR}/ublox_structs.h"
    "${MUONDETECTOR_LIBRARY_HEADER_DIR}/muondetector_structs.h"
    "${MUONDETECTOR_LIBRARY_HEADER_DIR}/config.h"
    )

if (MUONDETECTOR_BUILD_DAEMON)
set(MUONDETECTOR_LIBRARY_MQTT_SOURCE_FILES
    "${MUONDETECTOR_LIBRARY_SRC_DIR}/mqtthandler.cpp"
    )
set(MUONDETECTOR_LIBRARY_MQTT_HEADER_FILES
    "${MUONDETECTOR_LIBRARY_HEADER_DIR}/mqtthandler.h"
    )

add_library(muondetector-shared-mqtt OBJECT ${MUONDETECTOR_LIBRARY_MQTT_SOURCE_FILES} ${MUONDETECTOR_LIBRARY_MQTT_HEADER_FILES})
target_compile_definitions(muondetector-shared-mqtt PUBLIC MUONDETECTOR_LIBRARY_EXPORT)
set_target_properties(muondetector-shared-mqtt PROPERTIES POSITION_INDEPENDENT_CODE 1)
target_include_directories(muondetector-shared-mqtt PUBLIC
    ${MUONDETECTOR_LIBRARY_HEADER_DIR}
    )
target_link_libraries(muondetector-shared-mqtt Qt5::Network)
endif ()

add_library(muondetector-shared OBJECT ${MUONDETECTOR_LIBRARY_SOURCE_FILES} ${MUONDETECTOR_LIBRARY_HEADER_FILES})
target_compile_definitions(muondetector-shared PUBLIC MUONDETECTOR_LIBRARY_EXPORT)
set_target_properties(muondetector-shared PROPERTIES POSITION_INDEPENDENT_CODE 1)
target_include_directories(muondetector-shared PUBLIC
    ${MUONDETECTOR_LIBRARY_HEADER_DIR}
    )

if(WIN32)
target_include_directories(muondetector-shared PUBLIC
    $<BUILD_INTERFACE:${QWT_DIR}/include>
    ${Qt5Network_INCLUDE_DIRS}
    )
else()
target_link_libraries(muondetector-shared Qt5::Network)
endif()

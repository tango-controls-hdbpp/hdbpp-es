cmake_minimum_required(VERSION 3.2)

# source files
set(SRC_FILES ${SRC_FILES}
    ${CMAKE_CURRENT_SOURCE_DIR}/AbortableThread.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CheckPeriodicThread.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ClassFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/HdbDevice.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/HdbEventSubscriber.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/HdbEventSubscriberClass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/HdbEventSubscriberStateMachine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PollerThread.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PushThread.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StatsThread.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SubscribeThread.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    PARENT_SCOPE)

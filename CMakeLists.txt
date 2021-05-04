cmake_minimum_required(VERSION 3.13)
cmake_policy(VERSION 3.13)

########################################################################################################################

project(fty_common_mlm
    VERSION 1.0.0
    DESCRIPTION "Provides common Malamute and ZeroMQ tools for agents"
)

########################################################################################################################
find_package(fty-cmake PATHS ${CMAKE_BINARY_DIR}/fty-cmake)
########################################################################################################################

etn_target(shared ${PROJECT_NAME}
    PUBLIC_INCLUDE_DIR include
    PUBLIC
        fty_common_mlm_agent.h
        fty_common_mlm_guards.h
        fty_common_mlm_subprocess.h
        fty_common_mlm_tntmlm.h
        fty_common_mlm_uuid.h
        fty_common_mlm_basic_mailbox_server.h
        fty_common_mlm.h
        fty_common_mlm_stream_client.h
        fty_common_mlm_sync_client.h
        fty_common_mlm_utils.h
        fty_common_mlm_zconfig.h
    SOURCES
        fty_common_mlm_agent.cc
        fty_common_mlm_subprocess.cc
        fty_common_mlm_tntmlm.cc
        fty_common_mlm_uuid.cc
        fty_common_mlm_basic_mailbox_server.cc
        fty_common_mlm_stream_client.cc
        fty_common_mlm_sync_client.cc
        fty_common_mlm_utils.cc
        fty_common_mlm_zconfig.cc
    FLAGS -Wno-logical-op
    USES
        czmq
        cxxtools
        fty_common
        fty_common_logging
        mlm
        openssl
)

set_target_properties(${PROJECT_NAME} PROPERTIES SOVERSION ${PROJECT_VERSION_MAJOR})

########################################################################################################################

etn_test_target(${PROJECT_NAME}
    CONFIGS
        test/conf/test.conf
    SOURCES
        test/basic_mailbox_server.cc
        test/subprocess.cc
        test/tntmlm.cc
        test/utils.cc
        test/uuid.cc
        test/main.cpp
    USES
        pthread
    SUBDIR
        test
)

########################################################################################################################


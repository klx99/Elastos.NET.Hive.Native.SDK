project(libcrystal)

include(HiveDefaults)
include(ExternalProject)
include(ExternalCMakeArgs)

ExternalProject_Add(
    libcrystal

    PREFIX ${HIVE_DEPS_BUILD_PREFIX}
    URL "https://github.com/iwhisperio/libcrystal/archive/release-v1.0.5.tar.gz"
    URL_HASH SHA256=6256713f34bb341d718a38b5f3a108c6d9c1db93e6391f670108de1cd09b3cee
    DOWNLOAD_NAME "libcrystal-v1.0.5.tar.gz"
    DOWNLOAD_DIR ${HIVE_DEPS_TARBALL_DIR}
    DOWNLOAD_NO_PROGRESS 1

    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${HIVE_INT_DIST_DIR}
        -DENABLE_STATIC=${ENABLE_STATIC}
        -DENABLE_SHARED=${ENABLE_SHARED}
        -DENABLE_CRYPTO=FALSE
        -DENABLE_TESTS=FALSE
        ${CMAKE_ARGS_INIT}
        -DCMAKE_BUILD_TYPE=DEBUG
)

if(ENABLE_SHARED)
    if(WIN32)
        set(INSTALL_DESTINATION bin)
    else()
        set(INSTALL_DESTINATION lib)
    endif()

    string(CONCAT SHARED_LIBRARY_NAME
        "${HIVE_INT_DIST_DIR}/${INSTALL_DESTINATION}/"
        "${CMAKE_SHARED_LIBRARY_PREFIX}"
        "crystal"
        "${CMAKE_SHARED_LIBRARY_SUFFIX}")

    file(RELATIVE_PATH SHARED_LIBRARY_NAME ${CMAKE_CURRENT_LIST_DIR}
        ${SHARED_LIBRARY_NAME})

    install(PROGRAMS "${SHARED_LIBRARY_NAME}"
        DESTINATION ${INSTALL_DESTINATION})
endif()

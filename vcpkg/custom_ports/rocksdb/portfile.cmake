vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO facebook/rocksdb
  REF v6.6.4
  SHA512 22f5105244c55ca7030cc8d7ee06cb87f4afdfc85b3f44f7512a9c9c41c037c3b70fd291ec1b0df2553a93b770e11abc4ae2d7f984b2a0f1addd26d424c09539
  HEAD_REF master
  PATCHES
    0001-disable-gtest.patch
    0002-only-build-one-flavor.patch
    0003-use-find-package.patch
    0004-static-linking-in-linux.patch
    0005-add-config-to-findpackage.patch
)

file(REMOVE "${SOURCE_PATH}/cmake/modules/Findzlib.cmake")
file(COPY
  "${CMAKE_CURRENT_LIST_DIR}/Findlz4.cmake"
  "${CMAKE_CURRENT_LIST_DIR}/Findsnappy.cmake"
  "${CMAKE_CURRENT_LIST_DIR}/Findzstd.cmake"
  DESTINATION "${SOURCE_PATH}/cmake/modules"
)

string(COMPARE EQUAL "${VCPKG_CRT_LINKAGE}" "dynamic" WITH_MD_LIBRARY)
string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "static" ROCKSDB_DISABLE_INSTALL_SHARED_LIB)
string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "dynamic" ROCKSDB_DISABLE_INSTALL_STATIC_LIB)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
  FEATURES
    "lz4"     WITH_LZ4
    "snappy"  WITH_SNAPPY
    "zlib"    WITH_ZLIB
    "zstd"    WITH_ZSTD
    "tbb"     WITH_TBB
  INVERTED_FEATURES
    "tbb"     CMAKE_DISABLE_FIND_PACKAGE_TBB
)

vcpkg_configure_cmake(
  SOURCE_PATH ${SOURCE_PATH}
  PREFER_NINJA
  OPTIONS
  -DWITH_GFLAGS=0
  -DWITH_TESTS=OFF
  -DUSE_RTTI=1
  -DROCKSDB_INSTALL_ON_WINDOWS=ON
  -DWITH_WINDOWS_UTF8_FILENAMES=TRUE
  -DFAIL_ON_WARNINGS=OFF
  -DWITH_MD_LIBRARY=${WITH_MD_LIBRARY}
  -DPORTABLE=ON
  -DCMAKE_DEBUG_POSTFIX=d
  -DROCKSDB_DISABLE_INSTALL_SHARED_LIB=${ROCKSDB_DISABLE_INSTALL_SHARED_LIB}
  -DROCKSDB_DISABLE_INSTALL_STATIC_LIB=${ROCKSDB_DISABLE_INSTALL_STATIC_LIB}
  -DCMAKE_DISABLE_FIND_PACKAGE_NUMA=TRUE
  -DCMAKE_DISABLE_FIND_PACKAGE_gtest=TRUE
  -DCMAKE_DISABLE_FIND_PACKAGE_Git=TRUE
  ${FEATURE_OPTIONS}
)

vcpkg_install_cmake()

vcpkg_fixup_cmake_targets(CONFIG_PATH lib/cmake/rocksdb)

file(REMOVE_RECURSE ${CURRENT_PACKAGES_DIR}/debug/include)

file(INSTALL ${SOURCE_PATH}/LICENSE.Apache DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)
file(COPY ${CMAKE_CURRENT_LIST_DIR}/vcpkg-cmake-wrapper.cmake ${SOURCE_PATH}/LICENSE.leveldb DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT})

vcpkg_copy_pdbs()

vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO TanninOne/pagan.expr
  REF 47612a33eab32f50d9c67b4d846dbadf145ea3ea
  SHA512 de74fb3c9bb277ecf2d5f39f6278098b574a65b02c88ba6cf1709da205a5c0b2c783d6bdc6643038e5ece1ac510cd4acbedffde5ca8f7fc3f7a1d75e0cfa9f49
  HEAD_REF main
)

vcpkg_cmake_configure(
  SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup()

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO TanninOne/pagan.expr
  REF 4ab5c64aa6453b4df367b3e70fe993cab3bd1d8d
  SHA512 4e71bf1df8b8fb84b2c69c816e064e57da9a112b20cd4d0d25753419fcfee6ff09a6046054f381a1c72c8273e503cc3f2c8443f96737a75905cd7bad77f2b059
  HEAD_REF main
)

vcpkg_cmake_configure(
  SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_cmake_install()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_cmake_config_fixup()

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

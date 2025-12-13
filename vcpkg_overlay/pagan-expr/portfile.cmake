vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO TanninOne/pagan.expr
  REF 25dd51f42cb78b4553f9eb9237f458f5bcbc7210
  SHA512 2c8656998cd8fa6cfa018d580bc97d9a1cf8c2314059937a4538a433fe5bf905aad4a6c062b394b2e31db2928aa9411495cd96fd3dac2622ad534c5287f4fda0
  HEAD_REF main
)

vcpkg_cmake_configure(
  SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_cmake_install()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_cmake_config_fixup()

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

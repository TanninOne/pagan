vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO TanninOne/pagan.expr
  REF 9715ad2875211148cafb6b3b0e050c3e933d38ae
  SHA512 26c58644fbfe13cc304646cef5854e2093efac99b2226082082556025394d2424e92f1364062e52b6de889d68bb8569e4bec012cf71d0f26a16f46a0355643ad
  HEAD_REF main
)

vcpkg_cmake_configure(
  SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_cmake_install()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_cmake_config_fixup()

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

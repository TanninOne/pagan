vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO TanninOne/pagan.expr
  REF b8b2d7a7ca342a39bad1d949bd51994b18133265
  SHA512 6acafe93a0ab9dba0ce897b5692348f3676c01e441829b01618224eb1b30f6443423d0661ef94f6cbd15ad8b2190c39634608035ee84c502b29dcf99353f8514
  HEAD_REF main
)

vcpkg_cmake_configure(
  SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_cmake_install()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_cmake_config_fixup()

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

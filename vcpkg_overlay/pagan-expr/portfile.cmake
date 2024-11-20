vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO TanninOne/pagan.expr
  REF 4d1b84afe3cdb2a42406b8aef60b6e2db427e87d
  SHA512 0bab46a2440d6b07860067073d784e1f81a63fdb36ebf826e3576560b682e6ada86ce63aaa5b98ef664201067178926fd1e6bcf185db43cf8c0723b91d087547
  HEAD_REF main
)

vcpkg_cmake_configure(
  SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_cmake_install()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_cmake_config_fixup()

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

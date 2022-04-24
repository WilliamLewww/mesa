test(
  'test_instance',
  executable(
    'test_instance',
    'test_instance.cpp',
    include_directories : [
      inc_include, inc_src
    ],
    link_with : [
      libvulkan_playground
    ],
    dependencies : [
      idep_vulkan_util_headers
    ],
  ),
  suite : ['playground']
)

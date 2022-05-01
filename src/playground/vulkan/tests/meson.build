vulkan_dep = dependency('vulkan')

test_files = [
  'test_instance.cpp',
  'test_physical_device.cpp'
]

foreach test_file : test_files
  test(
    test_file.split('.')[0],
    executable(
      test_file.split('.')[0],
      test_file,
      include_directories : [
        inc_include, inc_src
      ],
      link_with : [
        libvulkan_playground
      ],
      dependencies : [
        vulkan_dep
      ],
    ),
    suite : ['playground']
  )
endforeach

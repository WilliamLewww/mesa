playground_icd = custom_target(
  'playground_icd',
  input : [vk_icd_gen, vk_api_xml],
  output : 'playground_icd.@0@.json'.format(host_machine.cpu()),
  command : [
    prog_python, '@INPUT0@',
    '--api-version', '1.3', '--xml', '@INPUT1@',
    '--lib-path', join_paths(get_option('prefix'), get_option('libdir'),
                             'libvulkan_playground.so'),
    '--out', '@OUTPUT@',
  ],
  build_by_default : true,
  install_dir : with_vulkan_icd_dir,
  install : true,
)

pvk_files = files(
  'pvk_private.h',
  'pvk_device.c',
  'pvk_debug.h',
  'pvk_debug.c'
)

pvk_deps = [
  dep_libdrm_amdgpu,
  idep_vulkan_runtime,
  idep_vulkan_util,
  idep_nir
]

pvk_entrypoints = custom_target(
  'pvk_entrypoints',
  input : [vk_entrypoints_gen, vk_api_xml],
  output : ['pvk_entrypoints.h', 'pvk_entrypoints.c'],
  command : [
    prog_python, '@INPUT0@', '--xml', '@INPUT1@', '--proto', '--weak',
    '--out-h', '@OUTPUT0@', '--out-c', '@OUTPUT1@', '--prefix', 'pvk',
  ],
  depend_files : vk_entrypoints_gen_depend_files,
)

libvulkan_playground = shared_library(
  'vulkan_playground',
  [pvk_entrypoints, pvk_files],
  include_directories : [inc_include, inc_src],
  dependencies : pvk_deps,
  gnu_symbol_visibility : 'hidden',
  install : true,
)


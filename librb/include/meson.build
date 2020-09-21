configure_file(input: 'librb_config.h.meson',
  output: 'librb_config.h',
  configuration: cdata)

configure_file(input: 'librb-config.h.meson',
  output: 'librb-config.h',
  configuration: cdata)

librb_serno_h = custom_target(
  'serno.h',
  input: 'serno.h.SH',
  output: 'serno.h',
  command: ['sh', '@INPUT@', '@OUTPUT@']
)

librb_inc = include_directories('.')

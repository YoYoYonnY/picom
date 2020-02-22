libev = dependency('libev', required: false)
if not libev.found()
	libev = cc.find_library('ev')
endif
base_deps = [
	cc.find_library('m'),
	libev
]

srcs = [ files('picom.c', 'win.c', 'c2.c', 'x.c', 'config.c', 'vsync.c',
               'diagnostic.c', 'log.c', 'options.c', 'event.c',
               'atom.c', 'file_watch.c', 'module.c') ]
subdir('utils')

picom_inc = include_directories('.')

cflags = []

required_xcb_packages = [
	'xcb-render', 'xcb-damage', 'xcb-randr', 'xcb-sync', 'xcb-composite',
	'xcb-shape', 'xcb-xinerama', 'xcb-xfixes', 'xcb-present', 'xcb'
]

required_packages = [
	'x11', 'x11-xcb', 'xcb-renderutil', 'xcb-image', 'xext', 'pixman-1'
]

foreach i : required_packages
	base_deps += [dependency(i, required: true)]
endforeach

foreach i : required_xcb_packages
	base_deps += [dependency(i, version: '>=1.12.0', required: true)]
endforeach

if not cc.has_header('uthash.h')
  error('Dependency uthash not found')
endif

deps = []

if get_option('stacktraces') and get_option('buildtype') != 'release'
	libbacktrace = cc.find_library('backtrace', required: false)
	if libbacktrace.found()
		deps += libbacktrace
		cflags += ['-DCONFIG_STACKTRACES']
	endif
	srcs += ['stacktrace.c']
endif

if get_option('config_file')
	deps += [dependency('libconfig', version: '>=1.4', required: true),
	         dependency('libxdg-basedir', required: true)]
	cflags += ['-DCONFIG_LIBCONFIG']
	srcs += [ 'config_libconfig.c' ]
endif
if get_option('regex')
	pcre = dependency('libpcre', required: true)
	cflags += ['-DCONFIG_REGEX_PCRE']
	if pcre.version().version_compare('>=8.20')
		cflags += ['-DCONFIG_REGEX_PCRE_JIT']
	endif
	deps += [pcre]
endif

if get_option('vsync_drm')
	cflags += ['-DCONFIG_VSYNC_DRM']
	deps += [dependency('libdrm', required: true)]
endif

if get_option('opengl')
	cflags += ['-DCONFIG_OPENGL', '-DGL_GLEXT_PROTOTYPES']
	deps += [dependency('gl', required: true)]
	srcs += [ files('opengl.c') ]
endif
if get_option('compton_compat')
	cflags += ['-DCONFIG_COMPTONCOMPAT']
	subdir('compton-compat')
endif
if get_option('modules')
	cflags += ['-DCONFIG_MODULES']
	subdir('module')
endif

if get_option('xrescheck')
	cflags += ['-DDEBUG_XRC']
	srcs += [ 'xrescheck.c' ]
endif

if get_option('unittest')
	cflags += ['-DUNIT_TEST']
endif

host_system = host_machine.system()
if host_system == 'linux'
	cflags += ['-DHAS_INOTIFY']
elif host_system == 'freebsd' or host_system == 'netbsd' or
     host_system == 'dragonfly' or host_system == 'openbsd'
	cflags += ['-DHAS_KQUEUE']
endif

subdir('backend')

picom = executable('picom', srcs, c_args: cflags,
  dependencies: [ base_deps, deps, test_h_dep ],
  install: true, include_directories: picom_inc)

if get_option('unittest')
	test('picom unittest', picom, args: [ '--unittest' ])
endif

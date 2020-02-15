srcs += [ files('blur/module.c', 'dbus/module.c', 'fading/module.c', 'shadow/module.c', 'transparency/module.c') ]

if get_option('dbus')
	cflags += ['-DCONFIG_DBUS']
	deps += [dependency('dbus-1', required: true)]
	srcs += [ files('dbus/dbus.c') ]
endif


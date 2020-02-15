srcs += [ files('blur/module.c', 'dbus/module.c', 'fade/module.c', 'filter/module.c', 'shadow/module.c', 'trans/module.c') ]

if get_option('dbus')
	cflags += ['-DCONFIG_DBUS']
	deps += [dependency('dbus-1', required: true)]
	srcs += [ files('dbus/dbus.c') ]
endif

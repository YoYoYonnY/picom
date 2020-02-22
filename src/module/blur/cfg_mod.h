OPTION(bool,             enabled,              cfg_type_bool,        false)
OPTION(bool,             fixed_strength,       cfg_type_bool,        false)
/// Whether to blur background when the window frame is not opaque.
/// Implies cfg_window.background.
OPTION(bool,             background_frame,     cfg_type_bool,        false)
/// Whether to use fixed blur strength instead of adjusting according
/// to window opacity.
OPTION(bool,             background_fixed,     cfg_type_bool,        false)
/// Background blur blacklist. A linked list of conditions.
OPTION(c2_lptr_t*,       background_blacklist, cfg_type_pointer,     NULL)
/// Blur method for background of semi-transparent windows
OPTION(enum blur_method, method,               cfg_type_blur_method, BLUR_METHOD_NONE)
/// Size of the blur kernel (gaussian blur + box blur)
OPTION(int,              radius,               cfg_type_int,         -1)
/// Standard deviation (gaussian blur)
OPTION(double,           deviation,            cfg_type_float,       0.84089642)
/// Blur convolution kernel (kernel blur).
OPTION(struct conv **,   kernels,              cfg_type_pointer,     NULL)
/// Number of convolution kernels (kernel blur)
OPTION(int,              kernel_count,         cfg_type_int,         0)

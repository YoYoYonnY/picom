struct gaussian_blur_args {
	int size;
	double deviation;
};

struct box_blur_args {
	int size;
};

struct kernel_blur_args {
	struct conv **kernels;
	int kernel_count;
};

static struct conv **generate_box_blur_kernel(struct box_blur_args *args, int *kernel_count) {
	int r = args->size * 2 + 1;
	assert(r > 0);
	auto ret = ccalloc(2, struct conv *);
	ret[0] = cvalloc(sizeof(struct conv) + sizeof(double) * (size_t)r);
	ret[1] = cvalloc(sizeof(struct conv) + sizeof(double) * (size_t)r);
	ret[0]->w = r;
	ret[0]->h = 1;
	ret[1]->w = 1;
	ret[1]->h = r;
	for (int i = 0; i < r; i++) {
		ret[0]->data[i] = 1;
		ret[1]->data[i] = 1;
	}
	*kernel_count = 2;
	return ret;
}

static struct conv **
generate_gaussian_blur_kernel(struct gaussian_blur_args *args, int *kernel_count) {
	int r = args->size * 2 + 1;
	assert(r > 0);
	auto ret = ccalloc(2, struct conv *);
	ret[0] = cvalloc(sizeof(struct conv) + sizeof(double) * (size_t)r);
	ret[1] = cvalloc(sizeof(struct conv) + sizeof(double) * (size_t)r);
	ret[0]->w = r;
	ret[0]->h = 1;
	ret[1]->w = 1;
	ret[1]->h = r;
	for (int i = 0; i <= args->size; i++) {
		ret[0]->data[i] = ret[0]->data[r - i - 1] =
		    1.0 / (sqrt(2.0 * M_PI) * args->deviation) *
		    exp(-(args->size - i) * (args->size - i) /
		        (2 * args->deviation * args->deviation));
		ret[1]->data[i] = ret[1]->data[r - i - 1] = ret[0]->data[i];
	}
	*kernel_count = 2;
	return ret;
}

/// Generate blur kernels for gaussian and box blur methods. Generated kernel is not
/// normalized, and the center element will always be 1.
static struct conv **generate_blur_kernel(enum blur_method method, void *args, int *kernel_count) {
	switch (method) {
	case BLUR_METHOD_BOX: return generate_box_blur_kernel(args, kernel_count);
	case BLUR_METHOD_GAUSSIAN:
		return generate_gaussian_blur_kernel(args, kernel_count);
	default: break;
	}
	return NULL;
}
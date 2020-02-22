#include "log.h"

struct log_target;

struct log {
	struct log_target *head;

	int log_level;
};

struct log_target {
	const struct log_ops *ops;
	struct log_target *next;
};

struct log_ops {
	void (*write)(struct log_target *, const char *, size_t);
	void (*writev)(struct log_target *, const struct iovec *, int vcnt);
	void (*destroy)(struct log_target *);

	/// Additional strings to print around the log_level string
	const char *(*colorize_begin)(enum log_level);
	const char *(*colorize_end)(enum log_level);
};

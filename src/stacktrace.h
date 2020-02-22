#pragma once
#include <stddef.h>
#include <backtrace.h>
#include <sys/uio.h>

struct log_target;

struct stacktrace_data {
	int depth;
	struct log_target *target;
	int fd;
};

extern void initialize_stacktraces(void);
extern void print_stacktrace(struct stacktrace_data *data);

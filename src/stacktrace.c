#include "stacktrace.h"
#include "utils/compiler.h"
#include "log-internal.h"
#include "utils/utils.h"

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

static struct backtrace_state *backtrace_state = NULL;

static void backtrace_err_cb(void *vdata, const char *msg, attr_unused int errnum)
{
	if (vdata) {
		struct stacktrace_data *data = vdata;
		fprintf(stderr, "  #%-3d ??? (%s)\n", data->depth, msg);
		data->depth++;
	}
}
static int backtrace_full_cb(void *vdata, uintptr_t pc, const char *filename,
	int lineno, const char *function)
{
	struct stacktrace_data *data = vdata;
	static char buffer[1024]; /* Should be more than enough */
	int buffersz = snprintf(buffer, ARR_SIZE(buffer),
		"  #%-3d [0x%016zx] %s() in %s:%d\n", data->depth, pc,
		function, filename, lineno);
	if (data->target) {
		data->target->ops->writev(
	    		data->target,
	    		(struct iovec[]){
			    	{.iov_base = buffer, .iov_len = (unsigned long)buffersz} },
	    		1);
	} else if (data->fd != -1) {
		write(data->fd, buffer, (size_t)buffersz);
	}
	data->depth++;
	return 0;
}

void initialize_stacktraces(void) {
	static char buffer[PATH_MAX];

	if (backtrace_state == NULL) {
		backtrace_state = backtrace_create_state(realpath("/proc/self/exe", buffer), 0, backtrace_err_cb, NULL);
	}
}
void print_stacktrace(struct stacktrace_data *data)
{
	if (backtrace_state && data) {
		int skip = data ? data->depth : 0;
		data->depth = 0;
		backtrace_full(backtrace_state, skip, backtrace_full_cb,
					backtrace_err_cb, data);
	}
}

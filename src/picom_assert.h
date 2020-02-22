#include <assert.h>
#ifdef CONFIG_STACKTRACES
#include <unistd.h>
#include <stdlib.h>

#include "utils/meta.h"
#include "stacktrace.h"

#undef assert
#define assert(expr) \
	do { \
		if (!(expr)) { \
			fprintf(stderr, "%s:%d %s: Assertion `%s' failed:\n", \
				__FILE__, __LINE__, \
				__func__, #expr); \
			print_stacktrace(&(struct stacktrace_data){ \
				.depth = 0, \
				.fd = STDERR_FILENO, \
				.target = NULL \
			}); \
			abort(); \
		} \
	} while (0)
#endif

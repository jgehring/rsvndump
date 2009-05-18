#include <stdlib.h>

#include <svn_delta.h>
#include <svn_io.h>
#include <svn_pools.h>

#include <apr_file_io.h>


int main(int argc, char **argv)
{
	svn_txdelta_stream_t *stream;
	svn_txdelta_window_handler_t handler;
	void *handler_baton;
	svn_stream_t *source, *delta, *dest, *din;
	apr_file_t *source_file = NULL, *delta_file = NULL;
	apr_pool_t *pool;
	char *in1, *in2;
	svn_error_t *err;

	if (argc < 3) {
		printf("%s <file1> <diff>\n", argv[0]);
		return 1;
	}

	svn_cmdline_init("svndiffgen", stderr);
	pool = svn_pool_create(NULL);

	in1 = argv[1];
	in2 = argv[2];

	/* Open streams */
	apr_file_open(&source_file, in1, APR_READ, 0600, pool);
	source = svn_stream_from_aprfile2(source_file, FALSE, pool);
	svn_stream_for_stdout(&dest, pool);
	apr_file_open(&delta_file, in2, APR_READ, 0600, pool);
	delta = svn_stream_from_aprfile2(delta_file, FALSE, pool);

	/* Setup handler and parser */
	svn_txdelta_apply(source, dest, NULL, NULL, pool, &handler, &handler_baton);
	din = svn_txdelta_parse_svndiff(handler, handler_baton, TRUE, pool);

	err = svn_stream_copy(delta, din, pool);

	svn_pool_destroy(pool);
	if (err) {
		svn_handle_error2(err, stderr, FALSE, "ERROR: ");
		return 1;
	}
	return 0;
}


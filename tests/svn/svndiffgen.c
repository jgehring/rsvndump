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
	svn_stream_t *source, *target, *dest;
	apr_file_t *source_file = NULL, *target_file = NULL;
	apr_pool_t *pool;
	char *in1, *in2;
	svn_error_t *err;

	if (argc < 3) {
		printf("%s <file1> <file2>\n", argv[0]);
		return 1;
	}

	svn_cmdline_init("svndiffgen", stderr);
	pool = svn_pool_create(NULL);

	in1 = argv[1];
	in2 = argv[2];

	/* Open source and target */
	apr_file_open(&target_file, in2, APR_READ, 0600, pool);
	target = svn_stream_from_aprfile2(target_file, FALSE, pool);
	apr_file_open(&source_file, in1, APR_READ, 0600, pool);
	source = svn_stream_from_aprfile2(source_file, FALSE, pool);

	svn_stream_for_stdout(&dest, pool);

	/* Produce delta in svndiff format */
	svn_txdelta(&stream, source, target, pool);
	svn_txdelta_to_svndiff2(&handler, &handler_baton, dest, 0, pool);

	err = svn_txdelta_send_txstream(stream, handler, handler_baton, pool);
	svn_pool_destroy(pool);
	if (err) {
		svn_handle_error2(err, stderr, FALSE, "ERROR: ");
		return 1;
	}
	return 0;
}

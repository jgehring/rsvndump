CC            = gcc
SVN_CFLAGS    = -I/usr/include/subversion-1
SVN_LIBS      = -L/usr/lib -lsvn_client-1
APR_CFLAGS    = `pkg-config --cflags apr-1`
APR_LFLAGS    = `pkg-config --libs apr-1`

all: svndiffgen svndiffapply

svndiffgen: svndiffgen.c
	$(CC) -o svndiffgen $(SVN_CFLAGS) $(APR_CFLAGS) $(SVN_LIBS) $(APR_LIBS) svndiffgen.c

svndiffapply: svndiffapply.c
	$(CC) -o svndiffapply $(SVN_CFLAGS) $(APR_CFLAGS) $(SVN_LIBS) $(APR_LIBS) svndiffapply.c

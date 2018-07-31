/*
 * harness-hostdir.c
 *
 * DXPB repository file transfer - test harness
 */

#define _POSIX_C_SOURCE 200809L

#include <sys/stat.h>
#include <bsd/stdlib.h>
#include <czmq.h>
#include <xbps.h>
#include "dxpb.h"
#include "dxpb-client.h"
#include "bfs.h"
#include "bstring.h"
#include "bwords.h"
#include "bxpkg.h"
#include "bgraph.h"
#include "bxbps.h"
#include "pkggraph_msg.h"
#include "pkgfiles_msg.h"
#include "pkggraph_filer.h"
#include "pkgfiler.h"

#include <openssl/sha.h>

#include "dxpb-common.h"
#include "dxpb-version.h"

// This is the largest number of 4-byte integers we can expect in a file.
// The lucky package that is this massive is `ceph-dbg`. Since this is 2.5 GB,
// this may not be desireable for tests, but is necessary just to be sure.
// But we have now shrunk the size to something we can put under a 4 GB tmp
// 510000000
// Now shrunk by 2
#define LARGEST_SIZE 51000000

int
sock_ssl_setup(zsock_t *sock, const char *mysec, const char *mypub, const char *servpub)
{
	zsock_set_curve_secretkey(sock, mysec);
	zsock_set_curve_publickey(sock, mypub);
	zsock_set_curve_serverkey(sock, servpub);
	return 0;
}

void
forkoff_master(const char *ssldir, const char *sdir, const char *rdir,
		const char *ldir, const char *file_end,
		const char *graph_end, const char *file_pub,
		const char *graph_pub)
{
	switch(fork()) {
	case 0:
		execlp("./dxpb-hostdir-master", "dxpb-hostdir-master",
				"-r", rdir, "-l", ldir,
				"-s", sdir, "-g", graph_end, "-G", graph_pub,
				"-f", file_end, "-F", file_pub, "-k", ssldir,
				NULL);
		exit(0);
	case -1:
		exit(ERR_CODE_BAD);
	default:
		return;
	}
}

void
forkoff_remote(const char *ssldir, const char *hostdir, const char *file_end)
{
	switch(fork()) {
	case 0:
		execlp("./dxpb-hostdir-remote", "dxpb-hostdir-remote",
				"-r", hostdir, "-f", file_end, "-k", ssldir,
				NULL);
		exit(0);
	case -1:
		exit(ERR_CODE_BAD);
	default:
		return;
	}
}

int
run(const char *ssldir, const char *sdir, const char *rdir, const char *ldir,
		const char *remdir1, const char *remdir2,
		const char *file_end, const char *graph_end,
		const char *file_pub, const char *graph_pub)
{
	forkoff_remote(ssldir, remdir2, file_end);
	(void) file_pub;
	(void) graph_pub;
	assert(sdir);
	assert(rdir);
	assert(ldir);
	assert(remdir1);
	assert(remdir2);
	assert(file_end);
	assert(graph_end);
	assert(file_pub);
	assert(graph_pub);
	assert(ssldir);

	char *repopath1 = bstring_add(bstring_add(NULL, remdir1, NULL, NULL),
			"/", NULL, NULL);
	char *repopath2 = bstring_add(bstring_add(NULL, remdir2, NULL, NULL),
			"/", NULL, NULL);

	char *remdir[2] = {repopath1, repopath2};

	zhash_t *todo = zhash_new();

	int rc;

	pkgfiles_msg_t *file_msg = pkgfiles_msg_new();

	zsock_t *file  = zsock_new (ZMQ_DEALER);
	assert(file);

	rc = setup_ssl(file, (setssl_cb)sock_ssl_setup, "dxpb-hostdir-remote", "dxpb-hostdir-master", ssldir);
	assert(rc == 0);

	zsock_attach(file, file_end, false);

#define SEND(this, msg, sock)        { \
					pkgfiles_msg_set_id(msg, this); \
					rc = pkgfiles_msg_send(msg, sock); \
					assert(rc == 0); \
				}

#define GET(mymsg, sock)        { \
					zpoller_t *p = zpoller_new(sock, NULL); \
					(void) zpoller_wait(p, -1); \
					assert(!zpoller_expired(p)); \
					if (zpoller_terminated(p)) \
					exit(-1); \
					rc = pkgfiles_msg_recv(mymsg, sock); \
					assert(rc == 0); \
					zpoller_destroy(&p); \
				}

#define DOPING(msg,sock)	{ \
					for (int i = 0; i < 2; i++) { \
						sleep(10); \
						SEND(TOMSG(PING), msg, sock); \
						GET(msg, sock); \
						ASSERTMSG(id, msg, TOMSG(ROGER)); \
					} \
				}

#define DOPINGRACE(msg,sock)	{ \
					for (int i = 0; i < 2; i++) { \
						sleep(10); \
						SEND(TOMSG(PING), msg, sock); \
						GET(msg, sock); \
						ASSERTMSGOR(id, msg, TOMSG(ROGER), TOMSG(PKGHERE)); \
					} \
				}

#define WRITEFILE(path, iterations)	{ \
						PRINTUP; \
						printf("Writing to path: %s\n", path); \
						FILE *fp = fopen(path, "wb"); \
						for (uint64_t i = 0; i < iterations / sizeof(uint32_t); i++) { \
							fprintf(fp, "%uld", arc4random()); \
						} \
						fclose(fp); hash = xbps_file_hash(path); puts(hash); \
					}

#define WRITEPKG(dir, pkgname, arch) { \
						char *tmp = bstring_add(strdup(dir), bxbps_pkg_to_filename(pkgname, DXPB_VERSION, arch), NULL, NULL); \
						assert(tmp); \
						WRITEFILE(tmp, 1024*64*2); \
						free(tmp); \
					}

#define PRINTUP			printf("****************************\n*****> WRITE FILE <*********\n****************************\n");
#define TOMSG(str)                   PKGFILES_MSG_##str
#define SETMSG(what, msg, to)        pkgfiles_msg_set_##what(msg, (to))
#define ASSERTMSG(what, msg, eq)     assert(pkgfiles_msg_##what(msg) == eq)
#define ASSERTMSGOR(what, msg, eq, eq2)     assert(pkgfiles_msg_##what(msg) == eq || pkgfiles_msg_##what(msg) == eq2)
#define ASSERTMSGSTR(what, msg, eq)     assert(strcmp(pkgfiles_msg_##what(msg), eq) == 0)
#define ASSERTMSGSTROR(what, msg, eq, eq2)     assert(strcmp(pkgfiles_msg_##what(msg), eq) == 0 || strcmp(pkgfiles_msg_##what(msg), eq2) == 0)

	/* And now we get to work */

	for (int i = 0; i < 10; i++) {
		char *hash = malloc(sizeof(SHA256_DIGEST_LENGTH));
		assert(hash);
		char name[5] = "foo0";
		name[3] = '0' + i;
		WRITEPKG(remdir[i%2], name, "noarch");
		zhash_insert(todo, name, hash);
		hash = malloc(sizeof(SHA256_DIGEST_LENGTH));
		assert(hash);
		char name2[5] = "bar0";
		name2[3] = '0' + i;
		WRITEPKG(remdir[i%2], name2, "noarch");
		zhash_insert(todo, name2, hash);
	}

	SEND(TOMSG(HELLO), file_msg, file);
	GET(file_msg, file);
	ASSERTMSG(id, file_msg, TOMSG(ROGER));

	zlist_t *newnames = zhash_keys(todo);

	DOPING(file_msg, file);

	SETMSG(version, file_msg, DXPB_VERSION);
	SETMSG(arch, file_msg, "noarch");
	for (const char *tmp = zlist_first(newnames); tmp; tmp = zlist_next(newnames)) {
		SETMSG(pkgname, file_msg, tmp);
		SEND(TOMSG(ISPKGHERE), file_msg, file);
	}

	size_t todo_size = zhash_size(todo);
	for (size_t i = 0; i < todo_size; i++) {
		SEND(TOMSG(PING), file_msg, file);
		for (int k = 0; k < 2; k++) {
			GET(file_msg, file);

			switch (pkgfiles_msg_id(file_msg)) {
				case TOMSG(ROGER):
					break;
				case TOMSG(PKGHERE):
					assert(1 == 1);
					char *pkgpath = bstring_add(strdup(rdir), pkgfiles_msg_pkgname(file_msg), NULL, NULL);
					assert(xbps_file_hash_check(pkgpath, zhash_lookup(todo, pkgfiles_msg_pkgname(file_msg))));
					zhash_delete(todo, pkgfiles_msg_pkgname(file_msg));
					break;
			}
		}
	}

	/* Work over, let's clean up. */

	zhash_destroy(&todo);

	pkgfiles_msg_destroy(&file_msg);

	zsock_destroy(&file);

	return 0;
}

int
main(int argc, char * const *argv)
{
	(void) argc;
	char *tmppath = strdup("/tmp/dxpb-harness-XXXXXX");
	char *ourdir = mkdtemp(tmppath);
	assert(ourdir);
	char *logpath = bstring_add(bstring_add(NULL, ourdir, NULL, NULL), "/logs/", NULL, NULL);
	char *stagepath = bstring_add(bstring_add(NULL, ourdir, NULL, NULL), "/staging/", NULL, NULL);
	char *repopath = bstring_add(bstring_add(NULL, ourdir, NULL, NULL), "/trgtdir/", NULL, NULL);
	char *remotepath1 = bstring_add(bstring_add(NULL, ourdir, NULL, NULL), "/remote1/", NULL, NULL);
	char *remotepath2 = bstring_add(bstring_add(NULL, ourdir, NULL, NULL), "/remote2/", NULL, NULL);
	char *ssldir = bstring_add(bstring_add(NULL, ourdir, NULL, NULL), "/ssl/", NULL, NULL);

	char *file_endpoint = "tcp://127.0.0.1:15953";
	char *file_pubpoint = "tcp://127.0.0.1:15954";
	char *graph_endpoint = "tcp://127.0.0.1:15955";
	char *graph_pubpoint = "tcp://127.0.0.1:15956";

	int rc = mkdir(logpath, S_IRWXU);
	rc = mkdir(stagepath, S_IRWXU);
	rc = mkdir(repopath, S_IRWXU);
	rc = mkdir(remotepath1, S_IRWXU);
	rc = mkdir(remotepath2, S_IRWXU);
	rc = mkdir(ssldir, S_IRWXU);

	prologue(argv[0]);

	char create_certs_cmd[1024];
	snprintf(create_certs_cmd, 1024, "./%s -k %s -n %s", "dxpb-certs-remote", ssldir, "dxpb-hostdir-master");
	rc = system(create_certs_cmd);
	assert(rc == 0);
	snprintf(create_certs_cmd, 1024, "./%s -k %s -n %s", "dxpb-certs-remote", ssldir, "dxpb-hostdir-remote");
	rc = system(create_certs_cmd);
	assert(rc == 0);
	snprintf(create_certs_cmd, 1024, "./%s -k %s -n %s", "dxpb-certs-remote", ssldir, "dxpb-frontend");
	rc = system(create_certs_cmd);
	assert(rc == 0);

	puts("\n\nThis is a test harness.\nConducting tests....\n\n");

	forkoff_master(ssldir, stagepath, repopath, logpath,
		file_endpoint, graph_endpoint, file_pubpoint, graph_pubpoint);
	forkoff_remote(ssldir, remotepath1, file_endpoint);
	sleep(5);
	return run(ssldir, stagepath, repopath, logpath, remotepath1, remotepath2,
		file_endpoint, graph_endpoint, file_pubpoint, graph_pubpoint);
}
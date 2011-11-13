#include <stdio.h>
#include <stdlib.h>
#include <ixp.h>
#include "fns.h"

#define fatal(...) ixp_eprint(__VA_ARGS__)

extern Ixp9Srv p9srv;
static IxpServer server;

int main(int argc, char **argv) {
	int fd;
	int ret;
	char *mountaddr;
	char *address;
	char *msg;
	IxpConn *acceptor;

	if (argc != 2) {
		fprintf(stderr, "usage: %s file.xml\n", argv[0]);
		return -1;
	}

	address = getenv("IXP_ADDRESS");
	mountaddr = NULL;

	if(!address)
		fatal("$IXP_ADDRESS not set\n");

	fd = ixp_announce(address);
	if(fd < 0)
		fatal("ixp_announce failure\n");
	
	if(xml_init(argv[1]) == 0)
		fatal("xml_init failure");

	/* set up a fake client so we can grap connects. */
	acceptor = ixp_listen(&server, fd, &p9srv, ixp_serve9conn, NULL);

	ixp_serverloop(&server);
	printf("msg %s\n", ixp_errbuf());
	return ret;
}

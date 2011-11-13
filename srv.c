#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <ixp.h>
#define IxpFileIdU void*
#include <stdbool.h>
#include <ixp_srvutil.h>
#include "fns.h"

static bool isdigits(char *string) {
	char *p = string;

	while(*p) {
		if (!isdigit(*p))
			return false;
		p++;
	}

	return true;
}

void mkstat(IxpStat *s, char *name) {
	memset(s, 0, sizeof(IxpStat));
	s->mode = 0777;
	s->qid.type = 0;
	s->name = name? name: "error";
	s->uid = s->gid = s->muid = "eli"; // TODO XXX
}

#define debug(...) fprintf(stderr, "xmlfs: " __VA_ARGS__)

static void fs_open(Ixp9Req *r);
static void fs_walk(Ixp9Req *r);
static void fs_read(Ixp9Req *r);
static void fs_stat(Ixp9Req *r);
static void fs_write(Ixp9Req *r);
static void fs_clunk(Ixp9Req *r);
static void fs_flush(Ixp9Req *r);
static void fs_attach(Ixp9Req *r);
static void fs_create(Ixp9Req *r);
static void fs_remove(Ixp9Req *r);
static void fs_freefid(IxpFid *f);

Ixp9Srv p9srv = {
	.open=  fs_open,
	.walk=  fs_walk,
	.read=  fs_read,
	.stat=  fs_stat,
	.write= fs_write,
	.clunk= fs_clunk,
	.flush= fs_flush,
	.attach=fs_attach,
	.create=fs_create,
	.remove=fs_remove,
	.freefid=fs_freefid
};

static void fs_open(Ixp9Req *r) {
	debug("fs_open(%p)\n", r);

	ixp_respond(r, NULL);
}

static void fs_walk(Ixp9Req *r) {
	int i, n;
	char name[PATH_MAX] = "\0";

	debug("fs_walk(%p)\n", r);

	xml_reset((void*)r);

	for (i = 0; i < r->ifcall.twalk.nwname; i++) {
		strncpy(name, r->ifcall.twalk.wname[i], PATH_MAX-1);
		name[strcspn(name, "_")] = '\0';
		debug("fs_walk %s\n", name);
		if (isdigits(name)) {
			n = atoi(name);
			if (xml_nth_child((void*)r, n) == 0) {
				ixp_respond(r, "does not exist");
				return;
			}
			r->ofcall.rwalk.wqid[i].type = P9_QTDIR;
//			r->ofcall.rwalk.wqid[i].path |= n << (i * 8);
		} else if (strcmp(name, "contents") == 0) {
			r->ofcall.rwalk.wqid[i].type = P9_QTFILE;
			r->newfid->aux = (void*)2;
			
			r->ofcall.rwalk.nwqid = i+1;

			ixp_respond(r, NULL);
			
			return;
		} else {
			ixp_respond(r, "does not exist");
			return;
		}
	}

	r->ofcall.rwalk.nwqid = i;

	ixp_respond(r, NULL);
}

static void fs_read(Ixp9Req *r) {
	IxpMsg m;
	IxpStat s;
	unsigned char *buf;
	int size;

	debug("fs_read(%p)\n", r);

	// one-time thingy
	if ((int)r->fid->aux == 0) {
		xml_nth_child((void*)r, 0);
		r->fid->aux = (void*)1;
	}

	if (xml_valid((void*)r)) {
		if ((int)r->fid->aux == 1) {
			mkstat(&s, xml_n((void*)r));
			s.mode |= P9_DMDIR;
			s.qid.type |= P9_QTDIR;
			size = ixp_sizeof_stat(&s);

			buf = ixp_emallocz(size);

			m = ixp_message(buf, size, MsgPack);
			ixp_pstat(&m, &s);

			r->ofcall.rread.data = m.data;
			r->ofcall.rread.count = size;

			xml_next((void*)r);
		} else if ((int)r->fid->aux == 2) {
			buf = xml_content((void*)r);
			r->ofcall.rread.data = buf;
			r->ofcall.rread.count = strlen(buf);
			r->fid->aux = (void*)3;
		} else {
			r->ofcall.rread.data = NULL;
			r->ofcall.rread.count = 0;
		}
	} else if ((int)r->fid->aux < 2) {
		mkstat(&s, "contents");
		s.qid.type = P9_QTFILE;
		size = ixp_sizeof_stat(&s);

		buf = ixp_emallocz(size);

		m = ixp_message(buf, size, MsgPack);
		ixp_pstat(&m, &s);

		r->ofcall.rread.data = m.data;
		r->ofcall.rread.count = size;
		
		r->fid->aux = (void*)2;
	} else {
		r->ofcall.rread.data = NULL;
		r->ofcall.rread.count = 0;
	}

	ixp_respond(r, NULL);
}

// clean this shit up dammit
static void fs_stat(Ixp9Req *r) {
	IxpStat s;
	int size;
	IxpMsg m;
	unsigned char *buf;
	char *n;

	debug("fs_stat(%p)\n", r);

	n = xml_n((void*)r);
	if (n != NULL) {
		mkstat(&s, n);
		s.mode |= P9_DMDIR;
		s.qid.type = P9_QTDIR;
	} else {
		mkstat(&s, "contents");
		s.qid.type = P9_QTFILE;
	}

	r->fid->qid = s.qid;
	r->ofcall.rstat.nstat = size = ixp_sizeof_stat(&s);

	buf = ixp_emallocz(size);

	m = ixp_message(buf, size, MsgPack);
	ixp_pstat(&m, &s);

	r->ofcall.rstat.stat = m.data;
	ixp_respond(r, NULL);
}

static void fs_write(Ixp9Req *r) {
	debug("fs_write(%p)\n", r);

	r->ofcall.rwrite.count = xml_write((void*)r, r->ifcall.twrite.data, r->ifcall.twrite.count);

	ixp_respond(r, NULL);
}

static void fs_clunk(Ixp9Req *r) {
	debug("fs_clunk(%p)\n", r);

	ixp_respond(r, NULL);
}

static void fs_flush(Ixp9Req *r) {
	debug("fs_flush(%p)\n", r);

	ixp_respond(r, NULL);
}

static void fs_attach(Ixp9Req *r) {
	debug("fs_attach(%p)\n", r);

	r->fid->qid.type = P9_QTDIR;
	r->fid->qid.path = (uintptr_t)r->fid;
	r->ofcall.rattach.qid = r->fid->qid;
	ixp_respond(r, NULL);
}

static void fs_create(Ixp9Req *r) {
	debug("fs_create(%p)\n", r);

	xml_create((void*)r, r->ifcall.tcreate.name);

	ixp_respond(r, NULL);
}

static void fs_remove(Ixp9Req *r) {
	debug("fs_remove(%p)\n", r);

	xml_remove((void*)r);

	ixp_respond(r, NULL);
}

static void fs_freefid(IxpFid *f) {
	debug("fs_freefid(%p)\n", f);
}

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

enum {
	Qstart,
	Qcontinue,
	Qcontents,
	Qdone,
};

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
	s->qid.type = P9_QTFILE;
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
	debug("fs_walk %d\n", xml_get_state(r));

	if (xml_get_state((void*)r) == Qdone) {
		xml_set_state((void*)r, Qstart);
		xml_last((void*)r);
//		xml_next((void*)r);
	}

	for (i = 0; i < r->ifcall.twalk.nwname; i++) {
		strncpy(name, r->ifcall.twalk.wname[i], PATH_MAX-1);
		debug("fs_walk %s\n", name);
		name[strcspn(name, "_")] = '\0';
		if (strcmp(name, ".") == 0) {
			continue;
		} else if (strcmp(name, "..") == 0) {
			xml_last((void*)r);
		} else if (isdigits(name)) {
			n = atoi(name);
			if (xml_nth_child((void*)r, n) == 0) {
				debug("fs_walk wtf0 %d %s\n", xml_get_state((void*)r), xml_n((void*)r));
				xml_last((void*)r);
				ixp_respond(r, "file not found");
				return;
			}
			r->ofcall.rwalk.wqid[i].type = P9_QTDIR;
			xml_set_state((void*)r, Qstart);
		} else if (strcmp(name, "contents") == 0) {
			r->ofcall.rwalk.wqid[i].type = P9_QTFILE;

			if (xml_valid((void*)r))
				xml_set_state((void*)r, Qcontents);
			
			i++;
			break;

		} else {
			debug("fs_walk wtf1 %d %s\n", xml_get_state((void*)r), xml_n((void*)r));
			//xml_reset((void*)r);
			ixp_respond(r, "file not found");
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
	debug("fs_read %d %s\n", xml_get_state((void*)r), xml_n((void*)r));

	r->ofcall.rread.data = NULL;
	r->ofcall.rread.count = 0;

	// one-time thingy
	if (xml_get_state((void*)r) == Qstart) {
		if (xml_nth_child((void*)r, 0) != 0)
			xml_set_state((void*)r, Qcontinue);
		else
			xml_set_state((void*)r, Qdone);
	}

	if (xml_get_state((void*)r) == Qdone) {
		xml_last(r);
//		xml_reset((void*)r);
//		xml_next((void*)r);
//		xml_set_state((void*)r, Qcontinue);
		xml_set_state((void*)r, Qstart);
		r->ofcall.rread.data = ixp_estrdup("");
		debug("fs_read done1, ptr: %s\n", xml_n((void*)r));
	} else if (xml_valid((void*)r)) {
		if (xml_get_state((void*)r) == Qcontinue) {
			mkstat(&s, xml_n((void*)r));
			s.mode |= P9_DMDIR;
			s.qid.type = P9_QTDIR;
			size = ixp_sizeof_stat(&s);

			buf = ixp_emallocz(size);

			m = ixp_message(buf, size, MsgPack);
			ixp_pstat(&m, &s);

			r->ofcall.rread.data = m.data;
			r->ofcall.rread.count = size;

			xml_next((void*)r);
		} else if (xml_get_state((void*)r) == Qcontents) {

			buf = xml_content((void*)r);
			r->ofcall.rread.data = buf;
			r->ofcall.rread.count = strlen(buf);
			xml_set_state((void*)r, Qdone);
		} else {
			debug("fs_read wtf %d %s\n", xml_get_state((void*)r), xml_n((void*)r));
		}
	} else if (xml_get_state((void*)r) == Qcontinue) {
		mkstat(&s, "contents");
		size = ixp_sizeof_stat(&s);

		buf = ixp_emallocz(size);

		m = ixp_message(buf, size, MsgPack);
		ixp_pstat(&m, &s);

		r->ofcall.rread.data = m.data;
		r->ofcall.rread.count = size;
		
		xml_set_state((void*)r, Qdone);
		xml_last((void*)r);

	} else {
		debug("fs_read wtf %d\n", xml_get_state((void*)r));
	}

	ixp_respond(r, NULL);
}

static void fs_stat(Ixp9Req *r) {
	IxpStat s;
	int size;
	IxpMsg m;
	unsigned char *buf;
	char *n;

	debug("fs_stat(%p)\n", r);

	n = xml_n((void*)r);

	debug("fs_stat %d %s\n", xml_get_state((void*)r), n);

	if (n != NULL) {
		if (xml_get_state((void*)r) == Qcontents) {
			mkstat(&s, "contents");
		} else {
			mkstat(&s, n);
			s.mode |= P9_DMDIR;
			s.qid.type = P9_QTDIR;
		}
	} else {
		ixp_respond(r, "file not found");

		return;
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
	int state = xml_get_state((void*)r);

	debug("fs_clunk(%p)\n", r);

	xml_reset((void*)r);
	xml_set_state((void*)r, Qdone);

	ixp_respond(r, NULL);
}

static void fs_flush(Ixp9Req *r) {
	debug("fs_flush(%p)\n", r);

	ixp_respond(r, NULL);
}

static void fs_attach(Ixp9Req *r) {
	debug("fs_attach(%p)\n", r);

	xml_reset((void*)r);

	xml_set_state((void*)r, Qstart);
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

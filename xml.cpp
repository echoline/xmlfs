#include <map>
#include <libxml/parser.h>
#include <libxml/tree.h>
using namespace std;

char *filename = NULL;
xmlDoc *doc = NULL;
xmlNode *root = NULL;
map<void*,xmlNode*> pointers;
map<void*,xmlNode*> lastp;
map<void*,int> Qstates;

extern "C" int xml_init(char* f) {
	filename = f;
	doc = xmlReadFile(filename, NULL, 0);

	root = xmlDocGetRootElement(doc);
	root->_private = (void*)(0);

	return 1;
}

extern "C" void xml_reset(void *p) {
	pointers[p] = root;
	lastp[p] = root;
	Qstates[p] = 0;
}

extern "C" int xml_nth_child(void *p,int child) {
	int offset = 0;

	if (pointers[p] == NULL)
		return 0;

	lastp[p] = pointers[p];

	for (pointers[p] = xmlFirstElementChild(pointers[p]);
	     pointers[p] != NULL; 
	     pointers[p] = xmlNextElementSibling(pointers[p])) {
		pointers[p]->_private = (void*)offset;

		if (offset == child) {
			return 1;
		} else if (offset > child) {
			return 0;
		}
		offset++;
	}

	return 0;
}

extern "C" int xml_next(void *p) {
	int offset;

	if (pointers[p] == NULL)
		return 0;

	offset = (int)pointers[p]->_private + 1;

	pointers[p] = xmlNextElementSibling(pointers[p]);

	if (pointers[p] == NULL)
		return 0;

	pointers[p]->_private = (void*)offset;

	return 1;
}

extern "C" int xml_valid(void *p) {
	return (pointers[p] != NULL);
}

extern "C" char* xml_name(void *p) {
	if (pointers[p] == NULL)
		return NULL;
	
	return (char*)pointers[p]->name;
}

extern "C" char* xml_content(void *p) {
	if (pointers[p] == NULL)
		return NULL;
	
	return (char*)xmlNodeGetContent(pointers[p]);
}

extern "C" int xml_index(void *p) {
	if (pointers[p] == NULL)
		return -1;

	return (int)pointers[p]->_private;
}

extern "C" char* xml_n(void *p) {
#define XML_N_BUF_SIZE 128
	static char ret[XML_N_BUF_SIZE];

	if (pointers[p] == NULL)
		return NULL;

	snprintf(ret, XML_N_BUF_SIZE-1, "%d_%s", (int)pointers[p]->_private, xml_name(p));

	return ret;
}

extern "C" void xml_create(void *p, char *name) {
	lastp[p] = pointers[p];
	pointers[p] = xmlAddChild(pointers[p],
			xmlNewNode(pointers[p]->ns, (xmlChar*)name));
	xmlSaveFile(filename, doc);
}

extern "C" int xml_write(void *p, char *data, int len) {
	xmlAddChild(pointers[p], xmlNewTextLen((xmlChar*)data, len));
	xmlSaveFile(filename, doc);
	return len;
}

extern "C" int xml_parent(void *p) {
	if (pointers[p] == root)
		return 0;

	pointers[p] = pointers[p]->parent;
}

extern "C" int xml_remove(void *p) {
	xmlUnlinkNode(pointers[p]);
	xmlFreeNode(pointers[p]);
	xmlSaveFile(filename, doc);
	pointers[p] = NULL;
}

extern "C" void xml_last(void *p) {
	if (lastp[p] != NULL)
		pointers[p] = lastp[p];
	else
		pointers[p] = root;
}

extern "C" int xml_get_state(void *p) {
	return Qstates[p];
}

extern "C" void xml_set_state(void *p, int i) {
	Qstates[p] = i;
}

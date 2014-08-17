#include <assert.h>
#include <time.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>
#include <libxslt/documents.h>
#include "dive.h"

void picture_load_exif_data(struct picture *p, timestamp_t *timestamp)
{
	assert(0);
}


static xmlDocPtr get_stylesheet_doc(const xmlChar *uri, xmlDictPtr xdp, int in, void *v, xsltLoadType xlp)
{
	int len, ret;
	long int size;
	len = xmlStrlen(uri);
	char filename[6 + len];
	sprintf(filename, "xslt/%s", (char *) uri);
	FILE *f = fopen(filename, "r");
	if (f) {
		/* Load and parse the data */
		fseek(f, 0, SEEK_END);
		size = ftell(f);
		fseek(f, 0, SEEK_SET);
		char *buffer = (char *) malloc(size);
		ret = fwrite(buffer, size, 1, f);
		fclose(f);
		if (ret == 1) {
			xmlDocPtr doc = xmlParseMemory(buffer, size);
			return doc;
		} else {
			return NULL;
		}
	} else {
		return NULL;
	}
}

xsltStylesheetPtr get_stylesheet(const char *name)
{
	// this needs to be done only once, but doesn't hurt to run every time
	xsltSetLoaderFunc(get_stylesheet_doc);

	// get main document:
	xmlDocPtr doc = get_stylesheet_doc((const xmlChar *)name, NULL, 0, NULL, XSLT_LOAD_START);
	if (!doc)
		return NULL;

	//	xsltSetGenericErrorFunc(stderr, NULL);
	xsltStylesheetPtr xslt = xsltParseStylesheetDoc(doc);
	if (!xslt) {
		xmlFreeDoc(doc);
		return NULL;
	}

	return xslt;
}

#define DT_STR_SIZE 32
#define DT_FORMAT "%Y-%m-%d %I:%M:%S %p"
const char *get_dive_date_c_string(timestamp_t when)
{
	struct tm x;
	utc_mkdate(when, &x);
	char *dt_str = (char *) malloc(DT_STR_SIZE);
	int i = strftime(dt_str, DT_STR_SIZE, DT_FORMAT, &x);
	dt_str[i] = 0;
	return dt_str;
}


#define SZ_IDSET 1024

typedef struct divekey {
	int id;
} divekey;

static divekey *diveidset[SZ_IDSET];

// Multiplication method of hashing
// The formula being
// hash = (a * k % pow(2, w)) >> (w - r)
// &  m = pow(2, r)
// where,
//	w is the wordlength of the machine
//	a is a random number between biggest and smallest number possible
//	m is the size of hashset
static int ih(int k)
{
	int wl = 32;			// Wordlength
	int mxwrd = (1 << wl) - 1;	// Max int possible with given wordlength
	int a = rand() % mxwrd;		// Random integer a
	int r = 10;			// setsize = pow(2, r);
	return (a * k % mxwrd) >> (wl - r);
}

static int irh(int i, int j)
{
	return i + j % SZ_IDSET;
}

static int id_in_set(int id)
{
	int i = ih(id);
	int j = 0;
	while (diveidset[i] != NULL && diveidset[i]->id != id) {
		i = irh(i, j);
		j++;
		if (j == SZ_IDSET)
			break;
	}
	if (diveidset[i] == NULL) {		// Not present in set
		return 1;
	} else if (diveidset[i]->id == id) {	// Present in set
		return 0;
	} else {				// Table is full and all entries checked
		report_error("IDSET is completely filled.");
		return -1;
	}
}

static int insert_in_set(int id)
{
	int i = ih(id);
	int j = 0;
	while (diveidset[i] != NULL && diveidset[i]->id != id) {
		i = irh(i, j);
		j++;
		if (j == SZ_IDSET) {
			report_error("IDSET is completely filled");
			return -1;
		}
	}
	if (diveidset[i] == NULL) {
		struct divekey *nk = (struct divekey *) malloc (sizeof (struct divekey));
		nk->id = id;
		diveidset[i] = nk;
		return 0;
	}
	diveidset[i]->id = id;
	return 0;
}

// we need this to be uniq, but also make sure
// it doesn't change during the life time of a Subsurface session
// oh, and it has no meaning whatsoever - that's why we have the
// silly initial number and increment by 3 :-)
//
int dive_getUniqID(struct dive *d)
{
	static int maxId = 83529;
	int id = d->id;
	int ret;
	if (id) {
		if (!id_in_set(id)) {
			report_error("WTF - only I am allowed to create IDs");
			insert_in_set(id);
		}
		return id;
	}
	maxId += 3;
	id = maxId;
	ret = insert_in_set(id);
	if (ret == -1)
		return -1;
	else
		return id;
}


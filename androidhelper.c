#include "androidhelper.h"
#include "dive.h"
#include "statistics.h"
#include "file.h"
#include <regex.h>
#include <libxslt/documents.h>
#include <libexif/exif-data.h>

#define translate(_context, arg) trGettext(arg)

#define MAX_WT_STR_LEN 32
#define MAX_GPS_CORD_LEN 32
#define MAX_REG_EXP_LEN 32
#define MAX_REGEX_MATCH_NUM 16
#define MAX_FILENAME_LEN 64

char *weight_string(int weight_in_grams)
{
	char *str = (char *) malloc(MAX_WT_STR_LEN);
	if (get_units()->weight == KG) {
		int gr = weight_in_grams % 1000;
		int kg = weight_in_grams / 1000;
		if (kg >= 20.0) {
			sprintf(str, "0");
		} else {
			sprintf(str, "%d.%ud", kg, (unsigned)(gr) / 100);
		}
	} else {
		double lbs = grams_to_lbs(weight_in_grams);
		sprintf(str, "%f%d%c%d", lbs, 0, 'f', (lbs >= 40.0) ? 0 : 1);
	}
	return (str);
}

char *printGPSCoords(int lat, int lon)
{
	unsigned int latdeg, londeg;
	unsigned int latmin, lonmin;
	double latsec, lonsec;
	char lath, lonh;
	char *result = (char *) malloc(MAX_GPS_CORD_LEN);

	if (!lat && !lon)
		return NULL;

	lath = lat >= 0 ? 'N' : 'S';
	lonh = lon >= 0 ? 'E' : 'W';
	lat = abs(lat);
	lon = abs(lon);
	latdeg = lat / 1000000;
	londeg = lon / 1000000;
	latmin = (lat % 1000000) * 60;
	lonmin = (lon % 1000000) * 60;
	latsec = (latmin % 1000000) * 60;
	lonsec = (lonmin % 1000000) * 60;
	sprintf(result, "%u%s%02d\'%06.3f\"%c %u%s%02d\'%06.3f\"%c",
		       latdeg, UTF8_DEGREE, latmin / 1000000, latsec / 1000000, lath,
		       londeg, UTF8_DEGREE, lonmin / 1000000, lonsec / 1000000, lonh);
	return result;
}

static  char *trim(const char *str, int len)
{
	char *s = (char *) str;
	int i = 0, j = len - 1;
	while(s[i] == ' ' ||
		s[i] == '\t' ||
		s[i] == '\n' ||
		s[i] == '\f' ||
		s[i] == '\v' ||
		s[i] == '\r' )
		i++;
	while(s[j] == ' ' ||
		s[j] == '\t' ||
		s[j] == '\n' ||
		s[j] == '\f' ||
		s[j] == '\v' ||
		s[j] == '\r' )
		j--;
	s[j++] = 0;
	s = s + i;
}

static int count(const char* str, char c)
{
	char *s = (char *) str;
	int i;
	int len = strlen(s);
	int count  = 0;
	for (i = 0; i < len; i++)
		if (str[i] == c)
			count++;
	return count;
}

static double get_double_from_regex(const char *str, regmatch_t *pre, regmatch_t *post)
{
	int len;
	double ret, r;
	char a[16], b[16], c[16];
	if (pre) {
		len = pre->rm_eo - pre->rm_so;
		strncpy(a, str + pre->rm_so, len);
		a[len] = 0;
	} else {
		*a = '0';
		*(a + 1) = 0;
	}
	if (post) {
		len = post->rm_eo - post->rm_so;
		strncpy(b, str + post->rm_so, len);
		b[len] = 0;
	} else {
		*b = '0';
		*(b + 1) = 0;
	}
	if (sprintf(c, "%s.%s", a, b) <= 0)
		goto ERROR;
	r = sscanf(c, "%lf", &ret);
	if (r == 0 || r == EOF) {
		goto ERROR;
	}
	return ret;
ERROR:
	report_error("Error extracting double from regex");
	ret = 555;
	return ret;
}

static bool parseGpsText(const char *gps_text, double *latitude, double *longitude)
{
	enum {
		ISO6709D,
		SECONDS,
		MINUTES,
		DECIMAL
	} gpsStyle = ISO6709D;
	int eastWest = 4;
	int northSouth = 1;
	char trHemisphere[4];
	trHemisphere[0] = 'N';
	trHemisphere[1] = 'S';
	trHemisphere[2] = 'E';
	trHemisphere[3] = 'W';
	char regExp[MAX_REG_EXP_LEN];
	/* an empty string is interpreted as 0.0,0.0 and therefore "no gps location" */
	if (strlen(trim(gps_text, strlen(gps_text))) == 0) {
		*latitude = 0.0;
		*longitude = 0.0;
		return true;
	}
	// trying to parse all formats in one regexp might be possible, but it seems insane
	// so handle the four formats we understand separately

	// ISO 6709 Annex D representation
	// http://en.wikipedia.org/wiki/ISO_6709#Representation_at_the_human_interface_.28Annex_D.29
	// e.g. 52°49'02.388"N 1°36'17.388"E
	if ( isdigit(gps_text[0]) && count(gps_text, ',') % 2 == 0) {
		gpsStyle = ISO6709D;
		sprintf(regExp, "(\\d+)[" UTF8_DEGREE "\\s](\\d+)[\'\\s](\\d+)([,\\.](\\d+))?[\"\\s]([NS%c%c])"
				 "\\s*(\\d+)[" UTF8_DEGREE "\\s](\\d+)[\'\\s](\\d+)([,\\.](\\d+))?[\"\\s]([EW%c%c])"
				 ,trHemisphere[0]
				 ,trHemisphere[1]
				 ,trHemisphere[2]
				 ,trHemisphere[3]);
	} else if (count(gps_text, '\"') == 2) {
		gpsStyle = SECONDS;
		sprintf(regExp, "\\s*([NS%c%c])\\s*(\\d+)[" UTF8_DEGREE "\\s]+(\\d+)[\'\\s]+(\\d+)([,\\.](\\d+))?[^EW%c%c]*"
				 "([EW%c%c])\\s*(\\d+)[" UTF8_DEGREE "\\s]+(\\d+)[\'\\s]+(\\d+)([,\\.](\\d+))?"
				 ,trHemisphere[0]
				 ,trHemisphere[1]
				 ,trHemisphere[2]
				 ,trHemisphere[3]
				 ,trHemisphere[2]
				 ,trHemisphere[3]);
	} else if (count(gps_text, '\'') == 2) {
		gpsStyle = MINUTES;
		sprintf(regExp, "\\s*([NS%c%c])\\s*(\\d+)[" UTF8_DEGREE "\\s]+(\\d+)([,\\.](\\d+))?[^EW%c%c]*"
				 "([EW%c%c])\\s*(\\d+)[" UTF8_DEGREE "\\s]+(\\d+)([,\\.](\\d+))?"
				 ,trHemisphere[0]
				 ,trHemisphere[1]
				 ,trHemisphere[2]
				 ,trHemisphere[3]
				 ,trHemisphere[2]
				 ,trHemisphere[3]);
	} else {
		gpsStyle = DECIMAL;
		sprintf(regExp, "\\s*([-NS%c%c]?)\\s*(\\d+)[,\\.](\\d+)[^-EW%c%c\\d]*([-EW%c%c]?)\\s*(\\d+)[,\\.](\\d+)"
				 ,trHemisphere[0]
				 ,trHemisphere[1]
				 ,trHemisphere[2]
				 ,trHemisphere[3]
				 ,trHemisphere[2]
				 ,trHemisphere[3]);
	}

	int ret;
	regex_t regex;
	regmatch_t matchptr[MAX_REGEX_MATCH_NUM];
	ret = regcomp(&regex, regExp, 0);
	ret = regexec(&regex, gps_text, MAX_REGEX_MATCH_NUM, matchptr, 0);
	char tmpstr[16];
	double a, b, c, tmpval;
	regmatch_t tmpmatch;
	if (ret == 0) {
		switch (gpsStyle) {
		case ISO6709D:
			*latitude = get_double_from_regex(gps_text, matchptr + 1, NULL) +
					get_double_from_regex(gps_text, matchptr + 2, NULL) / 60 +
					get_double_from_regex(gps_text, matchptr + 3, matchptr + 5) / 3600;
			*longitude = get_double_from_regex(gps_text, matchptr + 7, NULL) +
					get_double_from_regex(gps_text, matchptr + 8, NULL) / 60 +
					get_double_from_regex(gps_text, matchptr + 9, matchptr + 11) / 3600;
			northSouth = 6;
			eastWest = 12;
			break;
		case SECONDS:
			*latitude = get_double_from_regex(gps_text, matchptr + 2, NULL) +
					get_double_from_regex(gps_text, matchptr + 3, NULL) / 60 +
					get_double_from_regex(gps_text, matchptr + 4, matchptr + 6) / 3600;
			*longitude = get_double_from_regex(gps_text, matchptr + 8, NULL) +
					get_double_from_regex(gps_text, matchptr + 9, NULL) / 60 +
					get_double_from_regex(gps_text, matchptr + 10, matchptr + 12) / 3600;
			eastWest = 7;
			break;
		case MINUTES:
			*latitude = get_double_from_regex(gps_text, matchptr + 2, NULL) +
					get_double_from_regex(gps_text, matchptr + 3, matchptr + 5) / 60;
			*longitude = get_double_from_regex(gps_text, matchptr + 7, NULL) +
					get_double_from_regex(gps_text, matchptr + 8, matchptr + 10) / 60;
			eastWest = 6;
			break;
		case DECIMAL:
		default:
			*latitude = get_double_from_regex(gps_text, matchptr + 2, matchptr + 3);
			*longitude = get_double_from_regex(gps_text, matchptr + 5, matchptr + 6);
			break;
		}
		if (gps_text[matchptr[northSouth].rm_so] == 'S' ||
				gps_text[matchptr[northSouth].rm_so] == trHemisphere[1] ||
				gps_text[matchptr[northSouth].rm_so] == '-')
			*latitude *= -1.0;
		if (gps_text[matchptr[eastWest].rm_so] == 'W' ||
				gps_text[matchptr[eastWest].rm_so] == trHemisphere[3] ||
				gps_text[matchptr[eastWest].rm_so] == '-')
			*longitude *= -1.0;
		return true;
	}
	return false;
}

bool gpsHasChanged(struct dive *dive, struct dive *master, const char *gps_text, bool *parsed_out)
{
	double latitude, longitude;
	int latudeg, longudeg;
	bool ignore;
	bool *parsed = parsed_out ? : &ignore;

	/* if we have a master and the dive's gps address is different from it,
	 * don't change the dive */
	if (master && (master->latitude.udeg != dive->latitude.udeg ||
		       master->longitude.udeg != dive->longitude.udeg))
		return false;

	if (!(*parsed = parseGpsText(gps_text, &latitude, &longitude)))
		return false;

	latudeg = rint(1000000 * latitude);
	longudeg = rint(1000000 * longitude);

	/* if dive gps didn't change, nothing changed */
	if (dive->latitude.udeg == latudeg && dive->longitude.udeg == longudeg)
		return false;
	/* ok, update the dive and mark things changed */
	dive->latitude.udeg = latudeg;
	dive->longitude.udeg = longudeg;
	return true;
}

int *getDivesInTrip(dive_trip_t *trip)
{
	int *ret = (int *) malloc(trip->nrdives);
	int i, j = 0;
	struct dive *d;
	for_each_dive (i, d) {
		if (d->divetrip == trip) {
			ret[j++] = get_divenr(d);
		}
	}
	return ret;
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

static timestamp_t get_epoch_from_exif(ExifEntry *tstamp, ExifEntry *dstamp)
{
	struct tm tm;
	int year = 0, month = 0, day = 0;
	int hour = 0, min = 0, sec = 0, csec = 0;
	if (tstamp)
		sscanf(tstamp->data, "%d:%d:%d.%d", &hour, &min, &sec, &csec);
	if (dstamp)
		sscanf(dstamp->data, "%d:%d:%d", &year, &month, &day);
	tm.tm_year = year;
	tm.tm_mon = month - 1;
	tm.tm_mday = day;
	tm.tm_hour = hour;
	tm.tm_min = min;
	tm.tm_sec = sec;
	return (utc_mktime(&tm));
}

static int get_int_str(const char *str, int len)
{
	char *s = (char *) str;
	int ret = 0;
	int i, x;
	for (i = 0; i < len; i++) {
		x = s[i];
		x &= 0xFF;
		x = x << (8 * (len -1 - i));
		ret += x;
	}
	return ret;
}

static int  get_latlon_from_exif(ExifEntry *latlon, ExifEntry *latlonref)
{
	char buf[64];
	if (latlon) {
		float ndeg = (float) get_int_str(latlon->data, 4);
		float ddeg = (float) get_int_str(latlon->data + 4, 4);
		float nmin = (float) get_int_str(latlon->data + 8, 4);
		float dmin = (float) get_int_str(latlon->data + 12, 4);
		float nsec = (float) get_int_str(latlon->data + 16, 4);
		float dsec = (float) get_int_str(latlon->data + 20, 4);
		double flat = (ndeg / ddeg) + (nmin / dmin) / 60 + (nsec / dsec) / 3600;
		char dir = latlonref->data[0];
		switch (dir) {
		case 'S':
		case 'W':
			flat *= -1;
			break;
		}
		return lrint(1000000.0 * flat);
	} else {
		return 0;
	}
}

void picture_load_exif_data(struct picture *p, timestamp_t *timestamp)
{
	ExifData *ed;
	struct memblock mem;
	ExifEntry *lat, *lon, *latref, *lonref, *dstamp, *tstamp;

	if (readfile(p->filename, &mem) <= 0)
		goto picture_load_exit;
	ed = exif_data_new_from_data((unsigned char *) mem.buffer, (unsigned int) mem.size);

	if (!ed)
		goto picture_load_exit;
	tstamp = exif_data_get_entry(ed, EXIF_TAG_GPS_TIME_STAMP);
	dstamp = exif_data_get_entry(ed, EXIF_TAG_GPS_DATE_STAMP);
	*timestamp = get_epoch_from_exif(tstamp, dstamp);

	lat = exif_data_get_entry(ed, EXIF_TAG_GPS_LATITUDE);
	latref = exif_data_get_entry(ed, EXIF_TAG_GPS_LATITUDE_REF);
	p->latitude.udeg = get_latlon_from_exif(lat, latref);

	lon = exif_data_get_entry(ed, EXIF_TAG_GPS_LONGITUDE);
	lonref = exif_data_get_entry(ed, EXIF_TAG_GPS_LONGITUDE_REF);
	p->longitude.udeg = get_latlon_from_exif(lon, lonref);

picture_load_exit:
	exif_data_free(ed);
	free(mem.buffer);
	return;
}

const char *get_file_name(const char *fileName)
{
	int i;
	int len = strlen(fileName);
	for (i = len - 1; i >= 0 && fileName[i] != '/'; i--);
	const char *name = fileName + i + 1;
	return name;
}

void copy_image_and_overwrite(const char *cfileName, const char *cnewName)
{
	char fileName[MAX_FILENAME_LEN];
	char newName[MAX_FILENAME_LEN];
	strcpy(fileName, cfileName);
	strcpy(newName, cnewName);

	const char *f = get_file_name(cfileName);
	int l = strlen(newName) + strlen(f) + 1;
	if (l > MAX_FILENAME_LEN)
		realloc(newName, l);
	strcat(newName, f);
	FILE *src, *dest;
	char c;
	src = fopen(fileName, "r");
	dest = fopen(newName, "w");
	if (src && dest) {
		while ((c = fgetc(src)) != EOF)
			fputc((int) c, dest);
	}
	fclose(src);
	fclose(dest);
}

#define SZ_HASHTABLE 1000

typedef struct gasnamevol {
	char name[64];
	int vol;
} gasnamevol;

static gasnamevol gashtable[SZ_HASHTABLE];

/*
 * Taken from Dan Bernstein http://www.cse.yorku.ca/~oz/hash.html
 */
static unsigned long int sh(const char *str)
{
	unsigned char *s = (unsigned char *) str;
	unsigned long hash = 5381;
	int c;
	while (c = *s++)
		hash = ((hash << 5) + hash) ^ c; /* (hash * 33) ^ c */
	return hash % SZ_HASHTABLE;
}

static unsigned long int srh(int i, int j)
{
	return (i + j) % SZ_HASHTABLE;
}

static int get_gas_vol(const char *gasname)
{
	unsigned long int i = sh(gasname);
	int j = 0;
	while (gashtable[i].name == NULL || strcmp(gashtable[i].name, gasname) == 0) {
		i = srh(i, j);
		j++;
	}
	if (gashtable[i].name == NULL) {
		strncpy(gashtable[i].name, gasname, 64);
		gashtable[i].vol = 0;
		return 0;
	} else {
		return gashtable[i].vol;
	}
}

static void set_gas_vol(const char *gasname, int gasvol)
{
	int i = sh(gasname);
	int j = 0;
	while (gashtable[i].name == NULL || strcmp(gashtable[i].name, gasname) == 0) {
		i = srh(i, j);
		j++;
	}
	if (gashtable[i].name == NULL) {
		strncpy(gashtable[i].name, gasname, 64);
	}
	gashtable[i].vol = gasvol;
}

static int get_gas_nos()
{
	int i, count = 0;
	for (i = 0; i < SZ_HASHTABLE; i++)
	{
		if (gashtable[i].name != NULL)
			count++;
	}
	return count;
}

static void gaslist_from_hashmap(gasnamevol *gaslist[])
{
	int i, j = 0;
	int count = get_gas_nos();
	realloc (gaslist, count * sizeof (gasnamevol));
	for (i = 0; i < SZ_HASHTABLE; i++) {
		if (gashtable[i].name != NULL)
			gaslist[j++] = gashtable + i;
	}
}

static int gasvolcmp(const void *a, const void *b)
{
	gasnamevol *x, *y;
	x = (gasnamevol *) a;
	y = (gasnamevol *) b;
	return (x->vol - y->vol);
}

void selectedDivesGasUsed(gasnamevol *gasUsedOrdered[])
{
	int i, j;
	struct dive *d;
	char gasName[64];
	int num = 0;
	for_each_dive (i, d) {
		if (!d->selected)
			continue;
		volume_t diveGases[MAX_CYLINDERS] = {};
		get_gas_used(d, diveGases);
		for (j = 0; j < MAX_CYLINDERS; j++)
			if (diveGases[j].mliter) {
				strncpy(gasName, gasname(&d->cylinder[j].gasmix), 64);
				int gasvol = get_gas_vol(gasName);
				gasvol += diveGases[j].mliter;
				set_gas_vol(gasName, gasvol);
			}
	}
	gaslist_from_hashmap(gasUsedOrdered);
	num = get_gas_nos();
	qsort(gasUsedOrdered, num, sizeof (gasnamevol), gasvolcmp);
}


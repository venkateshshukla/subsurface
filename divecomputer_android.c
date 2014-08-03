/* Functions for managing various divecomputers */

#include "divecomputer_android.h"
#include <stdint.h>

divecomputer_list dclist = {0, NULL};

divecomputer_node *new_dcnode(const char *mdl, uint32_t id, const char *sno, const char *fmw, const char *nick)
{
	divecomputer_node *newnode = (divecomputer_node *) malloc (sizeof (divecomputer_node));
	if (!newnode)
		return NULL;
	newnode->deviceid = id;
	strncpy(newnode->model, mdl, 64);
	strncpy(newnode->serialnumber, sno, 64);
	strncpy(newnode->firmware, fmw, 64);
	strncpy(newnode->nickname, nick, 64);
	return newnode;
}

bool is_dc_equal(const divecomputer_node *x, const divecomputer_node *y)
{
	bool a = (x->deviceid == y->deviceid);
	bool b = !strncmp(x->serialnumber, y->serialnumber, 64);
	bool c = !strncmp(x->model, y->model, 64);
	bool d = !strncmp(x->firmware, y->firmware, 64);
	bool e = !strncmp(x->nickname, y->nickname, 64);

	return a && b && c && d && e;
}

bool dc_changed_values(const divecomputer_node *old, const divecomputer_node *new)
{
	bool a = (old->deviceid == new->deviceid);
	bool b = !strncmp(old->model, new->model, 64);
	if (!a || !b)
		return false;
	bool c = strncmp(old->serialnumber, new->serialnumber, 64);
	bool d = strncmp(old->firmware, new->firmware, 64);
	bool e = strncmp(old->nickname, new->nickname, 64);
	return c || d || e;
}

static int num_dcs_model(const char *m)
{
	int count = 0;
	divecomputer_node *head = dclist.head;
	while(head != NULL) {
		if(!m || !strncmp(m, head->model, 64)) // if m is NULL, it matches all
			count++;
		head = head->next;
	}
	return count;
}

static divecomputer_node **get_all_dcs_model(const char *m, int *num)
{
	if (m == NULL)
		return NULL;
	*num = num_dcs_model(m);
	if (*num == 0)
		return NULL;
	divecomputer_node **list = (divecomputer_node **) malloc(*num * sizeof (divecomputer_node *));
	if (!list)
		return NULL;
	divecomputer_node *head = dclist.head;
	int i = 0;
	while (head != NULL) {
		if (!strncmp(m, head->model, 64)) {
			list[i] = head;
			i++;
		}
		head = head->next;
	}
	return list;
}

static divecomputer_node **get_all_dcs(int *num)
{
	*num = num_dcs_model(NULL);
	if (*num == 0)
		return NULL;
	divecomputer_node **list = (divecomputer_node **) malloc(*num * sizeof (divecomputer_node *));
	if (!list)
		return NULL;
	divecomputer_node *head = dclist.head;
	int i = 0;
	while (head != NULL) {
		list[i] = head;
		i++;
		head = head->next;
	}
	return list;
}

const divecomputer_node *get_exact_dc(const char *m, uint32_t id)
{
	int num = 0;
	int i;
	divecomputer_node **dcl = get_all_dcs_model(m, &num);
	if (num == 0)
		return NULL;
	for (i = 0; i < num; i++) {
		divecomputer_node *tmp = *(dcl + i);
		if (tmp->deviceid == id) {
			free(dcl);
			return tmp;
		}
	}
	free(dcl);
	return NULL;
}

const divecomputer_node *get_dc(const char *m, int *num)
{
	*num = 0;
	divecomputer_node **dcl = get_all_dcs_model(m, num);
	if ( *num && dcl && *dcl) {
		free(dcl);
		return *dcl;
	}
	free(dcl);
	return NULL;
}

static void insert_dcnode(const char *m, const divecomputer_node *n)
{
	if (!m || !n)
		return;
	divecomputer_node *node = (divecomputer_node *) n;
	divecomputer_node *now = dclist.head;
	divecomputer_node *prev = NULL;
	if (now == NULL) {
		// Empty linked list. Insert the node at head.
		dclist.head = node;
		dclist.count++;
		return;
	}
	do {
		if (!strncmp(now->model, m, 64)) {
			// Found a node with matching model. Add the node before
			// this node so that the latest addition always comes at
			// first position.
			node->next = now;
			if (prev == NULL) {
				// First member has the same model. Insert
				// before
				dclist.head = node;
				node->next = now;
				dclist.count ++;
				return;
			}
			prev->next = node;
			dclist.count++;
			return;
		}
		prev = now;
		now = now->next;
	} while (now != NULL);

	// Reached the end of the list. Attach the node here.
	prev->next = node;
	dclist.count++;
}

static void remove_dcnode(const char *m, const divecomputer_node *node)
{
	if (!m || !node)
		return;
	divecomputer_node *now = dclist.head;
	divecomputer_node *tmp = NULL;
	if (now == NULL)
		return;

	// If matching nodes are present at the beginning, remove them all.
	while(is_dc_equal(now, node)) {
		dclist.head = now->next;
		dclist.count--;
		free(now);
		now = dclist.head;
	}

	// Now, check the rest of the linked list
	while (now->next != NULL) {
		if (is_dc_equal(now->next, node)) {
			tmp = now->next;
			now->next = tmp->next;
			free (tmp);
			dclist.count --;
		}
		now = now->next;
	}
}

void add_dc_mdn(const char *m, uint32_t d, const char *n)
{
	add_dc(m, d, n, NULL, NULL);
}

void add_dc_md(const char *m, uint32_t d)
{
	add_dc(m, d, NULL, NULL, NULL);
}

void add_dc(const char *m, uint32_t d, const char *n, const char *s, const char *f)
{
	if (!m || !d)
		return;
	const divecomputer_node *existnode = get_exact_dc(m, d);
	const divecomputer_node *newnode = new_dcnode(m, d, s, f, n);
	if (existnode) {
		if (dc_changed_values(existnode, newnode)) {
			if (n && strncmp(existnode->nickname, n, 64))
				fprintf(stderr, "new nickname %s for DC model %s deviceid 0x%x", n, m, d);
			if (f && strncmp(existnode->firmware, f, 64))
				fprintf(stderr, "new firmware version %s for DC model %s deviceid 0x%x", f, m, d);
			if (s && strncmp(existnode->serialnumber, s, 64))
				fprintf(stderr, "new serial number %s for DC model %s deviceid 0x%x", s, m, d);
		} else {
			return;
		}
		remove_dcnode(m, existnode);
	}
	insert_dcnode(m, newnode);
}

void rm_dc(const char *m, uint32_t d)
{
	const divecomputer_node *existnode = get_exact_dc(m, d);
	remove_dcnode(m, existnode);
}

void create_device_node(const char *model, uint32_t deviceid, const char *serial, const char *firmware, const char *nickname)
{
	add_dc(model, deviceid, nickname, serial, firmware);
}

static int dc_node_cmp(const void *a, const void *b)
{
	divecomputer_node *x = (divecomputer_node *) a;
	divecomputer_node *y = (divecomputer_node *) b;
	return (x->deviceid - y->deviceid);
}

void call_for_each_dc (void *f, void (*callback)(void *, const char *, uint32_t, const char *, const char *, const char *))
{
	int num = 0;
	divecomputer_node **values = get_all_dcs(&num);
	qsort(values, num, sizeof (divecomputer_node *), dc_node_cmp);
	for (int i = 0; i < num; i++) {
		const divecomputer_node *node = values[i];
		callback(f, node->model, node->deviceid, node->nickname, node->serialnumber, node->firmware);
	}
	free(values);
}


int is_default_dive_computer(const char *vendor, const char *product)
{
	// To be implemented in android
	return 0;
}

int is_default_dive_computer_device(const char *name)
{
	// To be implented in android settings.
	return 0;
}

void set_default_dive_computer(const char *vendor, const char *product)
{
	// This would be implemented on android settings
}

void set_default_dive_computer_device(const char *name)
{
	// Implementation would be in android settings
}

void set_dc_nickname(struct dive *dive)
{
	if (!dive)
		return;

	struct divecomputer *dc;

	for_each_dc (dive, dc) {
		if (dc->model && *dc->model && dc->deviceid &&
		    !get_exact_dc(dc->model, dc->deviceid)) {
			// we don't have this one, yet
			const divecomputer_node *existnode = get_exact_dc(dc->model, dc->deviceid);
			if (existnode) {
				// we already have this model but a different deviceid
				char simplenick[64] = {0};
				if (dc->deviceid == 0)
					sprintf(simplenick, "%s (unknown deviceid)", dc->model);
				else
					sprintf(simplenick, "%s (%X)", dc->model, dc->deviceid);
				add_dc_mdn(dc->model, dc->deviceid, simplenick);
			} else {
				add_dc_md(dc->model, dc->deviceid);
			}
		}
	}
}

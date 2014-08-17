#ifndef DIVECOMPUTER_H
#define DIVECOMPUTER_H

#include <stdint.h>
#include <stdbool.h>
#include "dive.h"

typedef struct divecomputer_node {
	uint32_t deviceid;
	char model[64];
	char serialnumber[64];
	char firmware[64];
	char nickname[64];
	struct divecomputer_node *next;
} divecomputer_node;

/* I dont think the number of divecomputers in this list be humungous. So, for
 * timebeing, I am implementing a linked list for this. This can of course be
 * changed later on.
 */
typedef struct divecomputer_list {
	uint32_t count;
	struct divecomputer_node *head;
} divecomputer_list;

divecomputer_node *new_dcnode(const char *, uint32_t, const char *, const char *, const char *);
bool is_dc_equal(const divecomputer_node *, const divecomputer_node *);
bool dc_changed_values(const divecomputer_node *, const divecomputer_node *);
const divecomputer_node *get_exact_dc(const char *, uint32_t);
const divecomputer_node *get_dc(const char *, int *);
void add_dc(const char *, uint32_t, const char *, const char *, const char *);
void rm_dc(const char *, uint32_t);

void create_device_node(const char *, uint32_t , const char *, const char *, const char *);
void call_for_each_dc (void *, void (*callback)(void *, const char *, uint32_t, const char *, const char *, const char *));
int is_default_dive_computer(const char *, const char *);
int is_default_dive_computer_device(const char *);
void set_default_dive_computer(const char *, const char *);
void set_default_dive_computer_device(const char *);
void set_dc_nickname(struct dive *);

extern divecomputer_list dclist;

#endif

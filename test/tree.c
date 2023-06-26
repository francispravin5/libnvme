// SPDX-License-Identifier: LGPL-2.1-or-later
/**
 * This file is part of libnvme.
 * Copyright (c) 2023 Daniel Wagner, SUSE LLC
 */

#include <assert.h>
#include <string.h>

#include <ccan/array_size/array_size.h>

#include <libnvme.h>

struct test_data {
	/* input data */
	const char *subsysname;
	const char *subsysnqn;
	const char *transport;
	const char *traddr;
	const char *host_traddr;
	const char *host_iface;
	const char *trsvcid;

	/* track controller generated by input data */
	nvme_subsystem_t s;
	nvme_ctrl_t c;
	int ctrl_id;
};

#define DEFAULT_SUBSYSNAME "subsysname"
#define DEFAULT_SUBSYSNQN "subsysnqn"

struct test_data test_data[] = {
	{ DEFAULT_SUBSYSNAME, DEFAULT_SUBSYSNQN, "tcp", "192.168.1.1", "192.168.1.20", NULL, "4420" },
	{ DEFAULT_SUBSYSNAME, DEFAULT_SUBSYSNQN, "tcp", "192.168.1.1", "192.168.1.20", NULL, "4421" },
	{ DEFAULT_SUBSYSNAME, DEFAULT_SUBSYSNQN, "tcp", "192.168.1.2", "192.168.1.20", "eth1", "4420" },
	{ DEFAULT_SUBSYSNAME, DEFAULT_SUBSYSNQN, "tcp", "192.168.1.2", "192.168.1.20", "eth1", "4421" },
	{ DEFAULT_SUBSYSNAME, DEFAULT_SUBSYSNQN, "rdma", "192.168.1.3", "192.168.1.20", NULL, NULL },
	{ DEFAULT_SUBSYSNAME, DEFAULT_SUBSYSNQN, "rdma", "192.168.1.4", "192.168.1.20", NULL, NULL },
	{ DEFAULT_SUBSYSNAME, DEFAULT_SUBSYSNQN, "fc",
	  "nn-0x201700a09890f5bf:pn-0x201900a09890f5bf",
	  "nn-0x200000109b579ef3:pn-0x100000109b579ef3"
	},
	{ DEFAULT_SUBSYSNAME, DEFAULT_SUBSYSNQN, "fc",
	  "nn-0x201700a09890f5bf:pn-0x201900a09890f5bf",
	  "nn-0x200000109b579ef6:pn-0x100000109b579ef6",
	},
};

static struct test_data *find_test_data(nvme_ctrl_t c)
{
	for (int i = 0; i < ARRAY_SIZE(test_data); i++) {
		struct test_data *d = &test_data[i];

		if (d->c == c)
			return d;
	}

	return NULL;
}

static void show_ctrl(nvme_ctrl_t c)
{
	struct test_data *d = find_test_data(c);

	if (d)
		printf("ctrl%d ", d->ctrl_id);

	printf("0x%p: %s %s %s %s %s\n",
	       c,
	       nvme_ctrl_get_transport(c),
	       nvme_ctrl_get_traddr(c),
	       nvme_ctrl_get_host_traddr(c),
	       nvme_ctrl_get_host_iface(c),
	       nvme_ctrl_get_trsvcid(c));
}

static nvme_root_t create_tree()
{
	nvme_root_t r;
	nvme_host_t h;

	r = nvme_create_root(stdout, LOG_DEBUG);
	assert(r);
	h = nvme_default_host(r);
	assert(h);

	printf("  ctrls created:\n");
	for (int i = 0; i < ARRAY_SIZE(test_data); i++) {
		struct test_data *d = &test_data[i];

		d->s = nvme_lookup_subsystem(h, d->subsysname, d->subsysnqn);
		assert(d->s);
		d->c = nvme_lookup_ctrl(d->s, d->transport, d->traddr,
					d->host_traddr, d->host_iface,
					d->trsvcid, NULL);
		assert(d->c);
		d->ctrl_id = i;

		assert(!strcmp(d->transport, nvme_ctrl_get_transport(d->c)));
		assert(!strcmp(d->traddr, nvme_ctrl_get_traddr(d->c)));
		assert(!d->host_traddr || !strcmp(d->host_traddr, nvme_ctrl_get_host_traddr(d->c)));
		assert(!d->host_iface || !strcmp(d->host_iface, nvme_ctrl_get_host_iface(d->c)));
		assert(!d->trsvcid || !strcmp(d->trsvcid, nvme_ctrl_get_trsvcid(d->c)));

		printf("    ");
		show_ctrl(d->c);
	}
	printf("\n");

	return r;
}

static unsigned int count_entries(nvme_root_t r)
{
	nvme_host_t h;
	nvme_subsystem_t s;
	nvme_ctrl_t c;
	unsigned int i = 0;

	nvme_for_each_host(r, h) {
		nvme_for_each_subsystem(h, s) {
			nvme_subsystem_for_each_ctrl(s, c) {
				i++;
			}
		}
	}

	return i;
}

static void ctrl_lookups(nvme_root_t r)
{
	nvme_host_t h;
	nvme_subsystem_t s;
	nvme_ctrl_t c;

	h = nvme_first_host(r);
	s = nvme_lookup_subsystem(h, DEFAULT_SUBSYSNAME, DEFAULT_SUBSYSNQN);

	printf("  lookup controller:\n");
	for (int i = 0; i < ARRAY_SIZE(test_data); i++) {
		struct test_data *d = &test_data[i];

		printf("%10s %10s    ", "", "");
		show_ctrl(d->c);
		c = nvme_lookup_ctrl(s, d->transport, d->traddr, d->host_traddr,
				     NULL, NULL, NULL);
		printf("%10s %10s -> ", "-", "-");
		show_ctrl(c);

		assert(!strcmp(d->transport, nvme_ctrl_get_transport(c)));
		assert(!strcmp(d->traddr, nvme_ctrl_get_traddr(c)));
		assert(!strcmp(d->host_traddr, nvme_ctrl_get_host_traddr(c)));

		if (d->host_iface) {
			c = nvme_lookup_ctrl(s, d->transport, d->traddr, d->host_traddr,
					     d->host_iface, NULL, NULL);
			printf("%10s %10s -> ", d->host_iface, "-");
			show_ctrl(c);

			assert(!strcmp(d->transport, nvme_ctrl_get_transport(c)));
			assert(!strcmp(d->traddr, nvme_ctrl_get_traddr(c)));
			assert(!strcmp(d->host_traddr, nvme_ctrl_get_host_traddr(c)));
			assert(!strcmp(d->host_iface, nvme_ctrl_get_host_iface(c)));
		}

		if (d->trsvcid) {
			c = nvme_lookup_ctrl(s, d->transport, d->traddr, d->host_traddr,
					     NULL, d->trsvcid, NULL);
			printf("%10s %10s -> ", "-", d->trsvcid);
			show_ctrl(c);

			assert(!strcmp(d->transport, nvme_ctrl_get_transport(c)));
			assert(!strcmp(d->traddr, nvme_ctrl_get_traddr(c)));
			assert(!strcmp(d->host_traddr, nvme_ctrl_get_host_traddr(c)));
			assert(!strcmp(d->trsvcid, nvme_ctrl_get_trsvcid(c)));
		}

		if (d->host_iface && d->trsvcid) {
			c = nvme_lookup_ctrl(s, d->transport, d->traddr, d->host_traddr,
					     d->host_iface, d->trsvcid, NULL);
			printf("%10s %10s -> ", d->host_iface, d->trsvcid);
			show_ctrl(c);

			assert(!strcmp(d->transport, nvme_ctrl_get_transport(c)));
			assert(!strcmp(d->traddr, nvme_ctrl_get_traddr(c)));
			assert(!strcmp(d->host_traddr, nvme_ctrl_get_host_traddr(c)));
			assert(!strcmp(d->trsvcid, nvme_ctrl_get_trsvcid(c)));
			assert(!strcmp(d->host_iface, nvme_ctrl_get_host_iface(c)));
		}

		printf("\n");
	}
}

static void test_lookup_1(void)
{
	nvme_root_t r;

	printf("test_lookup_1:\n");

	r = create_tree();
	assert(count_entries(r) == ARRAY_SIZE(test_data));
	ctrl_lookups(r);

	nvme_free_tree(r);
}

static void test_lookup_2(void)
{
	nvme_root_t r;
	nvme_subsystem_t s;
	nvme_host_t h;
	nvme_ctrl_t c1, c2, c3, c4;

	printf("test_lookup_2:\n");

	r = nvme_create_root(stdout, LOG_DEBUG);
	assert(r);
	h = nvme_default_host(r);
	assert(h);

	s = nvme_lookup_subsystem(h, DEFAULT_SUBSYSNAME, DEFAULT_SUBSYSNQN);
	assert(s);

	assert(nvme_lookup_ctrl(s, "tcp", "192.168.2.1", "192.168.2.20",
				"eth0", "4420", NULL));

	c1 = nvme_lookup_ctrl(s, "tcp", "192.168.1.1", "192.168.1.20",
			      NULL, NULL, NULL);
	assert(c1);
	printf("%10s %10s    ", "", "");
	show_ctrl(c1);

	c2 = nvme_lookup_ctrl(s, "tcp", "192.168.1.1", "192.168.1.20",
			      "eth0", NULL, NULL);
	assert(c1 == c2);
	printf("%10s %10s    ", "eth0", "-");
	show_ctrl(c2);

	c3 = nvme_lookup_ctrl(s, "tcp", "192.168.1.1", "192.168.1.20",
			      NULL, "4420", NULL);
	assert(c1 == c3);
	printf("%10s %10s    ", "-", "4420");
	show_ctrl(c3);

	c4 = nvme_lookup_ctrl(s, "tcp", "192.168.1.1", "192.168.1.20",
			      "eth0", "4420", NULL);
	assert(c1 == c4);
	printf("%10s %10s    ", "eth0", "4420");
	show_ctrl(c4);

	nvme_free_tree(r);
}

int main(int argc, char *argv[])
{
	test_lookup_1();
	test_lookup_2();

	return 0;
}
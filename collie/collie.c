/*
 * Copyright (C) 2009 Nippon Telegraph and Telephone Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syslog.h>

#include "collie.h"

#define EPOLL_SIZE 4096
#define DEFAULT_OBJECT_DIR "/tmp"
#define LOG_FILE_NAME "collie.log"

static char program_name[] = "collie";

static struct option const long_options[] = {
	{"port", required_argument, 0, 'p'},
	{"foreground", no_argument, 0, 'f'},
	{"loglevel", required_argument, 0, 'l'},
	{"debug", no_argument, 0, 'd'},
	{"help", no_argument, 0, 'h'},
	{0, 0, 0, 0},
};

static char *short_options = "p:fl:dh";

static void usage(int status)
{
	if (status)
		fprintf(stderr, "Try `%s --help' for more information.\n",
			program_name);
	else {
		printf("Usage: %s [OPTION] [PATH]\n", program_name);
		printf("\
Sheepdog Daemon, version %s\n\
  -p, --port              specify the listen port number\n\
  -f, --foreground        make the program run in the foreground\n\
  -l, --loglevel          specify the message level printed by default\n\
  -d, --debug             print debug messages\n\
  -h, --help              display this help and exit\n\
", SD_VERSION);
	}
	exit(status);
}

struct cluster_info __sys, *sys = &__sys;

int main(int argc, char **argv)
{
	int ch, longindex;
	int ret, port = SD_LISTEN_PORT;
	char *dir = DEFAULT_OBJECT_DIR;
	int is_daemon = 1;
	int log_level = LOG_INFO;
	char path[PATH_MAX];

	while ((ch = getopt_long(argc, argv, short_options, long_options,
				 &longindex)) >= 0) {
		switch (ch) {
		case 'p':
			port = atoi(optarg);
			break;
		case 'f':
			is_daemon = 0;
			break;
		case 'l':
			log_level = atoi(optarg);
			break;
		case 'd':
			/* removed soon. use loglevel instead */
			log_level = LOG_DEBUG;
			break;
		case 'h':
			usage(0);
			break;
		default:
			usage(1);
			break;
		}
	}

	if (optind != argc)
		dir = argv[optind];

	snprintf(path, sizeof(path), "%s/" LOG_FILE_NAME, dir);

	ret = init_store(dir);
	if (ret)
		exit(1);

	ret = log_init(program_name, LOG_SPACE_SIZE, is_daemon, log_level, path);
	if (ret)
		exit(1);

	if (is_daemon && daemon(0, 0))
		exit(1);

	ret = init_event(EPOLL_SIZE);
	if (ret)
		exit(1);

	dobj_queue = init_work_queue(DATA_OBJ_NR_WORKER_THREAD);
	if (!dobj_queue)
		exit(1);

	ret = create_listen_port(port, sys);
	if (ret)
		exit(1);

	ret = create_cluster(port);
	if (ret) {
		eprintf("failed to create sheepdog cluster.\n");
		exit(1);
	}

	vprintf(SDOG_NOTICE "Sheepdog daemon (version %s) started\n", SD_VERSION);

	while (sys->status != SD_STATUS_SHUTDOWN)
		event_loop(-1);

	vprintf(SDOG_INFO "shutdown\n");

	log_close();

	return 0;
}

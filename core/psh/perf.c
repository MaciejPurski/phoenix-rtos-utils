#include <sys/threads.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>


int psh_perf(char *args)
{
	time_t timeout_s;
	unsigned len;
	time_t timeout, elapsed = 0, sleeptime = 200 * 1000;
	unsigned char *buffer;
	const size_t bufsz = 4 << 20;
	int bcount = 0, tcnt, n = 32;
	threadinfo_t *info, *info_re;
	unsigned ncpus = 0;
	unsigned (*threadEvents)[4];
	unsigned **threadsCpu;
	unsigned events[4] = {0, 0, 0, 0};
	unsigned levents[5] = {};
	unsigned i = 0;
	unsigned j;
	unsigned type;
	perf_event_t pevent;

	int argc = 1;
	unsigned maxargs = 8;
	char **argv;
	char *tmp;
	int optindex = 0;
	int ch;

	int timeMode = 0;


	/* Temporary solution until args from psh.c are
	 * passed as argc, *argv[] */
	argv = malloc(maxargs * sizeof(char *));
	if (argv == NULL) {
		printf("perf: out of memory\n");
		return -ENOMEM;
	}

	argv[0] = "perf";
	tmp = strtok(args, " ");
	while (tmp != NULL) {
		argv[argc] = tmp;
		argc++;
		tmp = strtok(NULL, " ");
	}
	argv[argc] = NULL;

	while ((ch = getopt(argc, argv, "t:cp")) != -1) {
		switch (ch)
		{
		case 't':
			printf("Thread arg ");
			if (optarg != NULL)
				printf("%s", optarg);
			putchar('\n');
			break;

		case 'c':
			printf("cpu arg ");
			if (optarg != NULL)
				printf("%s", optarg);
			putchar('\n');
			break;
		case '?':
			printf("Unkown argument: -%c\n", ch);
			return -1;
			break;
		default:
			return -1;
		}
	}

	if (optind < argc) {
		timeout_s = strtoull(argv[optind++], NULL, 10);
		if (timeout_s == 0) {
			printf("perf: Required greater than 0 integer\n");
			return -1;
		}
    }
	else {
		printf("perf: Time argument missing!\n");
		return -1;
	}

	if (optind < argc) {
		printf("perf: Too many arguments\n");
		return -1;
	}

	printf("timeout: %d\n", timeout_s);
	if ((info = malloc(n * sizeof(threadinfo_t))) == NULL) {
		fprintf(stderr, "perf: out of memory\n");
		return -ENOMEM;
	}

	while ((tcnt = threadsinfo(n, info)) >= n) {
		n *= 2;
		if ((info_re = realloc(info, n * sizeof(threadinfo_t))) == NULL) {
			free(info);
			fprintf(stderr, "perf: out of memory\n");
			return -ENOMEM;
		}

		info = info_re;
	}

	/* Workaround to count number of cpus */
	for (i = 0; i < tcnt; i++) {
		if (!strcmp(info[i].name, "[idle]"))
			ncpus++;
	}
	printf("ncpus: %d\n", ncpus);

	threadEvents = calloc(tcnt, sizeof(*threadEvents));
	if (threadEvents == NULL) {
		free(info);
		fprintf(stderr, "perf: out of memory\n");
		return -ENOMEM;
	}

	threadsCpu = malloc(tcnt * sizeof(unsigned *));
	if (threadsCpu == NULL) {
		free(info);
		/* TODO: handle memory */
		fprintf(stderr, "perf: out of memory\n");
		return -ENOMEM;
	}

	for (i = 0; i < tcnt; i++) {
		threadsCpu[i] = calloc(ncpus, sizeof(unsigned));
		if (threadsCpu[i] == NULL) {
			fprintf(stderr, "perf: out of memory\n");
			/* TODO: free other buffers */
			return -ENOMEM;
		}
	}

	timeout = 1000 * 1000 * timeout_s;

	buffer = malloc(bufsz);

	if (buffer == NULL) {
		fprintf(stderr, "perf: out of memory\n");
		return -ENOMEM;
	}

	if (perf_start(-1) < 0) {
		fprintf(stderr, "perf: could not start\n");
		free(buffer);
		return -1;
	}

	while (elapsed < timeout) {
		bcount = perf_read(buffer, bufsz);

		for (i = 0; i < bcount;) {
			//printf("event timestamp: %d tid: %d event type: %d\n", buffer[i].deltaTimestamp, buffer[i].tid, buffer[i].type);
			/* levent */
			if (buffer[i] == 0 && buffer[i + 1] == 0 && buffer[i + 2] == 0 &&
				buffer[i + 3] == 0) {
				type = (buffer[i + 1] >> 12) & 0x7;
				//printf("levent: %x\n", buffer[i + 1]);
				levents[type]++;
				switch (type)
				{
				case perf_levBegin:
					i += sizeof(perf_levent_begin_t);
					break;
				case perf_levEnd:
					i += sizeof(perf_levent_end_t);
					break;
				case perf_levFork:
					i += sizeof(perf_levent_fork_t);
					break;
				case perf_levKill:
					i += sizeof(perf_levent_kill_t);
					break;
				case perf_levExec:
					i += sizeof(perf_levent_exec_t);
				default:
					printf("perf: Unknown event type\n");
					i++;
					break;
				}
			}
			else {
				memcpy(&pevent, buffer + i, sizeof(perf_event_t));
				events[pevent.type]++;
				for (j = 0; j < tcnt; j++) {
					if (info[j].tid == pevent.tid) {
						threadEvents[j][pevent.type]++;
						if (pevent.type == perf_evScheduling)
							threadsCpu[j][pevent.cpuid]++;
						break;
					}
				}
				i += sizeof(perf_event_t);
			}

		}

		usleep(sleeptime);
		elapsed += sleeptime;
	}

	printf("\n%-15s %-d\n", "Scheduling:", events[0]);
	printf("%-15s %-d\n", "Enqueued:", events[1]);
	printf("%-15s %-d\n", "Waking:", events[2]);
	printf("%-15s %-d\n", "Preempted:", events[3]);

	printf("\n%-15s %-d\n", "Begin:", levents[0]);
	printf("%-15s %-d\n", "End:", levents[1]);
	printf("%-15s %-d\n", "Fork:", levents[2]);
	printf("%-15s %-d\n", "Kill:", levents[3]);
	printf("%-15s %-d\n", "Exec:", levents[4]);

	printf("%-10s %5s %10s %10s %10s %10s\n", "CMD", "TID", "Scheduling", "Enqueued", "Waking", "Preempted");
	for (i = 0; i < tcnt; i++)
		printf("%-10s %5d %10d %10d %10d %10d\n", info[i].name,
										info[i].tid,
										threadEvents[i][0],
										threadEvents[i][1],
										threadEvents[i][2],
										threadEvents[i][3]);

	
	printf("%-10s %5s ", "CMD", "TID");
	char buf[8];

	for (i = 0; i < ncpus; i++) {
		sprintf(buf, "CPU%d", i);
		printf("%5s ", buf);
	}
	putchar('\n');
	for (i = 0; i < tcnt; i++) {
		printf("%-10s %5d ", info[i].name, info[i].tid);
		for (j = 0; j < ncpus; j++)
			printf("%5d ", threadsCpu[i][j]);
		putchar('\n');
	}

	for (i = 0; i < ncpus; i++) {
		unsigned sum = 0;

		for (j = 0; j < tcnt; j++)
			sum += threadsCpu[j][i];

		sprintf(buf, "CPU%d", i);
		printf("%-5s %5d\n", buf, sum);
	}


	// for (i = 0; i < 60; i++)
	// 	putchar('-');
	// putchar('\n');
	// printf("%-16s %10u %10u %10u %10u\n", "Total", events[0], events[1], events[2], events[3]);
	perf_finish();
	free(buffer);
	free(threadEvents);
	free(info);
	free(threadsCpu);
	return EOK;
}
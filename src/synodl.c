/*

SynoDL - CLI for Synology's DownloadStation
Copyright (C) 2015  Stefan Ott

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <getopt.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "cfg.h"
#include "syno.h"
#include "ui.h"

void help()
{
	printf("Syntax: synodl [options] [URL]\n\n");
	printf("If URL is empty a list of current download tasks is shown,\n");
	printf("otherwise the URL is added as a download task.\n\n");
	printf("  -h, --help           Show this help\n");
	printf("      --dump           Show current downloads\n");
	printf("  -p, --pause=ID       Pause download\n");
	printf("  -r, --resume=ID      Resume download\n");
	printf("\n");
	printf("This is %s.\n", PACKAGE_STRING);
	printf("Report bugs at https://github.com/cockroach/synodl/\n");
}

int main(int argc, char **argv)
{
	int c, option_idx;
	const char *url, *pause, *resume;
	struct cfg config;
	struct session s;
	struct syno_ui ui;

	setlocale(LC_ALL, "");

	struct option long_options[] =
	{
		{"help",   no_argument,       0, 'h' },
		{"dump",   no_argument,       0,  0 },
		{"pause",  required_argument, 0, 'p' },
		{"resume", required_argument, 0, 'r' },
		{0,        0,                 0,  0 }
	};

	memset(&config, 0, sizeof(struct cfg));
	memset(&ui, 0, sizeof(struct syno_ui));

	pause = "";
	resume = "";
	curses_ui(&ui);

	while (1)
	{
		c = getopt_long(argc, argv, "hp:r:", long_options, &option_idx);

		if (c == -1)
		{
			break;
		}

		switch (c)
		{
		case 0:
			switch (option_idx)
			{
			case 1:
				console_ui(&ui);
				break;
			}
			break;
		case 'h':
			help();
			return EXIT_SUCCESS;
		case 'p':
			pause = optarg;
			console_ui(&ui);
			break;
		case 'r':
			resume = optarg;
			console_ui(&ui);
			break;
		default:
			help();
			return EXIT_FAILURE;
		}
	}

	if (load_config(&config) != 0)
	{
		fprintf(stderr, "Failed to load configuration\n");
		return EXIT_FAILURE;
	}

	memset(&s, 0, sizeof(struct session));

	if (syno_login(config.url, &s, config.user, config.pw) != 0)
	{
		return EXIT_FAILURE;
	}

	ui.init();

	if (optind < argc)
	{
		url = argv[optind];
		syno_download(config.url, &s, url);
	}
	else
	{
		if (strcmp(pause, ""))
		{
			syno_pause(config.url, &s, pause);
		}
		if (strcmp(resume, ""))
		{
			syno_resume(config.url, &s, resume);
		}
		syno_list(config.url, &s, ui.add_task);
		ui.render();
	}

	ui.loop(&ui, config.url, &s);
	syno_logout(config.url, &s);
	ui.stop();
	ui.free();

	return EXIT_SUCCESS;
}

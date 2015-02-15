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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cfg.h"
#include "http.h"

int main(int argc, char **argv)
{
	struct cfg config;
	const char *url;

	memset(&config, 0, sizeof(struct cfg));

	if (load_config(&config) != 0)
	{
		fprintf(stderr, "Failed to load configuration\n");
		return EXIT_FAILURE;
	}

	struct session s;
	memset(&s, 0, sizeof(struct session));

	if (login(config.url, &s, config.user, config.pw) != 0)
	{
		return EXIT_FAILURE;
	}

	if (argc > 1)
	{
		url = argv[1];
		download(config.url, &s, url);
	}
	else
	{
		info(config.url, &s);
	}

	logout(config.url, &s);

	return EXIT_SUCCESS;
}

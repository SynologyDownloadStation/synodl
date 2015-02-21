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

#ifndef __SYNODL_UI_H
#define __SYNODL_UI_H

#include "task.h"

struct syno_ui
{
	void (*init)();
	void (*stop)();
	int (*status)(const char *fmt, ...);
	void (*loop)(struct syno_ui *ui, const char *base, struct session *s);
	void (*add_task)(struct download_task *t);
	void (*free)();
	void (*render)();
};

void console_ui();
void curses_ui();

#endif

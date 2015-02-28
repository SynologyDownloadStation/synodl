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

#include <string.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <json-c/json_tokener.h>

#include "syno.h"
#include "ui.h"

struct string
{
	int size;
	char *ptr;
};

static int
init_string(struct string *s)
{
	s->size = 1;
	s->ptr = malloc(s->size);

	if (!s->ptr)
	{
		fprintf(stderr, "Malloc failed\n");
		return 1;
	}

	return 0;
}

static void
free_string(struct string *s)
{
	free(s->ptr);
}

static void
json_load_login(json_object *obj, struct session *s)
{
	int login_success;
	json_object *success, *data, *sid;

	success = json_object_object_get(obj, "success");
	if (!success)
	{
		fprintf(stderr, "Value 'success' missing from %s\n",
						json_object_get_string(obj));
		return;
	}

	login_success = json_object_get_int(success);
	if (login_success != 1)
	{
		printf("%s\n", json_object_get_string(obj));
		return;
	}

	data = json_object_object_get(obj, "data");
	if (!data)
	{
		fprintf(stderr, "Value 'data' missing from %s\n",
						json_object_get_string(obj));
		return;
	}

	sid = json_object_object_get(data, "sid");
	snprintf(s->sid, sizeof(s->sid), "%s", json_object_get_string(sid));
}

static void
json_load_tasks(json_object *obj, struct session *s, struct syno_ui *ui)
{
	json_object *data, *tasks, *task, *tmp, *additional, *transfer;
	struct download_task dt;
	int i;

	data = json_object_object_get(obj, "data");
	if (!data)
	{
		fprintf(stderr, "Value 'data' missing from %s\n",
						json_object_get_string(obj));
		return;
	}

	tasks = json_object_object_get(data, "tasks");
	if (!tasks)
	{
		fprintf(stderr, "No tasks found\n");
		return;
	}

	if (json_object_get_type(tasks) != json_type_array)
	{
		fprintf(stderr, "Invalid type returned for tasks\n");
		return;
	}

	for (i=0; i < json_object_array_length(tasks); i++)
	{
		memset(&dt, 0, sizeof(struct download_task));

		task = json_object_array_get_idx(tasks, i);

		tmp = json_object_object_get(task, "id");
		snprintf(dt.id, sizeof(dt.id), "%s",
						json_object_get_string(tmp));

		tmp = json_object_object_get(task, "title");
		snprintf(dt.fn, sizeof(dt.fn), "%s",
						json_object_get_string(tmp));

		tmp = json_object_object_get(task, "status");
		snprintf(dt.status, sizeof(dt.status), "%s",
						json_object_get_string(tmp));

		tmp = json_object_object_get(task, "size");
		dt.size = json_object_get_int(tmp);

		additional = json_object_object_get(task, "additional");
		transfer = json_object_object_get(additional, "transfer");

		if (transfer)
		{
			tmp = json_object_object_get(transfer,
							"size_downloaded");
			dt.downloaded = json_object_get_int(tmp);

			tmp = json_object_object_get(transfer,"speed_download");
			dt.speed_dn = json_object_get_int(tmp);

			tmp = json_object_object_get(transfer, "speed_upload");
			dt.speed_up = json_object_get_int(tmp);

			if ((dt.size != 0) && (dt.downloaded != 0))
			{
				dt.percent_dn = ((float)dt.downloaded / dt.size)
									* 100;
			}
		}

		tmp = json_object_object_get(transfer, "size_uploaded");
		dt.uploaded = json_object_get_int(tmp);

		ui->add_task(&dt);
	}
}

static int
session_load(struct string *st, struct session *session)
{
	json_tokener *tok;
	json_object *obj;

	tok = json_tokener_new();
	if (!tok)
	{
		fprintf(stderr, "Failed to initialize JSON tokener\n");
		return 1;
	}

	obj = json_tokener_parse_ex(tok, st->ptr, st->size);
	json_tokener_free(tok);

	if (is_error(obj))
	{
		fprintf(stderr, "Failed to decode JSON data\n");
		json_object_put(obj);
		return 1;
	}

	json_load_login(obj, session);
	json_object_put(obj);
	return 0;
}

static int
tasks_receive(struct string *st, struct syno_ui *ui)
{
	json_tokener *tok;
	json_object *obj;

	tok = json_tokener_new();
	if (!tok)
	{
		fprintf(stderr, "Failed to initialize JSON tokener\n");
		return 1;
	}

	obj = json_tokener_parse_ex(tok, st->ptr, st->size);
	json_tokener_free(tok);

	if (is_error(obj))
	{
		fprintf(stderr, "Failed to decode JSON data\n");
		json_object_put(obj);
		return 1;
	}

	json_load_tasks(obj, NULL, ui);
	json_object_put(obj);
	return 0;
}

static int
dump_reply(struct string *st)
{
	json_tokener *tok;
	json_object *obj;

	tok = json_tokener_new();
	if (!tok)
	{
		fprintf(stderr, "Failed to initialize JSON tokener\n");
		return 1;
	}

	obj = json_tokener_parse_ex(tok, st->ptr, st->size);
	json_tokener_free(tok);

	if (is_error(obj))
	{
		fprintf(stderr, "Failed to decode JSON data\n");
		json_object_put(obj);
		return 1;
	}

	printf("RES=%s\n", json_object_get_string(obj));
	json_object_put(obj);
	return 0;
}

static size_t
curl_recv(void *ptr, size_t size, size_t nmemb, struct string *s)
{
	int pos;

	pos = s->size - 1;
	s->size += (size * nmemb);
	s->ptr = realloc(s->ptr, s->size);

	if (!s->ptr)
	{
		fprintf(stderr, "Realloc failed");
		return -1;
	}

	memcpy(s->ptr + pos, ptr, size * nmemb);
	return size * nmemb;
}

static int
curl_do(const char *url, void *cb_arg, struct string *st)
{
	CURL *curl;
	CURLcode res;

	curl_global_init(CURL_GLOBAL_DEFAULT);
	curl = curl_easy_init();

	if (!curl)
	{
		fprintf(stderr, "Failed to initialize CURL\n");
		curl_global_cleanup();
		return 1;
	}

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_recv);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, st);

	res = curl_easy_perform(curl);

	if (res != CURLE_OK)
	{
		fprintf(stderr, "curl_easy_perform() failed: %s\n",
						curl_easy_strerror(res));
		curl_easy_cleanup(curl);
		curl_global_cleanup();
		return 1;
	}

	curl_easy_cleanup(curl);
	curl_global_cleanup();

	return 0;
}

int
syno_login(struct syno_ui *ui, const char *base, struct session *s,
						const char *usr, const char *pw)
{
	char url[1024];
	struct string st;

	ui->status("Logging in...");

	init_string(&st);

	snprintf(url, sizeof(url), "%s/webapi/auth.cgi?api=SYNO.API.Auth"
		"&version=2&method=login&account=%s&passwd=%s"
		"&session=DownloadStation&format=sid", base, usr, pw);

	if (curl_do(url, s, &st) != 0)
	{
		fprintf(stderr, "Login failed\n");
		free_string(&st);
		return 1;
	}

	session_load(&st, s);
	free_string(&st);

	if (!strcmp(s->sid, ""))
	{
		fprintf(stderr, "Login failed\n");
		return 1;
	}

	ui->status("Connected");
	return 0;
}

int
syno_logout(struct syno_ui *ui, const char *base, struct session *s)
{
	char url[1024];
	struct string st;

	init_string(&st);

	snprintf(url, sizeof(url), "%s/webapi/auth.cgi?api=SYNO.API.Auth"
		"&version=1&method=logout&session=DownloadStation"
		"&_sid=%s", base, s->sid);

	if (curl_do(url, s, &st) != 0)
	{
		free_string(&st);
		return 1;
	}

	free_string(&st);
	return 0;
}

int
syno_info(struct syno_ui *ui, const char *base, struct session *s)
{
	char url[1024];
	int res;
	struct string st;

	init_string(&st);

	snprintf(url, sizeof(url), "%s/webapi/DownloadStation/task.cgi?"
				"api=SYNO.DownloadStation.Task&version=2"
				"&method=list&additional=transfer&_sid=%s",
				base, s->sid);

	if (curl_do(url, s, &st) != 0)
	{
		free_string(&st);
		return 1;
	}

	ui->free();
	res = tasks_receive(&st, ui);
	ui->render();
	free_string(&st);

	return res;
}

int
syno_download(struct syno_ui *ui, const char *base, struct session *s,
							const char *dl_url)
{
	char url[1024], *esc;
	int res;
	struct string st;

	init_string(&st);

	esc = curl_escape(dl_url, strlen(dl_url));
	snprintf(url, sizeof(url), "%s/webapi/DownloadStation/task.cgi?"
			"api=SYNO.DownloadStation.Task&version=2&method=create"
			"&uri=%s&_sid=%s", base, esc, s->sid);
	curl_free(esc);

	if (curl_do(url, s, &st) != 0)
	{
		free_string(&st);
		return 1;
	}

	dump_reply(&st);

	free_string(&st);
	return res;
}

int
syno_pause(struct syno_ui *ui, const char *base, struct session *s,
								const char *ids)
{
	char url[1024];
	int res;
	struct string st;

	init_string(&st);

	snprintf(url, sizeof(url), "%s/webapi/DownloadStation/task.cgi?"
				"api=SYNO.DownloadStation.Task&version=1"
				"&method=pause&id=%s&_sid=%s", base, ids,
				s->sid);

	if (curl_do(url, s, &st) != 0)
	{
		free_string(&st);
		return 1;
	}

	dump_reply(&st);

	free_string(&st);
	return res;
}

int
syno_resume(struct syno_ui *ui, const char *base, struct session *s,
								const char *ids)
{
	char url[1024];
	int res;
	struct string st;

	init_string(&st);

	snprintf(url, sizeof(url), "%s/webapi/DownloadStation/task.cgi?"
				"api=SYNO.DownloadStation.Task&version=1"
				"&method=resume&id=%s&_sid=%s", base, ids,
				s->sid);

	if (curl_do(url, s, &st) != 0)
	{
		free_string(&st);
		return 1;
	}

	dump_reply(&st);

	free_string(&st);
	return res;
}

int
syno_delete(struct syno_ui *ui, const char *base, struct session *s,
								const char *ids)
{
	char url[1024];
	int res;
	struct string st;

	init_string(&st);

	snprintf(url, sizeof(url), "%s/webapi/DownloadStation/task.cgi?"
				"api=SYNO.DownloadStation.Task&version=1"
				"&method=delete&id=%s&_sid=%s", base, ids,
				s->sid);

	if (curl_do(url, s, &st) != 0)
	{
		free_string(&st);
		return 1;
	}

	dump_reply(&st);

	free_string(&st);
	return res;
}

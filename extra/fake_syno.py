#!/usr/bin/env python

import json

from tornado.ioloop import IOLoop
from tornado.web import RequestHandler, Application

class AuthHandler(RequestHandler):

	def get(self):

		res = {}
		data = {}

		res['success'] = True

		method = self.get_argument('method')
		if method == 'login':
			data['sid'] = 1

		res['data'] = data
		self.write(json.dumps(res))

class TaskHandler(RequestHandler):

	def get(self):

		res = {}
		data = {}

		res['success'] = True;

		method = self.get_argument('method')
		if method == 'list':
			tasks = []
			tasks.append({
				'id': 1,
				'title': 'test 1',
				'status': 'downloading',
				'size': 1234,
				'additional': {
					'transfer': {
						'size_downloaded': 500,
						'size_uploaded': 250,
						'speed_download': 100,
						'speed_upload': 50,
					}
				}
			})
			tasks.append({
				'id': 1,
				'title': 'test 2',
				'status': 'paused',
				'size': 1234567,
				'additional': {
					'transfer': {
						'size_downloaded': 900000,
						'size_uploaded': 25000,
						'speed_download': 100,
						'speed_upload': 50,
					}
				}
			})
			data['tasks'] = tasks

		res['data'] = data
		self.write(json.dumps(res))

application = Application([
	(r"/webapi/auth.cgi", AuthHandler),
	(r"//webapi/auth.cgi", AuthHandler),
	(r"/webapi/DownloadStation/task.cgi", TaskHandler),
	(r"//webapi/DownloadStation/task.cgi", TaskHandler),
])

if __name__ == "__main__":

	application.listen(8888)
	IOLoop.instance().start()

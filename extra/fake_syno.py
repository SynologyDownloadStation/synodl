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

	tasks = []

	def get(self):

		res = {}
		data = {}

		res['success'] = True;

		method = self.get_argument('method')

		print(method)

		if method == 'create':
			task = {}
			task['id'] = len(self.tasks)
			task['title'] = self.get_argument('uri')
			task['status'] = 'downloading'
			task['size'] = 1234

			transfer = {}
			transfer['size_downloaded'] = 500
			transfer['size_uploaded'] = 250
			transfer['speed_download'] = 1000
			transfer['speed_upload'] = 50

			task['additional'] = { 'transfer': transfer }
			self.tasks.append(task)
		elif method == 'delete':
			id = int(self.get_argument('id'))
			del(self.tasks[id])
		elif method == 'list':
			data['tasks'] = self.tasks

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

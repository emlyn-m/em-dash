import asyncio
import slixmpp
from slixmpp.plugins.xep_0313 import stanza
import fastapi
import time
import json
import re
import os

from pydantic import BaseModel
import logging


class Message(BaseModel):
	msg_timestamp: int
	msg_class: int
	msg_sev: int
	msg_body: str


class Listener(slixmpp.ClientXMPP):
	def __init__(self, msg_queue, jid, password):
		self.msg_queue = msg_queue

		slixmpp.ClientXMPP.__init__(self, jid, password)
		self['feature_mechanisms'].unencrypted_plain = True
		self['feature_mechanisms'].unencrypted_digest = True
		self['feature_mechanisms'].unencrypted_scram = True
		self.register_plugin('xep_0313')
		self.register_plugin('xep_0030')
		self.register_plugin('xep_0059')
		self.register_plugin('xep_0297')

		self.add_event_handler("session_start", self.start)
		self.add_event_handler("message", self.on_message)
		self.add_event_handler("mam_archived_message", self.on_message)

	async def retrieve_messages(self):
		try:
			results = await self['xep_0313'].retrieve()
			print(f'     \x1b[48;5;030m INFO \x1b[0m  Received {len(results['mam']['results'])} archived msgs')
			for result in results['mam']['results']:
				await self.on_message(result)

		except Exception as e:
			print(f'    \x1b[48;5;030m ERROR \x1b[0m  {e}')

	async def start(self, event):
		self.send_presence()
		await self.get_roster()
		print('     \x1b[48;5;030m INFO \x1b[0m  Roster received, fetching archive')
		await self.retrieve_messages()
		print('     \x1b[48;5;030m INFO \x1b[0m  Startup complete')

	async def on_message(self, msg):
		msg_body = re.search( '<body>(.*)</body>', str(msg) ).group(1)
		msg_content = json.loads( msg_body.replace('&quot;', '"') )
		msg_timestamp = msg_content['timestamp']
		msg_class = msg_content['class']
		msg_sev = msg_content['sev']
		msg_body = msg_content['body']

		#print(f'     \x1b[48;5;030m INFO \x1b[0m  Received a class {msg_class} msg (sev={msg_sev}) \'{msg_body}\' (sent at {msg_timestamp})')
		await self.msg_queue.put(Message(
			msg_timestamp=msg_timestamp,
			msg_class=msg_class,
			msg_sev=msg_sev,
			msg_body=msg_body
		))


def init_listener():
	msg_queue = asyncio.Queue()
	xmpp_listener = Listener(msg_queue, os.environ['CLIENT_JID'], os.environ['CLIENT_PWD'])
	xmpp_listener.register_plugin('xep_0199')
	xmpp_listener.connect()
	yield msg_queue

	try:
		asyncio.get_event_loop().run()
	except KeyboardInterrupt:
		print('     \x1b[48;5;030m INFO \x1b[0m  saying goodnight')


msg_queue = next(init_listener())
msg_archive = []
router = fastapi.APIRouter()

def msg_sendcheck(msg: Message, before: int|None=None, after: int|None=None, msg_class: int|None=None, max_sev: int|None=None):
	if (before is not None) and msg.msg_timestamp > before: return False
	if (after is not None) and msg.msg_timestamp < after: return False
	if (msg_class is not None) and (msg.msg_class != msg_class): return False
	if (max_sev is not None) and (msg.msg_sev > max_sev): return False

	return True


@router.get('/api/xmpp')
async def get_msgs(max_events:int|None=None, before: int|None=None, after: int|None=None, msg_class: int|None=None, max_sev: int|None=None) -> list[Message]:
	try:
		while latest_item := msg_queue.get_nowait(): msg_archive.append(latest_item)
	except asyncio.QueueEmpty:
		print(f'     \x1b[48;5;030m INFO \x1b[0m  have {len(msg_archive)} msgs')

	events = list(filter( lambda msg: msg_sendcheck(msg, before, after, msg_class, max_sev), msg_archive ))
	events.sort(key=lambda msg: -msg.msg_timestamp)
	events_rsp = events[:(max_events if max_events else len(events))]

	print(f'     \x1b[48;5;030m INFO \x1b[0m  sending {len(events_rsp)} msgs')
	return events_rsp

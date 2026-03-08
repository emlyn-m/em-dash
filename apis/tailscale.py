from fastapi import APIRouter, Response, status
from pydantic import BaseModel, model_validator
from typing import Union, Callable
from enum import Enum
import asyncio
import aiohttp
import ssl
import subprocess
import json
import logging
import os

class Err(BaseModel):
	msg: str

class TSDevice(BaseModel):
	name: str
	alias: str
	ip: str
	online: bool

class TSHealth(str, Enum):
	HEALTHY='healthy'
	DEGRADED='degraded'
	DOWN='down'
	CHECK_FAILED='check_failed'

class TSService(BaseModel):
	name: str
	status: TSHealth
	details: str|None = None

	@model_validator(mode='after')
	def validate_service(self) -> 'TSService':
		if self.details and self.status in (TSHealth.HEALTHY, TSHealth.DOWN):
			raise ValueError('["healthy", "down"] states cannot have details')
		return self

class TSServiceCheck(BaseModel):
    url: str
    handler: Callable[[aiohttp.ClientResponse, str], TSService]


router = APIRouter()
@router.get('/api/tailscale/devices', response_model_exclude_unset=True)
async def fetch_devices(response: Response) -> Union[list[TSDevice], Err]:
	logger = logging.getLogger(__name__)
	device_run = subprocess.run(["tailscale", "status", "--json"], capture_output=True, text=True)
	if (device_run.returncode):
		response.status_code = status.HTTP_500_INTERNAL_SERVER_ERROR
		return Err(msg=device_run.stderr.strip())

	device_j = json.loads(device_run.stdout)
	devs = [ TSDevice(
		name=device_j['Self']['HostName'],
		alias=device_j['Self']['DNSName'].split('.')[0],
		ip=device_j['Self']['TailscaleIPs'][0],
		online=True
	) ]
	for peer_id in device_j['Peer'].keys():
		devs.append(TSDevice(
			name=device_j['Peer'][peer_id]['HostName'],
			alias=device_j['Peer'][peer_id]['DNSName'].split('.')[0],
			ip=device_j['Peer'][peer_id]['TailscaleIPs'][0],
			online=device_j['Peer'][peer_id]['Online']
		))

	return devs


def execcheck_jellyfin(rsp: aiohttp.ClientResponse, txt: str) -> TSService:
	jellyfin_status = (
		TSHealth.HEALTHY if txt.strip() == 'Healthy'
		else TSHealth.DEGRADED if rsp
		else TSHealth.DOWN
	)
	return TSService(
		name='Jellyfin',
		status=jellyfin_status,
		details=(
			f'/health: {rsp.status} {txt}' if jellyfin_status == TSHealth.DEGRADED
			else None
		)
	)

def execcheck_mikochi(rsp: aiohttp.ClientResponse, txt: str) -> TSService:
	mikochi_status = (
		TSHealth.DOWN if not rsp
		else TSHealth.DEGRADED if not rsp.ok
		else TSHealth.HEALTHY
	)
	return TSService(
		name='Mikochi',
		status=mikochi_status,
		details=(
			f'http status: {rsp.status}' if mikochi_status == TSHealth.DEGRADED
			else None
		)
	)

def execcheck_prosody(rsp: aiohttp.ClientResponse, txt: str) -> TSService:
	prosody_status = (
		TSHealth.HEALTHY if txt.strip() == 'OK' else
		TSHealth.DEGRADED if rsp else
		TSHealth.DOWN
	)
	return TSService(
		name='Prosody',
		status=prosody_status,
		details=(
			f'/health: {rsp.status} {txt}' if prosody_status == TSHealth.DEGRADED
			else None
		)
	)

def execcheck_deluge(rsp: aiohttp.ClientResponse, txt: str) -> TSService:
	deluge_status = (
		TSHealth.DOWN if not rsp
		else TSHealth.DEGRADED if not rsp.ok
		else TSHealth.HEALTHY
	)
	return TSService(
		name='Deluge',
		status=deluge_status,
		details=(
			f'/health: {rsp.status}' if deluge_status == TSHealth.DEGRADED
			else None
		)
	)

def execcheck_matrix(rsp: aiohttp.ClientResponse, txt: str):
	matrix_status = (
		TSHealth.HEALTHY if txt.strip() == 'OK'
		else TSHealth.DEGRADED if rsp
		else TSHealth.DOWN
	)
	return TSService(
		name='Matrix',
		status=matrix_status,
		details=(
			f'/health: {rsp.status} {txt}' if matrix_status == TSHealth.DEGRADED
			else None
		)
	)

async def execcheck(session: aiohttp.ClientSession, url: str, handler: Callable[[aiohttp.ClientResponse, str], TSService]) -> TSService:
	try:
		async with session.get(url, ssl=ssl.SSLContext()) as rsp:
			service = handler(rsp, await rsp.text())
	except (aiohttp.client_exceptions.ClientConnectorDNSError, aiohttp.client_exceptions.InvalidUrlClientError):
		service = handler(None, '')

	#print(f'     \x1b[48;5;030m INFO \x1b[0m  Found service {service.name} - {service.status} per {url} {f"({service.details})" if service.details else ""}')
	return service


status_checks = [
	TSServiceCheck(url=os.environ['JELLYFIN_HEALTH_ENDPOINT'], handler=execcheck_jellyfin ),
	TSServiceCheck(url=os.environ['MIKOCHI_HEALTH_ENDPOINT'],  handler=execcheck_mikochi  ),
	TSServiceCheck(url=os.environ['PROSODY_HEALTH_ENDPOINT'],  handler=execcheck_prosody  ),
	TSServiceCheck(url=os.environ['DELUGE_HEALTH_ENDPOINT'],   handler=execcheck_deluge   ),
	TSServiceCheck(url=os.environ['MATRIX_HEALTH_ENDPOINT'],   handler=execcheck_matrix   )
]


@router.get('/api/tailscale/services', response_model_exclude_unset=True, response_model_exclude_none=True)
async def fetch_services() -> Union[list[TSService], Err]:
	ts_services = []

	# http - general case
	async with aiohttp.ClientSession() as session:
		ts_services = await asyncio.gather(*[execcheck(session, chk.url, chk.handler) for chk in status_checks], return_exceptions=True)
	ts_services = list(filter(lambda s: s.__class__ == TSService, ts_services))

	# non-netcat - special case
	execcheck_mc = subprocess.run(['nc', '-vz', os.environ['MINECRAFT_HEALTH_ENDPOINT'], str(os.environ['MINECRAFT_HEALTH_PORT'])], capture_output=True, text=True)
	minecraft_status =(
		TSHealth.DOWN if execcheck_mc.returncode == 1
		else TSHealth.HEALTHY if execcheck_mc.stderr.strip().endswith('succeeded!')
		else TSHealth.CHECK_FAILED
	)
	ts_services.append(TSService(
		name='Minecraft',
		status=minecraft_status,
		details=(
			f'nc: <{execcheck_mc.stdout.strip()}> <{execcheck_mc.stderr.strip()}>' if minecraft_status == TSHealth.CHECK_FAILED
			else None
		)
	))

	return ts_services

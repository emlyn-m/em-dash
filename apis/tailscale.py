from fastapi import APIRouter, Response, status
from pydantic import BaseModel, model_validator
from typing import Union
from enum import Enum
import subprocess
import json
import logging

JELLYFIN_HEALTH_ENDPOINT  = 'https://jellyfin.tailscale.emlyn.xyz/health/'
MIKOCHI_HEALTH_ENDPOINT   = 'https://mikochi.tailscale.emlyn.xyz/'
PROSODY_HEALTH_ENDPOINT   = 'http://prosody.tailscale.emlyn.xyz/health'
DELUGE_HEALTH_ENDPOINT    = 'https://deluge.tailscale.emlyn.xyz'
MATRIX_HEALTH_ENDPOINT    = 'https://matrix.tailscale.emlyn.xyz/health'
MINECRAFT_HEALTH_ENDPOINT = 'minecraft.tailscale.emlyn.xyz'
MINECRAFT_HEALTH_PORT     = 25565

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

@router.get('/api/tailscale/services', response_model_exclude_unset=True, response_model_exclude_none=True)
async def fetch_services() -> Union[list[TSService], Err]:
    status_results = []

    jellyfin_run = subprocess.run(['curl', '-f', JELLYFIN_HEALTH_ENDPOINT], capture_output=True, text=True)
    jellyfin_status=(
        TSHealth.DOWN if jellyfin_run.returncode == 22
        else  TSHealth.CHECK_FAILED if jellyfin_run.returncode
        else TSHealth.HEALTHY if jellyfin_run.stdout.strip() == 'Healthy'
        else  TSHealth.DEGRADED
    )
    status_results.append(TSService(
        name='Jellyfin',
        status=jellyfin_status,
        details=(
            f'cURL: {jellyfin_run.returncode}' if status == TSHealth.CHECK_FAILED
            else f'cURL: <{jellyfin_run.stdout.strip()}> <{jellyfin_run.stderr.strip()}>' if status == TSHealth.DOWN
            else None
        )
    ))

    mikochi_run = subprocess.run(['curl', '-f', MIKOCHI_HEALTH_ENDPOINT], capture_output=True, text=True)
    mikochi_status=(
        TSHealth.DOWN if mikochi_run.returncode == 22 else
        TSHealth.CHECK_FAILED if mikochi_run.returncode else
        TSHealth.HEALTHY
    )
    status_results.append(TSService(
        name='Mikochi',
        status=mikochi_status,
        details=(
            f'cURL: {mikochi_run.returncode}' if mikochi_status == TSHealth.CHECK_FAILED else None
        )
    ))

    prosody_run = subprocess.run(['curl', '-f', PROSODY_HEALTH_ENDPOINT], capture_output=True, text=True)
    prosody_status=(
        TSHealth.DOWN if prosody_run.returncode == 22 else
        TSHealth.CHECK_FAILED if prosody_run.returncode else
        TSHealth.HEALTHY if prosody_run.stdout.strip() == 'OK' else
        TSHealth.DEGRADED
    )
    status_results.append(TSService(
        name='Prosody',
        status=prosody_status,
        details=(
            f'cURL: {prosody_run.returncode}' if prosody_status == TSHealth.CHECK_FAILED
            else f'cURL: <{prosody_run.stdout.strip()}> <{prosody_run.stderr.strip()}>' if prosody_status == TSHealth.DOWN
            else None
        )
    ))

    deluge_run = subprocess.run(['curl', '-f', DELUGE_HEALTH_ENDPOINT], capture_output=True, text=True)
    deluge_status=(
        TSHealth.DOWN if deluge_run.returncode == 22
        else TSHealth.CHECK_FAILED if deluge_run.returncode
        else TSHealth.HEALTHY if deluge_run.stdout.strip() == 'OK'
        else TSHealth.DEGRADED
    )
    status_results.append(TSService(
        name='Deluge',
        status=deluge_status,
        details=(
            f'cURL: {deluge_run.returncode}' if deluge_status == TSHealth.CHECK_FAILED
            else None
        )
    ))

    matrix_run = subprocess.run(['curl', '-f', MATRIX_HEALTH_ENDPOINT], capture_output=True, text=True)
    matrix_status=(
        TSHealth.DOWN if matrix_run.returncode == 22
        else TSHealth.CHECK_FAILED if matrix_run.returncode
        else TSHealth.HEALTHY if matrix_run.stdout.strip() == 'OK'
        else TSHealth.DEGRADED
    )
    status_results.append(TSService(
        name='Matrix',
        status=matrix_status,
        details=(
            f'cURL: {matrix_run.returncode}' if matrix_status == TSHealth.CHECK_FAILED
            else f'cURL: <{matrix_run.stdout.strip()}> <{matrix_run.stderr.strip()}>' if matrix_status == TSHealth.DEGRADED
            else None
        )
    ))

    minecraft_run = subprocess.run(['nc', '-vz', MINECRAFT_HEALTH_ENDPOINT, str(MINECRAFT_HEALTH_PORT)], capture_output=True, text=True)
    minecraft_status =(
        TSHealth.DOWN if minecraft_run.returncode == 1
        else TSHealth.HEALTHY if minecraft_run.stderr.strip().endswith('succeeded!')
        else TSHealth.CHECK_FAILED
    )
    status_results.append(TSService(
        name='Minecraft',
        status=minecraft_status,
        details=(
            f'nc: <{minecraft_run.stdout.strip()}> <{minecraft_run.stderr.strip()}>' if minecraft_status == TSHealth.CHECK_FAILED
            else None
        )
    ))



    return status_results

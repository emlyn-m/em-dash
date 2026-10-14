import asyncio
import json
import os
from dataclasses import dataclass
from enum import Enum

import aiohttp
from fastapi import APIRouter, Response, status
from pydantic import BaseModel

router = APIRouter(prefix="/api/tailscale")

CHECK_TIMEOUT = aiohttp.ClientTimeout(total=5)


class Err(BaseModel):
    msg: str


class TSDevice(BaseModel):
    name: str
    alias: str
    ip: str
    online: bool


class TSHealth(str, Enum):
    HEALTHY = "healthy"
    DEGRADED = "degraded"
    DOWN = "down"
    CHECK_FAILED = "check_failed"


class TSService(BaseModel):
    name: str
    status: TSHealth
    details: str | None = None


@dataclass(frozen=True)
class HttpCheck:
    name: str
    url_env: str
    expect_body: str | None = None


@dataclass(frozen=True)
class TcpCheck:
    name: str
    host_env: str
    port_env: str


HTTP_CHECKS = [
    HttpCheck("Jellyfin", "JELLYFIN_HEALTH_ENDPOINT", "Healthy"),
    HttpCheck("Mikochi", "MIKOCHI_HEALTH_ENDPOINT"),
    HttpCheck("Prosody", "PROSODY_HEALTH_ENDPOINT", "OK"),
    HttpCheck("Deluge", "DELUGE_HEALTH_ENDPOINT"),
    HttpCheck("Matrix", "MATRIX_HEALTH_ENDPOINT", "OK"),
]

TCP_CHECKS = [
    TcpCheck("Minecraft", "MINECRAFT_HEALTH_ENDPOINT", "MINECRAFT_HEALTH_PORT")
]


def _device(node: dict, online: bool) -> TSDevice:
    return TSDevice(
        name=node["HostName"],
        alias=node["DNSName"].split(".")[0],
        ip=(node.get("TailscaleIPs") or [""])[0],
        online=online,
    )


@router.get("/devices", response_model_exclude_unset=True)
async def fetch_devices(response: Response) -> list[TSDevice] | Err:
    proc = await asyncio.create_subprocess_exec(
        "tailscale",
        "status",
        "--json",
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
    )
    out, err = await proc.communicate()
    if proc.returncode:
        response.status_code = status.HTTP_500_INTERNAL_SERVER_ERROR
        return Err(msg=err.decode().strip())

    ts = json.loads(out)
    peers = (ts.get("Peer") or {}).values()
    return [_device(ts["Self"], True)] + [
        _device(p, p.get("Online", False)) for p in peers
    ]


async def _check_http(session: aiohttp.ClientSession, chk: HttpCheck) -> TSService:
    url = os.environ.get(chk.url_env)
    if not url:
        return TSService(
            name=chk.name, status=TSHealth.CHECK_FAILED, details=f"{chk.url_env} unset"
        )

    try:
        async with session.get(url) as rsp:
            body = (await rsp.text()).strip()
    except aiohttp.ClientError, TimeoutError:
        return TSService(name=chk.name, status=TSHealth.DOWN)

    healthy = body == chk.expect_body if chk.expect_body is not None else rsp.ok
    if healthy:
        return TSService(name=chk.name, status=TSHealth.HEALTHY)
    return TSService(
        name=chk.name,
        status=TSHealth.DEGRADED,
        details=f"{rsp.status} {body[:100]}".strip(),
    )


async def _check_tcp(chk: TcpCheck) -> TSService:
    host, port = os.environ.get(chk.host_env), os.environ.get(chk.port_env)
    if not host or not port:
        return TSService(
            name=chk.name,
            status=TSHealth.CHECK_FAILED,
            details=f"{chk.host_env}/{chk.port_env} unset",
        )

    try:
        _, writer = await asyncio.wait_for(
            asyncio.open_connection(host, int(port)), CHECK_TIMEOUT.total
        )
    except ConnectionRefusedError, TimeoutError:
        return TSService(name=chk.name, status=TSHealth.DOWN)
    except (OSError, ValueError) as e:
        return TSService(name=chk.name, status=TSHealth.CHECK_FAILED, details=str(e))

    writer.close()
    await writer.wait_closed()
    return TSService(name=chk.name, status=TSHealth.HEALTHY)


@router.get(
    "/services", response_model_exclude_unset=True, response_model_exclude_none=True
)
async def fetch_services() -> list[TSService]:
    async with aiohttp.ClientSession(timeout=CHECK_TIMEOUT) as session:
        return await asyncio.gather(
            *(_check_http(session, chk) for chk in HTTP_CHECKS),
            *(_check_tcp(chk) for chk in TCP_CHECKS),
        )

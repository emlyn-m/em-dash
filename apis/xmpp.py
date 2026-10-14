import json
import logging
import os
from contextlib import asynccontextmanager

import fastapi
import slixmpp
from pydantic import BaseModel, ValidationError

log = logging.getLogger("uvicorn.error.xmpp")

router = fastapi.APIRouter()

ARCHIVE_FETCH_LIMIT = 200


class Message(BaseModel, frozen=True):
    msg_timestamp: int
    msg_sev: int
    msg_class: str
    msg_body: str


class Listener(slixmpp.ClientXMPP):
    def __init__(self, jid: str, password: str):
        super().__init__(jid, password)
        self.messages: set[Message] = set()

        self["feature_mechanisms"].unencrypted_plain = True
        self["feature_mechanisms"].unencrypted_digest = True
        self["feature_mechanisms"].unencrypted_scram = True
        for plugin in ("xep_0030", "xep_0059", "xep_0199", "xep_0297", "xep_0313"):
            self.register_plugin(plugin)

        self.add_event_handler("session_start", self.on_start)
        self.add_event_handler("message", lambda msg: self.ingest(msg["body"]))

    async def on_start(self, _event):
        self.send_presence()
        await self.get_roster()

        try:
            n = 0
            async for result in self["xep_0313"].iterate(
                reverse=True, total=ARCHIVE_FETCH_LIMIT
            ):
                self.ingest(result["mam_result"]["forwarded"]["stanza"]["body"])
                n += 1
            log.info("fetched %d archived msgs", n)
        except Exception:
            log.exception("archive fetch failed")

    def ingest(self, body: str):
        try:
            raw = json.loads(body)
            self.messages.add(
                Message(
                    msg_timestamp=raw["timestamp"],
                    msg_sev=raw["sev"],
                    msg_class=raw["class"],
                    msg_body=raw["body"],
                )
            )
        except json.JSONDecodeError, TypeError, KeyError, ValidationError:
            log.warning("skipping malformed msg: %r", body)


@asynccontextmanager
async def lifespan(app: fastapi.FastAPI):
    listener = Listener(os.environ["CLIENT_JID"], os.environ["CLIENT_PWD"])
    listener.connect()
    app.state.xmpp = listener
    yield
    await listener.disconnect()


@router.get("/api/xmpp")
async def get_msgs(
    request: fastapi.Request,
    max_events: int | None = None,
    before: int | None = None,
    after: int | None = None,
    msg_class: str | None = None,
    max_sev: int | None = None,
) -> list[Message]:
    events = [
        m
        for m in request.app.state.xmpp.messages
        if (before is None or m.msg_timestamp <= before)
        and (after is None or m.msg_timestamp >= after)
        and (msg_class is None or m.msg_class == msg_class)
        and (max_sev is None or m.msg_sev <= max_sev)
    ]
    events.sort(key=lambda m: m.msg_timestamp, reverse=True)
    return events[: max_events or None]

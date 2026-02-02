from fastapi import FastAPI
import tailscale
import xmpp

app = FastAPI()
app.include_router(tailscale.router)
app.include_router(xmpp.router)

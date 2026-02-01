from fastapi import FastAPI
import tailscale

app = FastAPI()
app.include_router(tailscale.router)

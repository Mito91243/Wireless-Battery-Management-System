import os
from datetime import datetime

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from app.models.database import engine, Base
from app.routes.auth import router as auth_router
from app.routes.packs import router as packs_router

# Import models so Base knows about them
from app.models import models  # noqa: F401

app = FastAPI(title="WBMS Backend", version="1.0.0")


@app.on_event("startup")
def on_startup():
    try:
        Base.metadata.create_all(bind=engine)
    except Exception as e:
        print(f"Warning: Could not create tables on startup: {e}")


# CORS — allow the configured frontend origin plus local dev hosts.
_frontend_origin = os.getenv("FRONTEND_ORIGIN", "http://localhost:5173")
_allowed_origins = {
    _frontend_origin,
    "http://localhost:5173",
    "http://127.0.0.1:5173",
}

app.add_middleware(
    CORSMiddleware,
    allow_origins=sorted(_allowed_origins),
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Include routers
app.include_router(auth_router)
app.include_router(packs_router)


@app.get("/")
async def root():
    return {"message": "WBMS API"}


@app.get("/health")
async def health_check():
    return {
        "status": "Healthy",
        "timestamp": datetime.now().isoformat(),
    }

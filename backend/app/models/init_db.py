from .database import engine, Base
from . import models  # noqa: F401 — import so models register with Base

def init_database():
    """Create all tables that don't already exist."""
    Base.metadata.create_all(bind=engine)
    print("Database tables created successfully!")

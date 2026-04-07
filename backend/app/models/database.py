import os
from pathlib import Path

from sqlalchemy import create_engine
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import sessionmaker

# Use SQLite by default (no server needed), or set WBMS_DATABASE_URL for MySQL
# MySQL example: mysql+pymysql://root:@localhost:8889/wbms
DEFAULT_DB_PATH = Path(__file__).resolve().parent.parent.parent / "wbms.db"
SQLALCHEMY_DATABASE_URL = os.getenv(
    "WBMS_DATABASE_URL",
    f"sqlite:///{DEFAULT_DB_PATH}",
)

connect_args = {}
if SQLALCHEMY_DATABASE_URL.startswith("sqlite"):
    connect_args["check_same_thread"] = False

engine = create_engine(
    SQLALCHEMY_DATABASE_URL,
    pool_pre_ping=True,
    connect_args=connect_args,
)

SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)

Base = declarative_base()


def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()

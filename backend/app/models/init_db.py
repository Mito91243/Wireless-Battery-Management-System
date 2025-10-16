# init_db.py
from database import engine, Base
import models

def init_database():
    """Initialize database tables - run this once"""
    Base.metadata.create_all(bind=engine)
    print("✅ Database tables created successfully!")

if __name__ == "__main__":
    init_database()
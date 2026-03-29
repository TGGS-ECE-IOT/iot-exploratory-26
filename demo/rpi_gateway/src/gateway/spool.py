from __future__ import annotations

import json
import sqlite3
from pathlib import Path

from gateway.models import CanonicalEvent


class SpoolStore:
    def __init__(self, db_path: str) -> None:
        path = Path(db_path)
        path.parent.mkdir(parents=True, exist_ok=True)
        self._conn = sqlite3.connect(path)
        self._conn.execute(
            """
            CREATE TABLE IF NOT EXISTS cloud_spool (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                event_json TEXT NOT NULL,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP
            )
            """
        )
        self._conn.commit()

    def enqueue(self, event: CanonicalEvent) -> None:
        self._conn.execute("INSERT INTO cloud_spool(event_json) VALUES (?)", (event.model_dump_json(),))
        self._conn.commit()

    def dequeue_batch(self, limit: int) -> list[tuple[int, CanonicalEvent]]:
        cur = self._conn.execute("SELECT id, event_json FROM cloud_spool ORDER BY id LIMIT ?", (limit,))
        rows = cur.fetchall()
        out: list[tuple[int, CanonicalEvent]] = []
        for spool_id, event_json in rows:
            out.append((spool_id, CanonicalEvent.model_validate(json.loads(event_json))))
        return out

    def delete_ids(self, ids: list[int]) -> None:
        if not ids:
            return
        placeholders = ",".join("?" for _ in ids)
        self._conn.execute(f"DELETE FROM cloud_spool WHERE id IN ({placeholders})", ids)
        self._conn.commit()

    def count(self) -> int:
        cur = self._conn.execute("SELECT COUNT(*) FROM cloud_spool")
        return int(cur.fetchone()[0])

    def close(self) -> None:
        self._conn.close()

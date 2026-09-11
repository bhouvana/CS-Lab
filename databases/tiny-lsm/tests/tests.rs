use tiny_lsm::Lsm;

// Each test gets its own scratch directory under the OS temp dir,
// named after the test, cleaned up at the end.
fn scratch_dir(name: &str) -> std::path::PathBuf {
    let dir = std::env::temp_dir().join(format!("tiny-lsm-test-{name}-{}", std::process::id()));
    let _ = std::fs::remove_dir_all(&dir);
    dir
}

#[test]
fn put_then_get_normal_case() {
    let dir = scratch_dir("put_get");
    let mut lsm = Lsm::open(&dir).unwrap();
    lsm.put("hello", "world").unwrap();
    assert_eq!(lsm.get("hello").unwrap(), Some("world".to_string()));
    std::fs::remove_dir_all(&dir).ok();
}

#[test]
fn get_missing_key_returns_none_edge_case() {
    let dir = scratch_dir("missing");
    let lsm = Lsm::open(&dir).unwrap();
    assert_eq!(lsm.get("nope").unwrap(), None);
    std::fs::remove_dir_all(&dir).ok();
}

#[test]
fn overwrite_returns_latest_value_normal_case() {
    let dir = scratch_dir("overwrite");
    let mut lsm = Lsm::open(&dir).unwrap();
    lsm.put("k", "v1").unwrap();
    lsm.put("k", "v2").unwrap();
    lsm.put("k", "v3").unwrap();
    assert_eq!(lsm.get("k").unwrap(), Some("v3".to_string()));
    std::fs::remove_dir_all(&dir).ok();
}

#[test]
fn memtable_limit_triggers_automatic_flush() {
    let dir = scratch_dir("autoflush");
    let mut lsm = Lsm::open(&dir).unwrap();
    lsm.memtable_limit = 3;
    lsm.put("a", "1").unwrap();
    lsm.put("b", "2").unwrap();
    assert_eq!(lsm.sstable_count(), 0);
    lsm.put("c", "3").unwrap(); // hits the limit -> auto flush
    assert_eq!(lsm.sstable_count(), 1);
    assert_eq!(lsm.memtable_len(), 0);
    std::fs::remove_dir_all(&dir).ok();
}

#[test]
fn get_finds_key_in_a_flushed_sstable() {
    let dir = scratch_dir("flushed_get");
    let mut lsm = Lsm::open(&dir).unwrap();
    lsm.memtable_limit = 2;
    lsm.put("a", "1").unwrap();
    lsm.put("b", "2").unwrap(); // triggers flush
    assert_eq!(lsm.sstable_count(), 1);
    assert_eq!(lsm.get("a").unwrap(), Some("1".to_string()));
    std::fs::remove_dir_all(&dir).ok();
}

#[test]
fn newer_sstable_wins_over_older_one_for_the_same_key() {
    let dir = scratch_dir("newer_wins");
    let mut lsm = Lsm::open(&dir).unwrap();
    lsm.memtable_limit = 1;
    lsm.put("k", "old").unwrap(); // flushes immediately (limit=1) -> sstable 0
    lsm.put("k", "new").unwrap(); // flushes immediately -> sstable 1, newer
    assert_eq!(lsm.sstable_count(), 2);
    assert_eq!(lsm.get("k").unwrap(), Some("new".to_string()));
    std::fs::remove_dir_all(&dir).ok();
}

#[test]
fn wal_replay_recovers_unflushed_writes_after_a_crash() {
    // Simulates a crash: writes go through put() (durably WAL-logged)
    // but the process ends (Lsm dropped) before a flush happens.
    // Reopening from the same directory must recover them via WAL replay.
    let dir = scratch_dir("wal_recovery");
    {
        let mut lsm = Lsm::open(&dir).unwrap();
        lsm.memtable_limit = 1000; // won't auto-flush
        lsm.put("survivor", "yes").unwrap();
    } // Lsm dropped here -- no explicit flush, simulating a crash

    let lsm2 = Lsm::open(&dir).unwrap();
    assert_eq!(lsm2.get("survivor").unwrap(), Some("yes".to_string()));
    std::fs::remove_dir_all(&dir).ok();
}

#[test]
fn compact_merges_sstables_and_keeps_latest_value() {
    let dir = scratch_dir("compact");
    let mut lsm = Lsm::open(&dir).unwrap();
    lsm.memtable_limit = 1;
    lsm.put("k", "v1").unwrap(); // sstable 0
    lsm.put("k", "v2").unwrap(); // sstable 1 (newer, same key)
    lsm.put("other", "x").unwrap(); // sstable 2
    assert_eq!(lsm.sstable_count(), 3);

    let (before, after) = lsm.compact().unwrap();
    assert_eq!(before, 3);
    assert_eq!(after, 2); // "k" and "other" -- duplicates collapsed
    assert_eq!(lsm.sstable_count(), 1);
    assert_eq!(lsm.get("k").unwrap(), Some("v2".to_string())); // latest value survived
    assert_eq!(lsm.get("other").unwrap(), Some("x".to_string()));
    std::fs::remove_dir_all(&dir).ok();
}

#[test]
fn compact_on_empty_store_is_a_safe_noop_invalid_case() {
    let dir = scratch_dir("compact_empty");
    let mut lsm = Lsm::open(&dir).unwrap();
    let (before, after) = lsm.compact().unwrap();
    assert_eq!((before, after), (0, 0));
    std::fs::remove_dir_all(&dir).ok();
}

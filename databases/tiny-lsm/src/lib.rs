//! A miniature log-structured merge (LSM) storage engine.
//!
//! ```text
//!   write
//!     |
//!     v
//!    WAL          durability: fsync'd before put() returns
//!     |
//!     v
//!  memtable       an in-memory sorted map (BTreeMap)
//!     |
//!     v  (once full)
//!   flush
//!     |
//!     v
//!  sorted run     an on-disk SSTable, sorted by key
//!     |
//!     v  (compact())
//!   merge
//! ```
//!
//! Keys and values are both `String` for simplicity — a real engine
//! would work over arbitrary bytes. No SQL, no transactions, no query
//! engine: just `put`/`get`, on purpose (see CS-LAB.md §19).

use std::collections::BTreeMap;
use std::fs::{self, File, OpenOptions};
use std::io::{self, BufReader, BufWriter, Read, Write};
use std::path::{Path, PathBuf};

fn write_entry<W: Write>(w: &mut W, key: &str, value: &str) -> io::Result<()> {
    let (kb, vb) = (key.as_bytes(), value.as_bytes());
    w.write_all(&(kb.len() as u32).to_le_bytes())?;
    w.write_all(kb)?;
    w.write_all(&(vb.len() as u32).to_le_bytes())?;
    w.write_all(vb)
}

fn read_entry<R: Read>(r: &mut R) -> io::Result<Option<(String, String)>> {
    let mut len_buf = [0u8; 4];
    match r.read_exact(&mut len_buf) {
        Ok(()) => {}
        Err(e) if e.kind() == io::ErrorKind::UnexpectedEof => return Ok(None),
        Err(e) => return Err(e),
    }
    let mut kbuf = vec![0u8; u32::from_le_bytes(len_buf) as usize];
    r.read_exact(&mut kbuf)?;
    r.read_exact(&mut len_buf)?;
    let mut vbuf = vec![0u8; u32::from_le_bytes(len_buf) as usize];
    r.read_exact(&mut vbuf)?;
    Ok(Some((String::from_utf8_lossy(&kbuf).into_owned(), String::from_utf8_lossy(&vbuf).into_owned())))
}

fn read_all_entries(path: &Path) -> io::Result<Vec<(String, String)>> {
    let mut r = BufReader::new(File::open(path)?);
    let mut out = Vec::new();
    while let Some(entry) = read_entry(&mut r)? {
        out.push(entry);
    }
    Ok(out)
}

pub struct Lsm {
    dir: PathBuf,
    memtable: BTreeMap<String, String>,
    wal: File,
    sstable_paths: Vec<PathBuf>, // oldest first
    pub memtable_limit: usize,   // entries before an automatic flush
}

impl Lsm {
    pub fn open<P: AsRef<Path>>(dir: P) -> io::Result<Self> {
        let dir = dir.as_ref().to_path_buf();
        fs::create_dir_all(&dir)?;
        let wal_path = dir.join("wal.log");

        // Crash recovery: replay any WAL entries that were durably
        // written but never made it into a flushed SSTable.
        let mut memtable = BTreeMap::new();
        if wal_path.exists() {
            for (k, v) in read_all_entries(&wal_path)? {
                memtable.insert(k, v);
            }
        }
        let wal = OpenOptions::new().create(true).append(true).open(&wal_path)?;

        let mut sstable_paths: Vec<PathBuf> = fs::read_dir(&dir)?
            .filter_map(|e| e.ok())
            .map(|e| e.path())
            .filter(|p| {
                p.file_name()
                    .and_then(|n| n.to_str())
                    .is_some_and(|n| n.starts_with("sstable_") && n.ends_with(".dat"))
            })
            .collect();
        sstable_paths.sort(); // filenames are zero-padded, so lexical order == creation order

        Ok(Lsm { dir, memtable, wal, sstable_paths, memtable_limit: 4 })
    }

    /// Appends to the WAL (fsync'd before returning -- this is what
    /// "durable" means here) THEN updates the memtable. If the
    /// memtable now has `memtable_limit` entries, flushes it.
    pub fn put(&mut self, key: &str, value: &str) -> io::Result<()> {
        write_entry(&mut self.wal, key, value)?;
        self.wal.flush()?;
        self.wal.sync_all()?;
        self.memtable.insert(key.to_string(), value.to_string());
        if self.memtable.len() >= self.memtable_limit {
            self.flush()?;
        }
        Ok(())
    }

    /// Checks the memtable first (most recent data), then SSTables
    /// from newest to oldest (a key can appear in several; the newest
    /// table's value is the current one).
    pub fn get(&self, key: &str) -> io::Result<Option<String>> {
        if let Some(v) = self.memtable.get(key) {
            return Ok(Some(v.clone()));
        }
        for path in self.sstable_paths.iter().rev() {
            for (k, v) in read_all_entries(path)? {
                if k == key {
                    return Ok(Some(v));
                }
            }
        }
        Ok(None)
    }

    /// Writes the memtable out as a new sorted SSTable, clears it, and
    /// truncates the WAL (its job was only to protect unflushed data).
    pub fn flush(&mut self) -> io::Result<()> {
        if self.memtable.is_empty() {
            return Ok(());
        }
        let path = self.dir.join(format!("sstable_{:05}.dat", self.sstable_paths.len()));
        let mut w = BufWriter::new(File::create(&path)?);
        for (k, v) in &self.memtable {
            write_entry(&mut w, k, v)?;
        }
        w.flush()?;
        self.sstable_paths.push(path);
        self.memtable.clear();

        self.wal = OpenOptions::new().create(true).write(true).truncate(true).open(self.dir.join("wal.log"))?;
        Ok(())
    }

    /// Merges every SSTable into a single new one, keeping only the
    /// most recent value per key (later tables overwrite earlier ones
    /// for the same key, since they're merged oldest-to-newest into a
    /// map). Returns (tables_before, keys_after).
    pub fn compact(&mut self) -> io::Result<(usize, usize)> {
        let tables_before = self.sstable_paths.len();
        let mut merged: BTreeMap<String, String> = BTreeMap::new();
        for path in &self.sstable_paths {
            for (k, v) in read_all_entries(path)? {
                merged.insert(k, v); // newer tables are visited later, so they win
            }
        }
        for path in &self.sstable_paths {
            fs::remove_file(path)?;
        }
        let keys_after = merged.len();
        let new_path = self.dir.join("sstable_00000.dat");
        let mut w = BufWriter::new(File::create(&new_path)?);
        for (k, v) in &merged {
            write_entry(&mut w, k, v)?;
        }
        w.flush()?;
        self.sstable_paths = vec![new_path];
        Ok((tables_before, keys_after))
    }

    pub fn memtable_len(&self) -> usize {
        self.memtable.len()
    }
    pub fn sstable_count(&self) -> usize {
        self.sstable_paths.len()
    }
}

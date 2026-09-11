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
use std::io::{self, BufReader, BufWriter, Read, Seek, SeekFrom, Write};
use std::path::{Path, PathBuf};

const SSTABLE_INDEX_STRIDE: usize = 100;

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
    Ok(Some((
        String::from_utf8_lossy(&kbuf).into_owned(),
        String::from_utf8_lossy(&vbuf).into_owned(),
    )))
}

fn read_all_entries(path: &Path) -> io::Result<Vec<(String, String)>> {
    let mut r = BufReader::new(File::open(path)?);
    let mut out = Vec::new();
    while let Some(entry) = read_entry(&mut r)? {
        out.push(entry);
    }
    Ok(out)
}

struct BloomFilter {
    bits: Vec<u64>,
    num_bits: usize,
    num_hashes: usize,
}

impl BloomFilter {
    fn for_items(item_count: usize) -> Self {
        let num_bits = (item_count.max(1) * 10).max(64);
        BloomFilter {
            bits: vec![0; num_bits.div_ceil(64)],
            num_bits,
            num_hashes: 7,
        }
    }

    fn insert(&mut self, item: &str) {
        for index in self.indices(item) {
            self.bits[index / 64] |= 1 << (index % 64);
        }
    }

    fn contains(&self, item: &str) -> bool {
        self.indices(item)
            .iter()
            .all(|&index| (self.bits[index / 64] >> (index % 64)) & 1 == 1)
    }

    fn indices(&self, item: &str) -> Vec<usize> {
        let first = hash64(item.as_bytes(), 0xcbf29ce484222325);
        let second = hash64(item.as_bytes(), 0x9e3779b97f4a7c15);
        (0..self.num_hashes)
            .map(|i| {
                first
                    .wrapping_add((i as u64).wrapping_mul(second))
                    .wrapping_rem(self.num_bits as u64) as usize
            })
            .collect()
    }
}

fn hash64(data: &[u8], mut hash: u64) -> u64 {
    for &byte in data {
        hash ^= byte as u64;
        hash = hash.wrapping_mul(0x100000001b3);
    }
    hash = (hash ^ (hash >> 30)).wrapping_mul(0xbf58476d1ce4e5b9);
    hash = (hash ^ (hash >> 27)).wrapping_mul(0x94d049bb133111eb);
    hash ^ (hash >> 31)
}

struct Sstable {
    path: PathBuf,
    bloom: BloomFilter,
    sparse_index: Vec<(String, u64)>,
}

fn build_sstable_metadata(path: &Path) -> io::Result<Sstable> {
    let mut reader = BufReader::new(File::open(path)?);
    let mut keys = Vec::new();
    let mut sparse_index = Vec::new();
    loop {
        let offset = reader.stream_position()?;
        let Some((key, _)) = read_entry(&mut reader)? else {
            break;
        };
        if keys.len() % SSTABLE_INDEX_STRIDE == 0 {
            sparse_index.push((key.clone(), offset));
        }
        keys.push(key);
    }
    let mut bloom = BloomFilter::for_items(keys.len());
    for key in keys {
        bloom.insert(&key);
    }
    Ok(Sstable {
        path: path.to_path_buf(),
        bloom,
        sparse_index,
    })
}

pub struct Lsm {
    dir: PathBuf,
    memtable: BTreeMap<String, String>,
    wal: File,
    sstables: Vec<Sstable>,    // oldest first
    pub memtable_limit: usize, // entries before an automatic flush
    pub auto_compact_threshold: Option<usize>,
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

        let sstables = sstable_paths
            .iter()
            .map(|path| build_sstable_metadata(path))
            .collect::<io::Result<Vec<_>>>()?;

        Ok(Lsm {
            dir,
            memtable,
            wal,
            sstables,
            memtable_limit: 4,
            auto_compact_threshold: None,
        })
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
            if self
                .auto_compact_threshold
                .is_some_and(|threshold| threshold > 0 && self.sstables.len() > threshold)
            {
                self.compact()?;
            }
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
        self.get_from_sstables(key, true, true)
    }

    fn get_from_sstables(
        &self,
        key: &str,
        use_bloom_filter: bool,
        use_sparse_index: bool,
    ) -> io::Result<Option<String>> {
        for table in self.sstables.iter().rev() {
            if use_bloom_filter && !table.bloom.contains(key) {
                continue;
            }
            let mut reader = BufReader::new(File::open(&table.path)?);
            let mut offset = 0;
            if use_sparse_index {
                for (indexed_key, indexed_offset) in &table.sparse_index {
                    if indexed_key.as_str() <= key {
                        offset = *indexed_offset;
                    } else {
                        break;
                    }
                }
            }
            reader.seek(SeekFrom::Start(offset))?;
            while let Some((k, v)) = read_entry(&mut reader)? {
                if k == key {
                    return Ok(Some(v));
                }
                if k.as_str() > key {
                    break;
                }
            }
        }
        Ok(None)
    }

    /// Reference scan used by the benchmark to isolate filter/index gains.
    pub fn get_linear(&self, key: &str) -> io::Result<Option<String>> {
        if let Some(v) = self.memtable.get(key) {
            return Ok(Some(v.clone()));
        }
        self.get_from_sstables(key, false, false)
    }

    /// Reference path with the sparse index but without the Bloom filter.
    pub fn get_sparse(&self, key: &str) -> io::Result<Option<String>> {
        if let Some(v) = self.memtable.get(key) {
            return Ok(Some(v.clone()));
        }
        self.get_from_sstables(key, false, true)
    }

    /// Writes the memtable out as a new sorted SSTable, clears it, and
    /// truncates the WAL (its job was only to protect unflushed data).
    pub fn flush(&mut self) -> io::Result<()> {
        if self.memtable.is_empty() {
            return Ok(());
        }
        let path = self.dir.join(format!("sstable_{:05}.dat", self.sstables.len()));
        let mut w = BufWriter::new(File::create(&path)?);
        for (k, v) in &self.memtable {
            write_entry(&mut w, k, v)?;
        }
        w.flush()?;
        self.sstables.push(build_sstable_metadata(&path)?);
        self.memtable.clear();

        self.wal = OpenOptions::new()
            .create(true)
            .write(true)
            .truncate(true)
            .open(self.dir.join("wal.log"))?;
        Ok(())
    }

    /// Merges every SSTable into a single new one, keeping only the
    /// most recent value per key (later tables overwrite earlier ones
    /// for the same key, since they're merged oldest-to-newest into a
    /// map). Returns (tables_before, keys_after).
    pub fn compact(&mut self) -> io::Result<(usize, usize)> {
        let tables_before = self.sstables.len();
        let mut merged: BTreeMap<String, String> = BTreeMap::new();
        for table in &self.sstables {
            for (k, v) in read_all_entries(&table.path)? {
                merged.insert(k, v); // newer tables are visited later, so they win
            }
        }
        for table in &self.sstables {
            fs::remove_file(&table.path)?;
        }
        let keys_after = merged.len();
        let new_path = self.dir.join("sstable_00000.dat");
        let mut w = BufWriter::new(File::create(&new_path)?);
        for (k, v) in &merged {
            write_entry(&mut w, k, v)?;
        }
        w.flush()?;
        self.sstables = vec![build_sstable_metadata(&new_path)?];
        Ok((tables_before, keys_after))
    }

    pub fn memtable_len(&self) -> usize {
        self.memtable.len()
    }
    pub fn sstable_count(&self) -> usize {
        self.sstables.len()
    }
}

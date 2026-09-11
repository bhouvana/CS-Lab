//! A deterministic Raft consensus simulator — no real network
//! transport (CS-LAB.md §21): "sending a message" is a direct
//! function call that returns its reply immediately, in the same
//! tick. That's what "simulated messages" means here: every Raft rule
//! (term comparisons, vote granting, log matching, commit safety) is
//! implemented for real, just without sockets, threads, or delay.
//!
//! ```text
//! nodes -> tick() -> election timeouts / heartbeats -> Raft state machine -> leader/log state
//! ```

use std::collections::HashMap;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Role {
    Follower,
    Candidate,
    Leader,
}

#[derive(Debug, Clone)]
pub struct LogEntry {
    pub term: u64,
    pub command: String,
}

pub struct Node {
    pub id: usize,
    pub role: Role,
    pub current_term: u64,
    pub voted_for: Option<usize>,
    pub log: Vec<LogEntry>,
    pub commit_index: usize,
    pub snapshot_index: usize,
    pub snapshot_term: u64,
    election_deadline: u64,
    election_timeout: u64,
    next_index: HashMap<usize, usize>,
    match_index: HashMap<usize, usize>,
}

impl Node {
    fn new(id: usize, election_timeout: u64) -> Self {
        Node {
            id,
            role: Role::Follower,
            current_term: 0,
            voted_for: None,
            log: Vec::new(),
            commit_index: 0,
            snapshot_index: 0,
            snapshot_term: 0,
            election_deadline: election_timeout,
            election_timeout,
            next_index: HashMap::new(),
            match_index: HashMap::new(),
        }
    }

    fn last_log_info(&self) -> (usize, u64) {
        match self.log.last() {
            Some(e) => (self.snapshot_index + self.log.len(), e.term),
            None => (self.snapshot_index, self.snapshot_term),
        }
    }

    fn log_term(&self, index: usize) -> Option<u64> {
        if index == self.snapshot_index {
            Some(self.snapshot_term)
        } else if index > self.snapshot_index {
            self.log.get(index - self.snapshot_index - 1).map(|e| e.term)
        } else {
            None
        }
    }
}

pub struct Cluster {
    pub nodes: Vec<Node>,
    pub alive: Vec<bool>,
    pub tick_count: u64,
    pub events: Vec<String>,
    pub timeout_collisions: usize,
    pub split_vote_count: usize,
    pub links: Vec<Vec<bool>>,
    election_timeout_base: u64,
    heartbeat_interval: u64,
    random_state: Option<u64>,
}

impl Cluster {
    /// `n` nodes, IDs 0..n. Election timeouts are deterministically
    /// staggered (`base + id * 10`) rather than randomized, so the
    /// same cluster size always produces the same sequence of events
    /// — reproducibility over strict Raft-paper realism (which
    /// randomizes timeouts to avoid split votes); see README.
    pub fn new(n: usize) -> Self {
        Self::build(n, None)
    }

    /// Creates a cluster with seeded, pseudo-random election timeouts.
    pub fn with_seed(n: usize, seed: u64) -> Self {
        Self::build(n, Some(seed))
    }

    fn build(n: usize, random_state: Option<u64>) -> Self {
        let election_timeout_base = 150;
        let mut state = random_state;
        let nodes = (0..n)
            .map(|id| {
                let timeout = match &mut state {
                    Some(seed) => {
                        *seed = seed.wrapping_mul(6364136223846793005).wrapping_add(1);
                        election_timeout_base + (*seed % 51)
                    }
                    None => election_timeout_base + (id as u64) * 10,
                };
                Node::new(id, timeout)
            })
            .collect();
        Cluster {
            nodes,
            alive: vec![true; n],
            tick_count: 0,
            events: Vec::new(),
            timeout_collisions: 0,
            split_vote_count: 0,
            links: vec![vec![true; n]; n],
            election_timeout_base,
            heartbeat_interval: 20,
            random_state: state,
        }
    }

    /// Blocks messages in both directions between two groups of nodes.
    pub fn partition(&mut self, left: &[usize], right: &[usize]) {
        for &a in left {
            for &b in right {
                self.links[a][b] = false;
                self.links[b][a] = false;
            }
        }
    }

    /// Restores all simulated network links.
    pub fn heal_partition(&mut self) {
        for row in &mut self.links {
            for link in row {
                *link = true;
            }
        }
    }

    /// Returns the number of retained log entries, excluding compacted data.
    pub fn log_storage_len(&self) -> usize {
        self.nodes.iter().map(|node| node.log.len()).sum()
    }

    /// Compacts each node through the requested committed index.
    pub fn compact(&mut self, index: usize) {
        for node in &mut self.nodes {
            let target = index.min(node.commit_index).min(node.last_log_info().0);
            if target <= node.snapshot_index {
                continue;
            }
            node.snapshot_term = node.log_term(target).expect("committed log index exists");
            let remove = target - node.snapshot_index;
            node.log.drain(..remove);
            node.snapshot_index = target;
        }
    }

    pub fn kill(&mut self, id: usize) {
        self.alive[id] = false;
        self.events
            .push(format!("tick {}: node {} killed", self.tick_count, id));
    }

    pub fn revive(&mut self, id: usize) {
        self.alive[id] = true;
        self.nodes[id].role = Role::Follower;
        self.nodes[id].voted_for = None;
        self.reset_deadline(id);
        self.events.push(format!(
            "tick {}: node {} revived (rejoins as follower)",
            self.tick_count, id
        ));
    }

    pub fn leader(&self) -> Option<usize> {
        self.nodes
            .iter()
            .enumerate()
            .find(|(i, n)| self.alive[*i] && n.role == Role::Leader)
            .map(|(i, _)| i)
    }

    /// Submits a command to the current leader's log. Returns false if
    /// there is no leader right now.
    pub fn submit(&mut self, command: &str) -> bool {
        let Some(leader) = self.leader() else { return false };
        let term = self.nodes[leader].current_term;
        self.nodes[leader].log.push(LogEntry {
            term,
            command: command.to_string(),
        });
        let log_index = self.nodes[leader].last_log_info().0;
        self.events.push(format!(
            "tick {}: leader {} appends '{}' at log index {}",
            self.tick_count, leader, command, log_index
        ));
        true
    }

    pub fn tick(&mut self) {
        self.tick_count += 1;

        let leaders: Vec<usize> = (0..self.nodes.len())
            .filter(|&i| self.alive[i] && self.nodes[i].role == Role::Leader)
            .collect();
        for leader in leaders {
            if self.tick_count.is_multiple_of(self.heartbeat_interval) {
                self.send_heartbeats(leader);
            }
        }

        let timed_out: Vec<usize> = (0..self.nodes.len())
            .filter(|&i| {
                self.alive[i]
                    && self.nodes[i].role != Role::Leader
                    && self.tick_count >= self.nodes[i].election_deadline
            })
            .collect();
        if timed_out.len() > 1 {
            self.timeout_collisions += 1;
        }
        for id in timed_out {
            if self.alive[id] && self.nodes[id].role != Role::Leader {
                self.start_election(id);
            }
        }
    }

    /// Runs `ticks` steps in a row.
    pub fn run(&mut self, ticks: u64) {
        for _ in 0..ticks {
            self.tick();
        }
    }

    fn reset_deadline(&mut self, id: usize) {
        let timeout = match &mut self.random_state {
            Some(seed) => {
                *seed = seed.wrapping_mul(6364136223846793005).wrapping_add(1);
                self.election_timeout_base + (*seed % 51)
            }
            None => self.election_timeout_base + (id as u64) * 10,
        };
        self.nodes[id].election_timeout = timeout;
        self.nodes[id].election_deadline = self.tick_count + timeout;
    }

    fn become_follower(&mut self, id: usize, term: u64) {
        self.nodes[id].current_term = term;
        self.nodes[id].role = Role::Follower;
        self.nodes[id].voted_for = None;
        self.reset_deadline(id);
    }

    fn start_election(&mut self, id: usize) {
        let n = self.nodes.len();
        self.nodes[id].current_term += 1;
        self.nodes[id].role = Role::Candidate;
        self.nodes[id].voted_for = Some(id);
        self.reset_deadline(id);
        let term = self.nodes[id].current_term;
        self.events.push(format!(
            "tick {}: node {} times out, starts election for term {}",
            self.tick_count, id, term
        ));

        let (last_log_index, last_log_term) = self.nodes[id].last_log_info();
        let mut votes = 1usize; // votes for self

        for other in 0..n {
            if other == id || !self.alive[other] || !self.links[id][other] {
                continue;
            }
            let (granted, reply_term) = self.handle_request_vote(other, term, id, last_log_index, last_log_term);
            if reply_term > self.nodes[id].current_term {
                self.become_follower(id, reply_term);
                return;
            }
            if granted {
                votes += 1;
            }
            if self.nodes[id].role != Role::Candidate {
                return; // may have stepped down mid-election (e.g. saw a higher term)
            }
        }

        if votes * 2 > n {
            self.become_leader(id);
        } else {
            self.split_vote_count += 1;
            self.events.push(format!(
                "tick {}: node {} lost the election for term {} ({votes}/{n} votes)",
                self.tick_count, id, term
            ));
        }
    }

    fn handle_request_vote(
        &mut self,
        voter: usize,
        term: u64,
        candidate_id: usize,
        last_log_index: usize,
        last_log_term: u64,
    ) -> (bool, u64) {
        if term > self.nodes[voter].current_term {
            self.become_follower(voter, term);
        }
        let mut granted = false;
        if term == self.nodes[voter].current_term {
            let can_vote = self.nodes[voter].voted_for.is_none() || self.nodes[voter].voted_for == Some(candidate_id);
            let (my_last_index, my_last_term) = self.nodes[voter].last_log_info();
            // Raft's "at least as up to date" rule: higher term wins;
            // equal term, longer (or equal) log wins.
            let log_ok =
                last_log_term > my_last_term || (last_log_term == my_last_term && last_log_index >= my_last_index);
            if can_vote && log_ok {
                self.nodes[voter].voted_for = Some(candidate_id);
                self.reset_deadline(voter); // granting a vote is "hearing from a leader-ish peer": don't also time out
                granted = true;
            }
        }
        (granted, self.nodes[voter].current_term)
    }

    fn become_leader(&mut self, id: usize) {
        self.nodes[id].role = Role::Leader;
        let term = self.nodes[id].current_term;
        self.events.push(format!(
            "tick {}: node {} becomes LEADER for term {}",
            self.tick_count, id, term
        ));
        let log_len = self.nodes[id].last_log_info().0;
        for other in 0..self.nodes.len() {
            self.nodes[id].next_index.insert(other, log_len);
            self.nodes[id].match_index.insert(other, 0);
        }
        self.send_heartbeats(id);
    }

    fn send_heartbeats(&mut self, leader: usize) {
        for other in 0..self.nodes.len() {
            if other == leader || !self.alive[other] || !self.links[leader][other] {
                continue;
            }
            self.replicate_to(leader, other);
        }
        self.advance_commit_index(leader);
    }

    fn replicate_to(&mut self, leader: usize, follower: usize) {
        let next = *self.nodes[leader].next_index.get(&follower).unwrap_or(&0);
        let prev_log_index = next;
        let prev_log_term = self.nodes[leader].log_term(prev_log_index).unwrap_or(0);
        let entries_start = next.saturating_sub(self.nodes[leader].snapshot_index);
        let entries: Vec<LogEntry> = self.nodes[leader].log[entries_start..].to_vec();
        let term = self.nodes[leader].current_term;
        let leader_commit = self.nodes[leader].commit_index;
        let n_entries = entries.len();

        let (success, reply_term) = self.handle_append_entries(
            follower,
            term,
            leader,
            prev_log_index,
            prev_log_term,
            entries,
            leader_commit,
        );

        if reply_term > self.nodes[leader].current_term {
            self.become_follower(leader, reply_term);
            return;
        }
        if success {
            let new_match = prev_log_index + n_entries;
            self.nodes[leader].next_index.insert(follower, new_match);
            self.nodes[leader].match_index.insert(follower, new_match);
        } else {
            let cur = *self.nodes[leader].next_index.get(&follower).unwrap_or(&0);
            self.nodes[leader].next_index.insert(follower, cur.saturating_sub(1));
        }
    }

    // 8 parameters mirrors the real AppendEntries RPC's own field
    // list (term, leaderId, prevLogIndex, prevLogTerm, entries[],
    // leaderCommit, plus the recipient and this sim's lack of a
    // separate request struct) -- a struct here would just rename
    // these same fields, not simplify anything.
    #[allow(clippy::too_many_arguments)]
    fn handle_append_entries(
        &mut self,
        follower: usize,
        term: u64,
        leader_id: usize,
        prev_log_index: usize,
        prev_log_term: u64,
        entries: Vec<LogEntry>,
        leader_commit: usize,
    ) -> (bool, u64) {
        if term < self.nodes[follower].current_term {
            return (false, self.nodes[follower].current_term);
        }
        self.nodes[follower].current_term = term;
        self.nodes[follower].role = Role::Follower;
        self.nodes[follower].voted_for = Some(leader_id);
        self.reset_deadline(follower);

        if prev_log_index > 0 {
            let ok = self.nodes[follower].log_term(prev_log_index) == Some(prev_log_term);
            if !ok {
                return (false, term);
            }
        }

        for (idx, entry) in (prev_log_index..).zip(entries) {
            let local_index = idx.saturating_sub(self.nodes[follower].snapshot_index);
            if self.nodes[follower].log.len() > local_index {
                if self.nodes[follower].log[local_index].term != entry.term {
                    self.nodes[follower].log.truncate(local_index);
                    self.nodes[follower].log.push(entry);
                }
            } else {
                self.nodes[follower].log.push(entry);
            }
        }

        if leader_commit > self.nodes[follower].commit_index {
            self.nodes[follower].commit_index = leader_commit.min(self.nodes[follower].last_log_info().0);
        }
        (true, term)
    }

    /// Commits index N if a majority of nodes (leader included) have
    /// replicated it AND its term matches the leader's current term —
    /// the Raft safety rule that stops a leader from committing (and
    /// thus exposing) an entry from an earlier term via replication
    /// count alone.
    fn advance_commit_index(&mut self, leader: usize) {
        let n = self.nodes.len();
        let leader_log_len = self.nodes[leader].last_log_info().0;
        let current_term = self.nodes[leader].current_term;

        for index in (self.nodes[leader].commit_index + 1..=leader_log_len).rev() {
            if self.nodes[leader].log_term(index) != Some(current_term) {
                continue;
            }
            let mut count = 1; // leader itself
            for other in 0..n {
                if other == leader {
                    continue;
                }
                if *self.nodes[leader].match_index.get(&other).unwrap_or(&0) >= index {
                    count += 1;
                }
            }
            if count * 2 > n {
                self.nodes[leader].commit_index = index;
                self.events.push(format!(
                    "tick {}: leader {} commits log index {}",
                    self.tick_count, leader, index
                ));
                break;
            }
        }
    }
}

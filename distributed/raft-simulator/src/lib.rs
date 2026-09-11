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
    election_deadline: u64,
    next_index: HashMap<usize, usize>,
    match_index: HashMap<usize, usize>,
}

impl Node {
    fn new(id: usize, deadline: u64) -> Self {
        Node {
            id,
            role: Role::Follower,
            current_term: 0,
            voted_for: None,
            log: Vec::new(),
            commit_index: 0,
            election_deadline: deadline,
            next_index: HashMap::new(),
            match_index: HashMap::new(),
        }
    }

    fn last_log_info(&self) -> (usize, u64) {
        match self.log.last() {
            Some(e) => (self.log.len(), e.term),
            None => (0, 0),
        }
    }
}

pub struct Cluster {
    pub nodes: Vec<Node>,
    pub alive: Vec<bool>,
    pub tick_count: u64,
    pub events: Vec<String>,
    election_timeout_base: u64,
    heartbeat_interval: u64,
}

impl Cluster {
    /// `n` nodes, IDs 0..n. Election timeouts are deterministically
    /// staggered (`base + id * 10`) rather than randomized, so the
    /// same cluster size always produces the same sequence of events
    /// — reproducibility over strict Raft-paper realism (which
    /// randomizes timeouts to avoid split votes); see README.
    pub fn new(n: usize) -> Self {
        let election_timeout_base = 150;
        let nodes = (0..n)
            .map(|id| Node::new(id, election_timeout_base + (id as u64) * 10))
            .collect();
        Cluster {
            nodes,
            alive: vec![true; n],
            tick_count: 0,
            events: Vec::new(),
            election_timeout_base,
            heartbeat_interval: 20,
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
        self.events.push(format!(
            "tick {}: leader {} appends '{}' at log index {}",
            self.tick_count,
            leader,
            command,
            self.nodes[leader].log.len()
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
        self.nodes[id].election_deadline = self.tick_count + self.election_timeout_base + (id as u64) * 10;
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
            if other == id || !self.alive[other] {
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
        let log_len = self.nodes[id].log.len();
        for other in 0..self.nodes.len() {
            self.nodes[id].next_index.insert(other, log_len);
            self.nodes[id].match_index.insert(other, 0);
        }
        self.send_heartbeats(id);
    }

    fn send_heartbeats(&mut self, leader: usize) {
        for other in 0..self.nodes.len() {
            if other == leader || !self.alive[other] {
                continue;
            }
            self.replicate_to(leader, other);
        }
        self.advance_commit_index(leader);
    }

    fn replicate_to(&mut self, leader: usize, follower: usize) {
        let next = *self.nodes[leader].next_index.get(&follower).unwrap_or(&0);
        let prev_log_index = next;
        let prev_log_term = if prev_log_index == 0 {
            0
        } else {
            self.nodes[leader].log[prev_log_index - 1].term
        };
        let entries: Vec<LogEntry> = self.nodes[leader].log[next..].to_vec();
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
            let ok = self.nodes[follower].log.len() >= prev_log_index
                && self.nodes[follower].log[prev_log_index - 1].term == prev_log_term;
            if !ok {
                return (false, term);
            }
        }

        for (idx, entry) in (prev_log_index..).zip(entries) {
            if self.nodes[follower].log.len() > idx {
                if self.nodes[follower].log[idx].term != entry.term {
                    self.nodes[follower].log.truncate(idx);
                    self.nodes[follower].log.push(entry);
                }
            } else {
                self.nodes[follower].log.push(entry);
            }
        }

        if leader_commit > self.nodes[follower].commit_index {
            self.nodes[follower].commit_index = leader_commit.min(self.nodes[follower].log.len());
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
        let leader_log_len = self.nodes[leader].log.len();
        let current_term = self.nodes[leader].current_term;

        for index in (self.nodes[leader].commit_index + 1..=leader_log_len).rev() {
            if self.nodes[leader].log[index - 1].term != current_term {
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

use raft_simulator::{Cluster, Role};

#[test]
fn a_leader_emerges_normal_case() {
    let mut cluster = Cluster::new(5);
    cluster.run(200);
    assert!(cluster.leader().is_some());
}

#[test]
fn exactly_one_leader_at_a_time() {
    let mut cluster = Cluster::new(5);
    cluster.run(200);
    let leader_count = cluster
        .nodes
        .iter()
        .enumerate()
        .filter(|(i, n)| cluster.alive[*i] && n.role == Role::Leader)
        .count();
    assert_eq!(leader_count, 1);
}

#[test]
fn killing_the_leader_triggers_a_new_election() {
    let mut cluster = Cluster::new(5);
    cluster.run(200);
    let first_leader = cluster.leader().expect("a leader should have emerged");
    let first_term = cluster.nodes[first_leader].current_term;

    cluster.kill(first_leader);
    cluster.run(250);

    let second_leader = cluster
        .leader()
        .expect("a new leader should emerge after the old one is killed");
    assert_ne!(second_leader, first_leader);
    assert!(cluster.nodes[second_leader].current_term > first_term);
}

#[test]
fn submitted_command_gets_committed_normal_case() {
    let mut cluster = Cluster::new(5);
    cluster.run(200);
    assert!(cluster.submit("x=1"));
    cluster.run(50);

    let leader = cluster.leader().unwrap();
    assert_eq!(cluster.nodes[leader].commit_index, 1);
    assert_eq!(cluster.nodes[leader].log[0].command, "x=1");
}

#[test]
fn committed_command_replicates_to_a_majority() {
    let mut cluster = Cluster::new(5);
    cluster.run(200);
    cluster.submit("x=1");
    cluster.run(50);

    let leader = cluster.leader().unwrap();
    let committed_count = (0..cluster.nodes.len())
        .filter(|&i| cluster.alive[i] && !cluster.nodes[i].log.is_empty() && cluster.nodes[i].log[0].command == "x=1")
        .count();
    assert!(
        committed_count * 2 > cluster.nodes.len(),
        "leader {leader}: only {committed_count}/5 nodes have the entry"
    );
}

#[test]
fn submit_with_no_leader_fails_edge_case() {
    let mut cluster = Cluster::new(5);
    // No ticks run yet -- every node is still a Follower, no leader exists.
    assert!(!cluster.submit("too-early"));
}

#[test]
fn minority_partition_cannot_elect_a_leader_invalid_case() {
    // Kill 3 of 5 nodes -- only 2 remain, which can never reach a
    // majority (3 of 5) no matter how elections go.
    let mut cluster = Cluster::new(5);
    cluster.kill(0);
    cluster.kill(1);
    cluster.kill(2);
    cluster.run(400);
    assert!(cluster.leader().is_none());
}

#[test]
fn revived_node_rejoins_as_a_follower() {
    let mut cluster = Cluster::new(5);
    cluster.run(200);
    let leader = cluster.leader().unwrap();
    let victim = (0..5).find(|&i| i != leader).unwrap();

    cluster.kill(victim);
    cluster.run(50);
    cluster.revive(victim);
    cluster.run(50);

    assert!(cluster.alive[victim]);
    assert_eq!(cluster.nodes[victim].role, Role::Follower);
}

// CLI: raft --nodes 5
// (no args: --nodes 5, runs the leader-failure scenario)
use std::env;

use raft_simulator::Cluster;

fn main() {
    let args: Vec<String> = env::args().collect();
    let mut num_nodes = 5;
    let mut i = 1;
    while i < args.len() {
        if args[i] == "--nodes" && i + 1 < args.len() {
            num_nodes = args[i + 1].parse().unwrap_or(5);
            i += 2;
        } else {
            i += 1;
        }
    }

    if args.iter().any(|arg| arg == "--experiments") {
        run_experiments();
        return;
    }

    let mut cluster = Cluster::new(num_nodes);
    println!("Raft simulator: {num_nodes} nodes\n");

    cluster.run(200); // let an initial leader emerge
    if let Some(leader) = cluster.leader() {
        cluster.submit("x=1");
        cluster.submit("x=2");
        cluster.run(50); // let the commands replicate and commit

        println!("--- killing the leader (node {leader}) ---\n");
        cluster.kill(leader);
        cluster.run(250); // let a new election happen
    }

    for line in &cluster.events {
        println!("{line}");
    }

    println!();
    match cluster.leader() {
        Some(leader) => println!(
            "final state: node {leader} is leader (term {}), commit_index {}",
            cluster.nodes[leader].current_term, cluster.nodes[leader].commit_index
        ),
        None => println!("final state: no leader"),
    }

    println!("\nExperiment: recovery time for repeated leader failures (5 nodes)\n");
    println!("A 5-node cluster tolerates up to 2 failures before losing majority (3/5).");
    println!("{:<20}ticks since the previous event", "event");
    let mut c = Cluster::new(5);
    let mut last_tick = 0u64;
    for t in 1..=1500u64 {
        c.tick();
        if c.leader().is_some() && t > last_tick {
            let elapsed = t - last_tick;
            println!("{:<20}{elapsed}", "leader elected");
            last_tick = t;
            let leader = c.leader().unwrap();
            if c.alive.iter().filter(|&&a| a).count() <= 3 {
                break; // one more kill would drop below majority; stop here
            }
            c.kill(leader);
        }
    }
}

fn run_experiments() {
    println!("Seeded timeout experiment (seeds 0..1000)");
    println!("nodes  timeout collisions  split votes");
    for nodes in [3, 5, 7] {
        let mut collisions = 0;
        let mut split_votes = 0;
        for seed in 0..1000 {
            let mut cluster = Cluster::with_seed(nodes, seed);
            cluster.run(400);
            collisions += cluster.timeout_collisions;
            split_votes += cluster.split_vote_count;
        }
        println!("{nodes:>5} {collisions:>19} {split_votes:>12}");
    }

    let mut partitioned = Cluster::new(4);
    partitioned.partition(&[0, 1], &[2, 3]);
    partitioned.run(400);
    println!(
        "\nPartition experiment (4 nodes split 2/2): leader while partitioned = {:?}",
        partitioned.leader()
    );
    partitioned.heal_partition();
    partitioned.run(200);
    println!("leader after healing = {:?}", partitioned.leader());

    let mut compacted = Cluster::new(5);
    compacted.run(200);
    for index in 0..100 {
        assert!(compacted.submit(&format!("x={index}")));
    }
    compacted.run(100);
    let before = compacted.log_storage_len();
    let commit_index = compacted.nodes[compacted.leader().unwrap()].commit_index;
    compacted.compact(commit_index);
    println!(
        "\nCompaction experiment (100 committed commands): storage before = {before}, after = {}",
        compacted.log_storage_len()
    );
}

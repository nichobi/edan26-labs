#[macro_use] extern crate text_io;

use std::sync::{Mutex,Arc,Condvar};
use std::collections::LinkedList;
use std::cmp;
use std::thread;
use std::process::exit;
use std::collections::VecDeque;

const DEBUG: bool = false;

macro_rules! pr {
    ($fmt_string:expr, $($arg:expr),*) => {
            if DEBUG {println!($fmt_string, $($arg),*);}
    };

    ($fmt_string:expr) => {
            if DEBUG {println!($fmt_string);}
    };
}


struct Node {
	i:	usize,			/* index of itself for debugging.	*/
	e:	i32,			/* excess preflow.			*/
	h:	i32,			/* height.				*/
}

struct Edge {
        u:      usize,
        v:      usize,
        f:      i32,
        c:      i32,
}

struct ExcessQueue {
    queue: Mutex<VecDeque<usize>>,
    cond: Condvar,
}

impl ExcessQueue {
    fn new() -> ExcessQueue {
        ExcessQueue {
            queue: Mutex::new(VecDeque::new()),
            cond: Condvar::new(),
        }
    }
}

impl Node {
	fn new(ii:usize) -> Node {
		Node { i: ii, e: 0, h: 0, }
	}

	fn new2(ii:usize, hh: i32) -> Node {
		Node { i: ii, e: 0, h: hh, }
	}
}

impl Edge {
        fn new(uu:usize, vv:usize,cc:i32) -> Edge {
                Edge { u: uu, v: vv, f: 0, c: cc }
        }
}

fn enter_excess(excess: &ExcessQueue, u: &mut Node, n: usize, index: usize){
    pr!("Node {}: u.i != 0 && u.i != n-1 && u.e > 0 = {}, {}, {}"
       , u.i, u.i != 0 , u.i != n-1 , u.e > 0);
    if u.i != 0 && u.i != n-1 && u.e > 0 {
        pr!("Thread {} adds node {} to excess", index, u.i);
        excess.queue.lock().unwrap().push_back(u.i);
        pr!("Added node to excess");
    }
    excess.cond.notify_all();
}

fn leave_excess(excess: &ExcessQueue, nodes: &mut Vec<Arc<Mutex<Node>>>,  n: usize) -> usize {
    pr!("Entering leave_excess");
    let mut q = excess.queue.lock().unwrap();
    let mut u_opt = q.pop_front();
    while u_opt == None  {
        are_we_done(&nodes, n);
        q = excess.cond.wait(q).unwrap();
        u_opt = q.pop_front();
    }
    let u = u_opt.unwrap();
    return u;
}

fn are_we_done(nodes: &Vec<Arc<Mutex<Node>>>, n: usize) -> bool {
    let t_node = nodes[n-1].lock().unwrap();
    let s_node = nodes[0].lock().unwrap();
    pr!("are_we_done: t.e={}, sum={}", t_node.e, -s_node.e);
    if t_node.e == -s_node.e {
        println!("f = {}", t_node.e);
        exit(0);
    }
    return t_node.e == -s_node.e;
}


fn main() {

	let n: usize = read!();		/* n nodes.						*/
	let m: usize = read!();		/* m edges.						*/
	let _c: usize = read!();	/* underscore avoids warning about an unused variable.	*/
	let _p: usize = read!();	/* c and p are in the input from 6railwayplanning.	*/
	let mut nodes = vec![];
	let mut edges = vec![];
	let mut adj: Vec<LinkedList<usize>> = Vec::with_capacity(n);
	//let mut excess: BlockingQueue<usize> = BlockingQueue::new();
    let excess = Arc::new(ExcessQueue::new());
	let debug = false;
    let num_threads = 4;

	let s = 0;
	let t = n-1;

	pr!("n = {}", n);
	pr!("m = {}", m);

    {
        let s:Node = Node::new2(0, n as i32);
        nodes.push(Arc::new(Mutex::new(s)));
		adj.push(LinkedList::new());
    }
	for i in 1..n {
		let u:Node = Node::new(i);
		nodes.push(Arc::new(Mutex::new(u)));
		adj.push(LinkedList::new());
	}

	for i in 0..m {
		let u: usize = read!();
		let v: usize = read!();
		let c: i32 = read!();
		let e:Edge = Edge::new(u,v,c);
		adj[u].push_back(i);
		adj[v].push_back(i);
		edges.push(Arc::new(Mutex::new(e)));
	}

	if debug {
		for i in 0..n {
			print!("adj[{}] = ", i);
			let iter = adj[i].iter();

			for e in iter {
				print!("e = {}, ", e);
			}
			println!("");
		}
	}

	pr!("initial pushes");
	let iter = adj[s].iter();
    for e in iter {
        let v : usize;
        let mut edge = edges[*e].lock().unwrap();
        if edge.u == s {v = edge.v;}
        else {v = edge.u;}

        let mut s_node = nodes[s].lock().unwrap();
        let mut v_node = nodes[v].lock().unwrap();
        let d : i32;

        if s == edge.u {
            d = edge.c;
            //d = cmp::min(s_node.e, edge.c - edge.f);
            edge.f += d;
        } else {
            d = edge.c;
            //d = cmp::min(s_node.e, edge.c + edge.f);
            edge.f -= d;
        }
        pr!("Pushing: {}->{}: d={}", s, v, d);
        s_node.e -= d;
        v_node.e += d;

        if v_node.e == d {
            enter_excess(&excess, &mut v_node, n, 0);
        }
    }

	// but nothing is done here yet...

	let mut threads = vec![];
	for i in 1 .. num_threads + 1{
        let index = i;
        let mut excess_l = excess.clone();
        let mut nodes_l = nodes.clone();
        let adj_l = adj.clone();
        let edges_l = edges.clone();
		let h = thread::spawn(move || {
            loop {
                pr!("Thread {} enters leave_excess", index);
                let u = leave_excess(&mut excess_l, &mut nodes_l, n);
                pr!("Thread {} takes node {} from excess", index, u);
                let iter = adj_l[u].iter();

                {
                    let u_node = nodes_l[u].lock().unwrap();
                    pr!("with excess {}", u_node.e);
                }

                let mut has_pushed = false;
                for e in iter {
                    let v : usize;
                    let mut edge = edges_l[*e].lock().unwrap();
                    if edge.u == u {v = edge.v;}
                    else {v = edge.u;}

                    let mut u_node;
                    let mut v_node;
                    if u < v {
                        u_node = nodes_l[u].lock().unwrap();
                        v_node = nodes_l[v].lock().unwrap();
                    } else {
                        v_node = nodes_l[v].lock().unwrap();
                        u_node = nodes_l[u].lock().unwrap();
                    }
                    if u_node.e == 0 {break;}
                    if u_node.h > v_node.h {
                        let d : i32;
                        if u == edge.u {
                            d = cmp::min(u_node.e, edge.c - edge.f);
                            edge.f += d;
                        } else {
                            d = cmp::min(u_node.e, edge.c + edge.f);
                            edge.f -= d;
                        }
                        if d > 0 {
                            pr!("Pushing: {}->{}: d={}", u, v, d);
                            u_node.e -= d;
                            v_node.e += d;

                            if v_node.e == d {
                                enter_excess(&excess_l, &mut v_node, n, index);
                            }
                            has_pushed = true;
                        }
                    }
                }

                if !has_pushed {
                    let mut u_node = nodes_l[u].lock().unwrap();
                    u_node.h += 1;
                    pr!("Relabeled node {}, new h={}", u, u_node.h);
                    enter_excess(&excess_l, &mut u_node, n, index);
                } else {
                    let mut u_node = nodes_l[u].lock().unwrap();
                    if u_node.e > 0 {
                        enter_excess(&excess_l, &mut u_node, n, index);
                    }
                }
            }
            //println!("Thread {} exits", index);
        });
        threads.push(h);
    }

    for h in threads {
        h.join().unwrap();
    }

    let t_node = nodes[t].lock().unwrap();
	println!("f = {}", t_node.e);

}

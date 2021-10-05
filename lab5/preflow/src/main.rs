#[macro_use] extern crate text_io;

use std::sync::{Mutex,Arc};
use std::collections::LinkedList;
use std::cmp;
//use std::thread;
use std::collections::VecDeque;

const DEBUG: bool = true;

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
    in_excess : bool,
}

struct Edge {
        u:      usize,
        v:      usize,
        f:      i32,
        c:      i32,
}

impl Node {
	fn new(ii:usize) -> Node {
		Node { i: ii, e: 0, h: 0, in_excess: false }
	}

	fn new2(ii:usize, hh: i32) -> Node {
		Node { i: ii, e: 0, h: hh, in_excess: false}
	}
}

impl Edge {
        fn new(uu:usize, vv:usize,cc:i32) -> Edge {
                Edge { u: uu, v: vv, f: 0, c: cc }
        }
}

fn enter_excess(excess: &mut VecDeque<usize>, u: &mut Node, n: usize){
    if u.i != 0 && u.i != n-1 && u.e > 0 && !u.in_excess {
        pr!("Adding node {} to excess", u.i);
        excess.push_back(u.i);
    }
    u.in_excess = true;
}

fn leave_excess(excess: &mut VecDeque<usize>, nodes: &mut Vec<Arc<Mutex<Node>>>) -> usize {
    let u = excess.pop_front().unwrap();
    let mut u_node = nodes[u].lock().unwrap();
    u_node.in_excess = false;
    return u;
}

fn main() {

	let n: usize = read!();		/* n nodes.						*/
	let m: usize = read!();		/* m edges.						*/
	let _c: usize = read!();	/* underscore avoids warning about an unused variable.	*/
	let _p: usize = read!();	/* c and p are in the input from 6railwayplanning.	*/
	let mut nodes = vec![];
	let mut edges = vec![];
	let mut adj: Vec<LinkedList<usize>> =Vec::with_capacity(n);
	let mut excess: VecDeque<usize> = VecDeque::new();
	let debug = false;

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
    //fn push(edge: Edge, u: Node) {
    //}

    //fn other(e: usize, u: usize) -> usize {
    //    let edge = edges[e].lock().unwrap();
    //    if edge.u == u {return edge.v;}
    //    else {return edge.u;}
    //}

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
            enter_excess(&mut excess, &mut v_node, n);
        }
    }

	// but nothing is done here yet...

	while !excess.is_empty() {
		//let mut c = 0;
		let u = leave_excess(&mut excess, &mut nodes);
        pr!("Take node {} from excess", u);
        let iter = adj[u].iter();

        {
            let u_node = nodes[u].lock().unwrap();
            pr!("with excess {}", u_node.e);
        }

        let mut has_pushed = false;
        for e in iter {
            let v : usize;
            let mut edge = edges[*e].lock().unwrap();
            if edge.u == u {v = edge.v;}
            else {v = edge.u;}

            let mut u_node = nodes[u].lock().unwrap();
            if u_node.e == 0 {break;}
            //pr!("with excess {}", u_node.e);
            let mut v_node = nodes[v].lock().unwrap();
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

                    //if u_node.e > 0 {
                        //excess.push_back(u);
                    //}
                    //if v_node.e == d {
                        enter_excess(&mut excess, &mut v_node, n);
                        //excess.push_back(v);
                    //}
                    has_pushed = true;
                }
            }
        }
        {
            let mut u_node = nodes[u].lock().unwrap();
            if u_node.e > 0 {
                enter_excess(&mut excess, &mut u_node, n);
            }
        }


        if !has_pushed {
            let mut u_node = nodes[u].lock().unwrap();
            u_node.h += 1;
            pr!("Relabeled node {}, new h={}", u, u_node.h);
            enter_excess(&mut excess, &mut u_node, n);
            //excess.push_back(u);
        }
	}

    let t_node = nodes[t].lock().unwrap();
	println!("f = {}", t_node.e);

}

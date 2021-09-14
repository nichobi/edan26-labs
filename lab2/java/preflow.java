import java.util.Scanner;
import java.util.Iterator;
import java.util.ListIterator;
import java.util.LinkedList;

import java.io.*;

class P {
  static boolean print = true;
  static void p(String string) {
    if(print) System.out.println(string);
  }
  static void f(String string, Object... objects){
    if(print) System.out.printf(string, objects);
  }
}


class Graph {

  int  s;
  int  t;
  int  n;
  int  m;
  Node  excess;    // list of nodes with excess preflow
  Node  node[];
  Edge  edge[];

  Graph(Node node[], Edge edge[])
  {
    this.node  = node;
    this.n    = node.length;
    this.edge  = edge;
    this.m    = edge.length;
  }

  void enter_excess(Node u)
  {
    if (u != node[s] && u != node[t]) {
      u.next = excess;
      excess = u;
    }
  }

  Node other(Edge a, Node u)
  {
    if (a.u == u)
      return a.v;
    else
      return a.u;
  }

  void relabel(Node u)
  {
    u.h+=1;
    P.f("relabel %d now h = %d\n", u.i, u.h);
    enter_excess(u);
  }

  void push(Node u, Node v, Edge e) {
    int    d;  /* remaining capacity of the edge. */

    P.f("push from %d to %d: ", u.i, v.i);
    P.f("f = %d, c = %d, so ", e.f, e.c);

    if (u == e.u) {
      d = Math.min(u.e, e.c - e.f);
      e.f += d;
    } else {
      d = Math.min(u.e, e.c + e.f);
      e.f -= d;
    }

    P.f("pushing %d\n", d);

    u.e -= d;
    v.e += d;

    /* the following are always true. */

    assert(d >= 0);
    assert(u.e >= 0);
    assert(Math.abs(e.f) <= e.c);

    if (u.e > 0) {

      /* still some remaining so let u push more. */

      enter_excess(u);
    }

    if (v.e == d) {

      /* since v has d excess now it had zero before and
       * can now push.
       *
       */

      enter_excess(v);
    }
  }

  int preflow(int s, int t)
  {
    ListIterator<Edge>  iter;
    int      b;
    Edge      e;
    Node      u;
    Node      v;

    this.s = s;
    this.t = t;
    node[s].h = n;

    iter = node[s].adj.listIterator();
    while (iter.hasNext()) {
      e = iter.next();

      node[s].e += e.c;

      push(node[s], other(e, node[s]), e);
    }

    while (excess != null) {
      u = excess;
      v = null;
      e = null;
      excess = u.next;
      P.f("selected u = %d with ", u.i);
      P.f("h = %d and e = %d\n", u.h, u.e);

      iter = u.adj.listIterator();
      while (iter.hasNext()) {
        e = iter.next();
        if (u == e.u) {
          v = e.v;
          b = 1;
        } else {
          v = e.u;
          b = -1;
        }

        if (u.h > v.h && b * e.f < e.c)
          break;
        else
          v = null;
      }

      if (v != null)
        push(u, v, e);
      else
        relabel(u);
    }

    return node[t].e;
  }
}

class Node {
  int  h;
  int  e;
  int  i;
  Node  next;
  LinkedList<Edge>  adj;

  Node(int i)
  {
    this.i = i;
    adj = new LinkedList<Edge>();
  }
}

class Edge {
  Node  u;
  Node  v;
  int  f;
  int  c;

  Edge(Node u, Node v, int c)
  {
    this.u = u;
    this.v = v;
    this.c = c;

  }
}

class Preflow {
  public static void main(String args[])
  {
    double  begin = System.currentTimeMillis();
    Scanner s = new Scanner(System.in);
    int  n;
    int  m;
    int  i;
    int  u;
    int  v;
    int  c;
    int  f;
    Graph  g;

    n = s.nextInt();
    m = s.nextInt();
    s.nextInt();
    s.nextInt();
    Node[] node = new Node[n];
    Edge[] edge = new Edge[m];

    for (i = 0; i < n; i += 1)
      node[i] = new Node(i);

    for (i = 0; i < m; i += 1) {
      u = s.nextInt();
      v = s.nextInt();
      c = s.nextInt();
      edge[i] = new Edge(node[u], node[v], c);
      node[u].adj.addLast(edge[i]);
      node[v].adj.addLast(edge[i]);
    }

    g = new Graph(node, edge);
    f = g.preflow(0, n-1);
    double  end = System.currentTimeMillis();
    System.out.println("t = " + (end - begin) / 1000.0 + " s");
    System.out.println("f = " + f);
  }
}

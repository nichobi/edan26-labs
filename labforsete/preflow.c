/* This is an implementation of the preflow-push algorithm, by
 * Goldberg and Tarjan, for the 2021 EDAN26 Multicore programming labs.
 *
 * It is intended to be as simple as possible to understand and is
 * not optimized in any way.
 *
 * You should NOT read everything for this course.
 *
 * Focus on what is most similar to the pseudo code, i.e., the functions
 * preflow, push, and relabel.
 *
 * Some things about C are explained which are useful for everyone
 * for lab 3, and things you most likely want to skip have a warning
 * saying it is only for the curious or really curious.
 * That can safely be ignored since it is not part of this course.
 *
 * Compile and run with: make
 *
 * Enable prints by changing from 1 to 0 at PRINT below.
 *
 * Feel free to ask any questions about it on Discord
 * at #lab0-preflow-push
 *
 * A variable or function declared with static is only visible from
 * within its file so it is a good practice to use in order to avoid
 * conflicts for names which need not be visible from other files.
 *
 */

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define PRINT		0	/* enable/disable prints. */

/* the funny do-while next clearly performs one iteration of the loop.
 * if you are really curious about why there is a loop, please check
 * the course book about the C preprocessor where it is explained. it
 * is to avoid bugs and/or syntax errors in case you use the pr in an
 * if-statement without { }.
 *
 */

#if PRINT
#define pr(...)		do { fprintf(stderr, __VA_ARGS__); } while (0)
#else
#define pr(...)		/* no effect at all */
#endif

#define MIN(a,b)	(((a)<=(b))?(a):(b))

/* introduce names for some structs. a struct is like a class, except
 * it cannot be extended and has no member methods, and everything is
 * public.
 *
 * using typedef like this means we can avoid writing 'struct' in
 * every declaration. no new type is introduded and only a shorter name.
 *
 */

typedef struct graph_t	graph_t;
typedef struct node_t	node_t;
typedef struct edge_t	edge_t;
typedef struct list_t	list_t;
typedef struct push_list_t	push_list_t;
typedef struct node_list_t	node_list_t;
typedef struct push_t	push_t;
typedef struct work_arg_t	work_arg_t;
typedef struct node_list_t node_list_t;
typedef struct edge_list_t edge_list_t;
typedef struct edge_data_t edge_data_t;

struct list_t {
	edge_t*		edge;
	list_t*		next;
};

struct work_arg_t {
	int		   index;
	graph_t* g;
  int      nThreads;
};

struct edge_data_t {
  int c;
  int f;
};

struct push_list_t {
  push_t* a;
  int     c;
  int     i;
};

struct edge_list_t {
  edge_t* a;
  int     c;
  int     i;
};

struct node_list_t {
  node_t** a;
  int      c;
  int      i;
};

struct node_t {
	int		h;	/* height.			*/
	int		e;	/* excess flow.			*/
	//list_t*		edge;	/* adjacency list.		*/
	node_t*		next;	/* with excess preflow.		*/
  edge_list_t edge;
  int  inExcess;
};

struct edge_t {
	node_t*		v;	/* the other. 			*/
	int		i;	/* edge index */
	int		b;	//direction
};

struct push_t {
  node_t* u; // Push from u
  node_t* v; // Push to v
  int     edge_i;
  int d; // Directed flow to push
};

struct graph_t {
	int		n;	/* nodes.			*/
	int		m;	/* edges.			*/
	node_t*		v;	/* array of n nodes.		*/
	edge_t*		e;	/* array of m edges.		*/
	node_t*		s;	/* source.			*/
	node_t*		t;	/* sink.			*/
	node_t*		excess;	/* nodes with e > 0 except s,t.	*/
  edge_data_t* edge_data;
  pthread_barrier_t barrier;
  push_list_t** pushes;
  node_list_t** relabels;
  node_list_t* workList;
  pthread_mutex_t mutex;
  int done;
};

/* a remark about C arrays. the phrase above 'array of n nodes' is using
 * the word 'array' in a general sense for any language. in C an array
 * (i.e., the technical term array in ISO C) is declared as: int x[10],
 * i.e., with [size] but for convenience most people refer to the data
 * in memory as an array here despite the graph_t's v and e members
 * are not strictly arrays. they are pointers. once we have allocated
 * memory for the data in the ''array'' for the pointer, the syntax of
 * using an array or pointer is the same so we can refer to a node with
 *
 * 			g->v[i]
 *
 * where the -> is identical to Java's . in this expression.
 *
 * in summary: just use the v and e as arrays.
 *
 * a difference between C and Java is that in Java you can really not
 * have an array of nodes as we do. instead you need to have an array
 * of node references. in C we can have both arrays and local variables
 * with structs that are not allocated as with Java's new but instead
 * as any basic type such as int.
 *
 */

static char* progname;

#if PRINT

static int id(graph_t* g, node_t* v)
{
	/* return the node index for v.
	 *
	 * the rest is only for the curious.
	 *
	 * we convert a node pointer to its index by subtracting
	 * v and the array (which is a pointer) with all nodes.
	 *
	 * if p and q are pointers to elements of the same array,
	 * then p - q is the number of elements between p and q.
	 *
	 * we can of course also use q - p which is -(p - q)
	 *
	 * subtracting like this is only valid for pointers to the
	 * same array.
	 *
	 * what happens is a subtract instruction followed by a
	 * divide by the size of the array element.
	 *
	 */

	return v - g->v;
}
#endif

void error(const char* fmt, ...)
{
	/* print error message and exit.
	 *
	 * it can be used as printf with formatting commands such as:
	 *
	 *	error("height is negative %d", v->h);
	 *
	 * the rest is only for the really curious. the va_list
	 * represents a compiler-specific type to handle an unknown
	 * number of arguments for this error function so that they
	 * can be passed to the vsprintf function that prints the
	 * error message to buf which is then printed to stderr.
	 *
	 * the compiler needs to keep track of which parameters are
	 * passed in integer registers, floating point registers, and
	 * which are instead written to the stack.
	 *
	 * avoid ... in performance critical code since it makes
	 * life for optimizing compilers much more difficult. but in
	 * in error functions, they obviously are fine (unless we are
	 * sufficiently paranoid and don't want to risk an error
	 * condition escalate and crash a car or nuclear reactor
	 * instead of doing an even safer shutdown (corrupted memory
	 * can cause even more damage if we trust the stack is in good
	 * shape)).
	 *
	 */

	va_list		ap;
	char		buf[BUFSIZ];

	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);

	if (progname != NULL)
		fprintf(stderr, "%s: ", progname);

	fprintf(stderr, "error: %s\n", buf);
	exit(1);
}

static int next_int()
{
        int     x;
        int     c;

	/* this is like Java's nextInt to get the next integer.
	 *
	 * we read the next integer one digit at a time which is
	 * simpler and faster than using the normal function
	 * fscanf that needs to do more work.
	 *
	 * we get the value of a digit character by subtracting '0'
	 * so the character '4' gives '4' - '0' == 4
	 *
	 * it works like this: say the next input is 124
	 * x is first 0, then 1, then 10 + 2, and then 120 + 4.
	 *
	 */

	x = 0;
        while (isdigit(c = getchar()))
                x = 10 * x + c - '0';

        return x;
}

static void* xmalloc(size_t s)
{
	void*		p;

	/* allocate s bytes from the heap and check that there was
	 * memory for our request.
	 *
	 * memory from malloc contains garbage except at the beginning
	 * of the program execution when it contains zeroes for
	 * security reasons so that no program should read data written
	 * by a different program and user.
	 *
	 * size_t is an unsigned integer type (printed with %zu and
	 * not %d as for int).
	 *
	 */

	p = malloc(s);

	if (p == NULL)
		error("out of memory: malloc(%zu) failed", s);

	return p;
}

static void* xcalloc(size_t n, size_t s)
{
	void*		p;

	p = xmalloc(n * s);

	/* memset sets everything (in this case) to 0. */
	memset(p, 0, n * s);

	/* for the curious: so memset is equivalent to a simple
	 * loop but a call to memset needs less memory, and also
 	 * most computers have special instructions to zero cache
	 * blocks which usually are used by memset since it normally
	 * is written in assembler code. note that good compilers
	 * decide themselves whether to use memset or a for-loop
	 * so it often does not matter. for small amounts of memory
	 * such as a few bytes, good compilers will just use a
	 * sequence of store instructions and no call or loop at all.
	 *
	 */

	return p;
}

static void add_edge(node_t* u, node_t* v, int i, int b)
{
	//list_t*		p;

	/* allocate memory for a list link and put it first
	 * in the adjacency list of u.
	 *
	 */

	//p = xmalloc(sizeof(list_t));
	//p->edge = e;
	//p->next = u->edge;
	//u->edge = p;
  if (u->edge.i == u->edge.c) {
    edge_t* b;
    u->edge.c *= 2; // double the capacity
    b = realloc(u->edge.a, u->edge.c * sizeof(u->edge.a[0]));
    if (b == NULL)
      error("no memory");
    u->edge.a = b;
  }

  //printf("u=%d\n", id(g, u));
  printf("i=%d\n", u->edge.i);
  u->edge.a[u->edge.i].v = v;
  u->edge.a[u->edge.i].i = i;
  u->edge.a[u->edge.i].b = b;
  u->edge.i += 1;
}

static void add_push(graph_t* g, node_t* u, node_t* v, int edge_index, int d, int threadIndex)
{
  //int d;
  if (g->pushes[threadIndex]->i == g->pushes[threadIndex]->c) {
    push_t* b;
    g->pushes[threadIndex]->c *= 2; // double the capacity
    b = realloc(g->pushes[threadIndex]->a, g->pushes[threadIndex]->c * sizeof(g->pushes[threadIndex]->a[0]));
    if (b == NULL)
      error("no memory");
    g->pushes[threadIndex]->a = b;
  }

	//if (u == e->u) {
	//	d = MIN(u->e, e->c - e->f);
	//	e->f += d;
	//} else {
	//	d = MIN(u->e, e->c + e->f);
	//	e->f -= d;
	//}

  pr("add_push changing excess node:%d, e=%d, d=%d\n", id(g,u), u->e, d);
  u->e -= d;

  int i = g->pushes[threadIndex]->i++;
  g->pushes[threadIndex]->a[i].u = u;
  g->pushes[threadIndex]->a[i].v = v;
  g->pushes[threadIndex]->a[i].edge_i = edge_index;
  g->pushes[threadIndex]->a[i].d = d;
}

static void add_relabel(graph_t* g, node_t* u, int threadIndex) {
  if (g->relabels[threadIndex]->i == g->relabels[threadIndex]->c) {
    node_t** b;
    g->relabels[threadIndex]->c *= 2; // double the capacity
    b = realloc(g->relabels[threadIndex]->a, g->relabels[threadIndex]->c * sizeof(g->relabels[threadIndex]->a[0]));
    if (b == NULL)
      error("no memory");
    g->relabels[threadIndex]->a = b;
  }

  g->relabels[threadIndex]->a[g->relabels[threadIndex]->i] = u;
  g->relabels[threadIndex]->i += 1;
}

static void add_work(graph_t* g, node_t* u) {
  if (g->workList->i == g->workList->c) {
    node_t** b;
    g->workList->c *= 2; // double the capacity
    b = realloc(g->workList->a, g->workList->c * sizeof(g->workList->a[0]));
    if (b == NULL)
      error("no memory");
    for(int i = g->workList->i; i < g->workList->c; i++){
      b[i] = NULL;
    }
    g->workList->a = b;
  }

  g->workList->a[g->workList->i] = u;
  g->workList->i+=1;
}

static void connect(node_t* u, node_t* v, int i)
{
	/* connect two nodes by putting a shared (same object)
	 * in their adjacency lists.
	 *
	 */

	//e->u = u;
	//e->v = v;
	//e->c = c;

//static void add_edge(node_t* u, node_t* v, int i, int b)
	add_edge(u, v, i, 1);
	add_edge(v, u, i, -1);
}

static graph_t* new_graph(FILE* in, int n, int m, int nThreads)
{
	graph_t*	g;
	node_t*		u;
	node_t*		v;
	int		i;
	int		a;
	int		b;
	int		c;

	g = xmalloc(sizeof(graph_t));

	g->n = n;
	g->m = m;
  g->done = 0;

	g->v = xcalloc(n, sizeof(node_t));
	//g->e = xcalloc(m, sizeof(edge_t));
	g->edge_data = xcalloc(m, sizeof(edge_data_t));

	g->s = &g->v[0];
	g->t = &g->v[n-1];
	g->excess = NULL;

	for (i = 0; i < n; i += 1) {
    g->v[i].edge.c = 2;
    g->v[i].edge.i = 0;
    g->v[i].edge.a = malloc(g->v[i].edge.c * sizeof(edge_t));
	}

	for (i = 0; i < m; i += 1) {
		a = next_int();
		b = next_int();
		c = next_int();
		u = &g->v[a];
		v = &g->v[b];
    g->edge_data[i].c = c;
    g->edge_data[i].f = 0;
//static void connect(node_t* u, node_t* v, int i)
		connect(u, v, i);
	}

  g->pushes = xcalloc(nThreads, sizeof(push_list_t*));
  for (int i = 0; i < nThreads; i++){
    g->pushes[i] = xmalloc(sizeof(push_list_t));
    g->pushes[i]->c = 8;
    g->pushes[i]->a = malloc(g->pushes[i]->c * sizeof(push_t));
    if(g->pushes[i]->a == NULL) error("no memory");
    g->pushes[i]->i = 0;
  }

  g->relabels = xcalloc(nThreads, sizeof(node_list_t*));
  for (int i = 0; i < nThreads; i++){
    g->relabels[i] = xmalloc(sizeof(node_list_t));
    g->relabels[i]->c = 8;
    g->relabels[i]->a = malloc(g->relabels[i]->c * sizeof(push_t));
    if(g->relabels[i]->a == NULL) error("no memory");
    g->relabels[i]->i = 0;
  }

  g->workList = xmalloc(sizeof(node_list_t));
  g->workList->c = 8;
  g->workList->a = malloc(c * sizeof(push_t));
  if(g->workList->a == NULL) error("no memory");
  g->workList->a = malloc(c * sizeof(push_t));
  g->workList->i = 0;

  if(pthread_barrier_init(&g->barrier, NULL, nThreads + 1) != 0) //nThreads+1 because of main thread
    error("g pthread_barrier_init failed");
  if(pthread_mutex_init(&g->mutex, NULL) != 0)
    error("g pthread_mutex_init failed");


	return g;
}

static void enter_excess(graph_t* g, node_t* v)
{
	/* put v at the front of the list of nodes
	 * that have excess preflow > 0.
	 *
	 * note that for the algorithm, this is just
	 * a set of nodes which has no order but putting it
	 * it first is simplest.
	 *
	 */

	if (v != g->t && v != g->s && !v->inExcess) {
    v->inExcess = 1;
		v->next = g->excess;
		g->excess = v;
	}
}

static node_t* leave_excess(graph_t* g)
{
	node_t*		v;

	/* take any node from the set of nodes with excess preflow
	 * and for simplicity we always take the first.
	 *
	 */

	v = g->excess;

	if (v != NULL) {
    v->inExcess = 0;
		g->excess = v->next;
    assert(v->e > 0);
  }

	return v;
}

static void push(graph_t* g, node_t* u, node_t* v, int edge_i, int d)
{
	//int		d;	/* remaining capacity of the edge. */

	pr("push from %d to %d: ", id(g, u), id(g, v));
	pr("f = %d, c = %d, so ", g->edge_data[edge_i].f, g->edge_data[edge_i].c);

	pr("pushing %d\n", d);

	//u->e -= d; //Move this to add_push
  pr("push changing excess node:%d, e=%d, d=%d\n", id(g,v), v->e, d);
	v->e += d;

	/* the following are always true. */

	assert(d > 0);
	assert(u->e >= 0 || u == g->s);
	assert(abs(g->edge_data[edge_i].f) <= g->edge_data[edge_i].c);

	if (u->e > 0) {

		/* still some remaining so let u push more. */

    enter_excess(g, u);
	}

	if (v->e == d) {

		/* since v has d excess now it had zero before and
		 * can now push.
		 *
		 */
    enter_excess(g, v);
	}
}

static void relabel(graph_t* g, node_t* u)
{
	u->h += 1;

	pr("relabel %d now h = %d\n", id(g, u), u->h);

  enter_excess(g, u);
}

//static node_t* other(node_t* u, edge_t* e)
//{
//	if (u == e->u)
//		return e->v;
//	else
//		return e->u;
//}

static int areWeDone(graph_t* g) {
  //node_t* source = g->s;
  //int sourceFlow = 0;
  //edge_t* e;
  ////list_t* p = source->edge;
  //while (p != NULL) {
  //  e = p->edge;
  //  p = p->next;
  //  if(source == e->u){
  //    sourceFlow += e->f;
  //  } else {
  //    sourceFlow -= e->f;
  //  }
  //}

  pr("s->e=%d\n", g->s->e);
  pr("t->e=%d\n", g->t->e);
  return -g->s->e == g->t->e;
}

static void* work(void* argsIn) {
  work_arg_t* args = (work_arg_t*) argsIn;
  graph_t* g       = args->g;
  int index        = args->index;
  int nThreads     = args->nThreads;
  node_t*    u;
  //node_t*    v;
  edge_t*    e;
  list_t*    p;
  int        b;
  int        d;
  int        hasPushed = 0;

  int        nodesProcessed = 0;
  while(!g->done){
    int numberOfWorks = (g->workList->i + (nThreads - 1))/nThreads + 1;
    int start = numberOfWorks*index;
    int end = numberOfWorks*(index+1);
    for(int i = start; i < end && i < g->workList->i; i++) {
      u = g->workList->a[i];

      /* u is any node with excess preflow. */

      pr("Thread %d takes node %d from excess list\n", index, id(g, u));
      pr("with h = %d and e = %d\n", u->h, u->e);
      assert(u->e > 0);

      /* if we can push we must push and only if we could
       * not push anything, we are allowed to relabel.
       *
       * we can push to multiple nodes if we wish but
       * here we just push once for simplicity.
       *
       */

      hasPushed = 0;
      //v = NULL;
      //p = u->edge;

      for(int i = 0; i < u->edge.i && u->e > 0; i++) {
        pr("Node %d checking edge %d\n", id(g,u), i);
        int edge_i = u->edge.a[i].i;
        edge_data_t e = g->edge_data[edge_i];
        //s->e += e->c;
        if (u->edge.a[i].b == 1) {
          d = MIN(u->e, e.c - e.f);
          //e.f += d;
        } else {
          d = MIN(u->e, e.c + e.f);
          //e.f -= d;
        }
        //push(g, s, s->edge.a[i]->v, edge_i, d);
        pr("u->h > u->edge.a[i].v->h = %d\n", u->h > u->edge.a[i].v->h);
        pr("u->edge.a[i].b * e.f < e.c = %d\n", u->edge.a[i].b * e.f < e.c);
        pr("u->edge.a[i].b = %d\ne.f = %d\ne.c = %d\n", u->edge.a[i].b, e.f, e.c);
        if (u->h > u->edge.a[i].v->h && u->edge.a[i].b * e.f < e.c) {
          hasPushed = 1;
          pr("Thread %d creates push, %d->%d\n", index, id(g,u), id(g,u->edge.a[i].v));
//static void add_push(graph_t* g, node_t* u, node_t* v, int d, int threadIndex)
          add_push(g, u, u->edge.a[i].v, edge_i, d, index);
          pr("Changing e.f by %d", u->edge.a[i].b * d);
          g->edge_data[edge_i].f += u->edge.a[i].b * d;
        }
          //v = NULL;
      }

      //while (p != NULL && u->e > 0) {
      //  e = p->edge;
      //  p = p->next;
      //  if (u == e->u) {
      //    v = e->v;
      //    b = 1;
      //  } else {
      //    v = e->u;
      //    b = -1;
      //  }

      //  if (u->h > v->h && b * e->f < e->c) {
      //    hasPushed = 1;
      //    pr("Thread %d creates push, %d->%d\n", index, id(g,u),id(g,v));
      //    add_push(g, u, v, e, index);
      //  } else
      //    v = NULL;
      //}
      nodesProcessed++;

      if(!hasPushed) {
        pr("Adding node %d to relabel list\n", id(g,u));
        add_relabel(g, u, index);
        //g->relabelLists[index] = add_node(g->relabelLists[index], u);
      }

      //if (v != NULL) {
      //  push(g, u, v, e);
      //} else
      //  relabel(g, u);
    }
    pr("Thread %d waiting at barrier 1\n", index);
    pthread_barrier_wait(&g->barrier); //Tell main thread our pushList is ready
    pr("Thread %d waiting at barrier 2\n", index);
    pthread_barrier_wait(&g->barrier); //Wait for main thread to finish processing
  }
  printf("Thread exited, %d nodes processed\n", nodesProcessed);
}

static void divideWork(graph_t* g, int nThreads) {
  node_t* u;
  g->workList->i = 0;
  while ((u = leave_excess(g)) != NULL) {
    assert(u->e > 0);
    pr("Add node %d to workList with e=%d\n", id(g,u), u->e);
    add_work(g, u);
  }
}
//int preflow(int n, int m, int s, int t, xedge_t* e)
static int preflow(graph_t* g, int nThreads)
{
	node_t*		s;
	node_t*		u;
	node_t*		v;
	edge_t*		e;
	list_t*		p;
	int		b;

	s = g->s;
	s->h = g->n;

	//p = s->edge;

	/* start by pushing as much as possible (limited by
	 * the edge capacity) from the source to its neighbors.
	 *
	 */

  //initial pushes
  int d;
  for(int i = 0; i < s->edge.i; i++) {
    int edge_i = s->edge.a[i].i;
    edge_data_t e = g->edge_data[edge_i];
		s->e -= e.c;
    if (s->edge.a[i].b == 1) {
      d = e.c;
      e.f += d;
    } else {
      d = e.c;
      e.f -= d;
    }
		push(g, s, s->edge.a[i].v, edge_i, d);
  }
	//while (p != NULL) {
	//	e = p->edge;
	//	p = p->next;

	//	s->e += e->c;
  //  if (s == e->u) {
  //    d = MIN(s->e, e->c - e->f);
  //    e->f += d;
  //  } else {
  //    d = MIN(s->e, e->c + e->f);
  //    e->f -= d;
  //  }
	//	push(g, s, other(s, e), e, d);
	//}

  divideWork(g, nThreads);

  work_arg_t* args = xcalloc(nThreads, sizeof(work_arg_t));

  // Create n threads
  pthread_t* thread = (pthread_t*) malloc(nThreads * sizeof(pthread_t));
  for (int i = 0; i < nThreads; i++){
    args[i].index = i;
    args[i].g = g;
    args[i].nThreads = nThreads;
    if (pthread_create(&thread[i], NULL, work, (void*) &args[i]) != 0)
      error("pthread_create failed");
  }

  while(!g->done) {
    pthread_barrier_wait(&g->barrier); //Wait for threads to finish their pushlists
    for(int i = 0; i < nThreads; i++) {
      for(int j = 0; j < g->pushes[i]->i; j++){
        push_t p = g->pushes[i]->a[j];
//static void push(graph_t* g, node_t* u, node_t* v, int edge_i, int d)
        push(g, p.u, p.v, p.edge_i, p.d);
      }
      g->pushes[i]->i = 0;
    }
    for(int i = 0; i < nThreads; i++) {
      for(int j = 0; j < g->relabels[i]->i; j++){
        relabel(g, g->relabels[i]->a[j]);
      }
      g->relabels[i]->i = 0;
    }
    g->done = areWeDone(g);
    divideWork(g, nThreads);
    pthread_barrier_wait(&g->barrier); // Let threads start making new pushlists
  }

  pr("Program done!");

  // Kill threads
  for (int i = 0; i < nThreads; i++){
    pthread_join(thread[i], NULL);
    //if (pthread_exit(&thread[i], NULL, work, (void*) &args[i]) != 0)
    //  error("pthread_create failed");
  }

	return g->t->e;
}

static void free_graph(graph_t* g)
{
	int		i;
	list_t*		p;
	list_t*		q;

	for (i = 0; i < g->n; i += 1) {
		//p = g->v[i].edge;
		//while (p != NULL) {
		//	q = p->next;
		//	free(p);
		//	p = q;
		//}
	}
	free(g->v);
	free(g->e);
	free(g);
}

int main(int argc, char* argv[])
{
	FILE*		in;	/* input file set to stdin	*/
	graph_t*	g;	/* undirected graph. 		*/
	int		f;	/* output from preflow.		*/
	int		n;	/* number of nodes.		*/
	int		m;	/* number of edges.		*/
  int   nThreads = 4;

	progname = argv[0];	/* name is a string in argv[0]. */

	in = stdin;		/* same as System.in in Java.	*/

	n = next_int();
	m = next_int();

	/* skip C and P from the 6railwayplanning lab in EDAF05 */
	next_int();
	next_int();

	g = new_graph(in, n, m, nThreads);

	fclose(in);

	f = preflow(g, nThreads);

	printf("f = %d\n", f);

	free_graph(g);

	return 0;
}

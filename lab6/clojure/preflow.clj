(require '[clojure.string :as str])		; for splitting an input line into words

(def debug false)
(def num-threads 4)

(def print-lock (Object.))
(defn p [& args]
    (if debug (locking print-lock
        (apply println (.getName (Thread/currentThread)) ":" args))))

(defn prepend [list value] (cons value list))	; put value at the front of list

(defrecord node [i e h adj])			; index excess-preflow height adjacency-list

(defn node-adj [u] (:adj u))			; get the adjacency-list of a node
(defn node-height [u] (:h u))			; get the height of a node
(defn node-index [u] (:i u))			; get the height of a node
(defn node-excess [u] (:e u))			; get the excess-preflow of a node

(defn has-excess [u nodes]
	(> (node-excess @(nodes u)) 0))

(defrecord edge [u v f c])			; one-node another-node flow capacity
(defn edge-flow [e] (:f e))			; get the current flow on an edge
(defn edge-capacity [e] (:c e))			; get the capacity of an edge

; read the m edges with the normal format "u v c"
(defn read-graph [i m nodes edges]
	(if (< i m)
		(do	(let [line 	(read-line)]
			(let [words	(str/split line #" ") ]

			(let [u		(Integer/parseInt (first words))]
			(let [v 	(Integer/parseInt (first (rest words)))]
			(let [c 	(Integer/parseInt (first (rest (rest words))))]

			(ref-set (edges i) (update @(edges i) :u + u))
			(ref-set (edges i) (update @(edges i) :v + v))
			(ref-set (edges i) (update @(edges i) :c + c))

			(ref-set (nodes u) (update @(nodes u) :adj prepend i))
			(ref-set (nodes v) (update @(nodes v) :adj prepend i)))))))

			; read remaining edges
			(recur (+ i 1) m nodes edges))))

(defn other [edge u]
	(if (= (:u edge) u) (:v edge) (:u edge)))

(defn u-is-edge-u [edge u]
	(= (:u edge) u))

(defn increase-flow [edges i d]
	(ref-set (edges i) (update @(edges i) :f + d)))

(defn decrease-flow [edges i d]
	(ref-set (edges i) (update @(edges i) :f - d)))

(defn move-excess [nodes u v d]
	(ref-set (nodes u) (update @(nodes u) :e - d))
	(ref-set (nodes v) (update @(nodes v) :e + d)))

(defn insert [excess-nodes v] (do
  (p "Inserting node" v "to excess list")
	(ref-set excess-nodes (cons v @excess-nodes))))

(defn check-insert [excess-nodes v s t]
	(if (and (not= v s) (not= v t))
		(insert excess-nodes v)
  )
)

(defn relabel [nodes u] (do
    (p "Relabeling node" u)
    (ref-set (nodes u) (update @(nodes u) :h + 1))
))

(defn push [edge-index u nodes edges excess-nodes change s t]
	(let [v 	(other @(edges edge-index) u)]
	(let [uh	(node-height @(nodes u))]
	(let [vh	(node-height @(nodes v))]
	(let [e 	(node-excess @(nodes u))]
	(let [i		edge-index]
	(let [f 	(edge-flow @(edges i))]
	(let [c 	(edge-capacity @(edges i))]
  (let [edge @(edges edge-index)]
  (let [d (if (u-is-edge-u edge u)
          (min e (- (:c edge)  (:f edge)))
          (min e (+ (:c edge)  (:f edge)))
          )]
  (if (and (> d 0) (> uh vh))
  (do
    (p "--------- push -------------------")
    (p "i = " i)
    (p "u = " u)
    (p "uh = " uh)
    (p "e = " e)
    (p "f = " f)
    (p "c = " c)
    (p "v = " v)
    (p "vh = " vh)
    (p "d = " d)
    (ref-set change true)
    (p "set change to" @change)
    (if (u-is-edge-u edge u) (
        increase-flow edges i d
        ;ref-set (edges edge-index) (update edge :f + d)
      ) ( ;else
        decrease-flow edges i d
        ;ref-set (edges edge-index) (update edge :f - d)
      ))
    (move-excess nodes u v d)
    ;(ref-set (nodes u) (update @(nodes u) :e - d))
    ;(ref-set (nodes v) (update @(nodes v) :e + d))
    (if (= (:e @(nodes v)) d) (check-insert excess-nodes v s t))
	))))))))))))

		;(ref-set (nodes) (update @(nodes u) :e - d))
		;(ref-set (nodes) (update @(nodes v) :e + d))


; go through adjacency-list of source and push
(defn initial-push [adj s t nodes edges excess-nodes]
  (p "enter initial-push")
	(let [change (ref 0)] ; unused for initial pushes since we know they will be performed
    (if (not (empty? adj))
      (let [c (:c @(edges (first adj)))]
        (do
          (p "Increasing source.e by" c)
          (ref-set (nodes 0) (update @(nodes 0) :e + c))
          (push (first adj) s nodes edges excess-nodes change s t)
          (initial-push (rest adj) s t nodes edges excess-nodes)
        )
      )
      (p "adj empty")
    )
  )
  (p "exit initial-push")
)

(defn initial-pushes [nodes edges s t excess-nodes]
  (p "enter initial-pushes")
	(initial-push (node-adj @(nodes s)) s t nodes edges excess-nodes)
  (p "exit initial-pushes")
)

(defn remove-any [excess-nodes]
	(dosync
		(let [ u (ref -1)]
			(do
				(if (not (empty? @excess-nodes))
					(do
						(ref-set u (first @excess-nodes))
						(ref-set excess-nodes (rest @excess-nodes))))
			@u))))

; read first line with n m c p from stdin

(def line (read-line))

; split it into words
(def words (str/split line #" "))

(def n (Integer/parseInt (first words)))
(def m (Integer/parseInt (first (rest words))))

(def s 0)
(def t (- n 1))
(def excess-nodes (ref ()))

(def nodes (vec (for [i (range n)] (ref (->node i 0 (if (= i 0) n 0) '())))))

(def edges (vec (for [i (range m)] (ref (->edge 0 0 0 0)))))

(dosync (read-graph 0 m nodes edges))

; go through adjacency-list of u and push
(defn do-pushes [adj u s t nodes edges excess-nodes change]
  (p "enter do-pushes")
    (if (not (empty? adj))
      (do
        (push (first adj) u nodes edges excess-nodes change s t)
        (recur (rest adj) u s t nodes edges excess-nodes change)
      )
    )
)

(defn work [nodes edges s t excess-nodes] (do
  (p "Enter work")
  (let [u (remove-any excess-nodes)]
	(let [change (ref false)]
  (if (not= u -1) ( dosync
      (p "Removed node" u "from excess-nodes with e=" (:e @(nodes u)) )
      (do-pushes (node-adj @(nodes u)) u s t nodes edges excess-nodes change)
      (if (not @change) ( do
        (p "relabel node" u)
        (relabel nodes u)
        (insert excess-nodes u)
      ) ( do
        (p "node" 1 "has changed, e:" (:e @(nodes u)))
        (if (> (:e @(nodes u)) 0) (insert excess-nodes u))
      ))
    )
  )
  (if (not= u -1) (recur nodes edges s t excess-nodes))
  ))))

(defn workWrapper []
  (work nodes edges s t excess-nodes)
)

(defn preflow []
	(dosync (initial-pushes nodes edges s t excess-nodes))
  (p "hello")
  (let [threads (repeatedly num-threads #(Thread. workWrapper))]
		(run! #(.start %) threads)
		(run! #(.join %) threads))
	(println "f =" (node-excess @(nodes t)))
)

(preflow)

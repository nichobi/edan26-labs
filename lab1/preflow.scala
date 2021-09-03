import scala.util._
import java.util.Scanner
import java.io._
import akka.actor._
import akka.pattern.ask
import akka.util.Timeout
import scala.concurrent.{Await, ExecutionContext, Future, Promise}
import scala.concurrent.duration._
import scala.language.postfixOps
import scala.io._

case class Flow(f: Int)
case class Debug(debug: Boolean)
case class Control(control: ActorRef)
case class Source(n: Int)
case class AskToPush(excess: Int, height: Int, sender: ActorRef)
case class PushResponse(accepted: Boolean, responseTo: AskToPush)

case object Print
case object Start
case object Excess
case object Maxflow
case object Sink
case object Hello

class Edge(var u: ActorRef, var v: ActorRef, var c: Int) {
  var f = 0
}

class Node(val index: Int) extends Actor {
  var excess            = 0;    // excess preflow.
  var height            = 0;    // height.
  var control: ActorRef = null  // controller to report to when e is zero.
  var source: Boolean   = false // true if we are the source.
  var sink: Boolean     = false // true if we are the sink.
  var edges: List[Edge] = Nil   // adjacency list with edge objects shared with other nodes.
  var debug             = false // to enable printing.
  var lastSentTo: Int = 0  // Index of last node we tried to push to

  def min(a: Int, b: Int): Int = if (a < b) a else b

  def id: String = "@" + index;

  def other(a: Edge, u: ActorRef): ActorRef = if (u == a.u) a.v else a.u

  def status: Unit =
    if (debug) println(id + " e = " + excess + ", h = " + height)

  def enter(func: String): Unit =
    if (debug) {
      println(id + " enters " + func); status
    }
  def exit(func: String): Unit =
    if (debug) {
      println(id + " exits " + func); status
    }

  def relabel: Unit = {

    enter("relabel")

    height += 1

    exit("relabel")
  }

  def receive = {

    case Debug(debug: Boolean) => this.debug = debug

    case Print => status

    case Excess => {
      sender ! Flow(
        excess
      ) // send our current excess preflow to actor that asked for it.
    }

    case edge: Edge => {
      this.edges =
        edge :: this.edges // put this edge first in the adjacency-list.
    }

    case Control(control: ActorRef) => this.control = control

    case Sink => { sink = true }

    case Source(n: Int) => { height = n; source = true }

    case AskToPush(excess: Int, height: Int, sender: ActorRef) =>
      if (this.height < height) {
        sender ! PushResponse(true, AskToPush(excess, height, sender))
        this.excess += excess
        for (edge <- edges) {
          if (excess > 0) {
            val otherNode = other(edge, self)
            val toSend    = min(excess, edge.c)
            otherNode ! AskToPush(toSend, this.height, self)
            this.excess -= toSend
          }
        }
      }

    case PushResponse(accepted: Boolean, responseTo: AskToPush) =>
      if (!accepted) this.excess += excess

    case m => {
      println(s"$index received an unknown message: $m")
    }

    assert(false)
  }

  def edgesToSendTo: List[Edge]  = {
    val (previous, next) = edges.splitAt(lastSentTo)
    return next ++ previous
  }

  def sendToEveryone = for (edge <- edgesToSendTo) {
    if (excess > 0) {
      val otherNode = other(edge, self)
      val toSend    = min(excess, edge.c)
      otherNode ! AskToPush(toSend, this.height, self)
      this.excess -= toSend
      lastSentTo = edges.indexOf(edge)
    }
  }

}

class Preflow extends Actor {
  var sourceIndex            = 0;   // index of source node.
  var sinkIndex              = 0;   // index of sink node.
  var vertexCount            = 0;   // number of vertices in the graph.
  var edges: Array[Edge]     = null // edges in the graph.
  var nodes: Array[ActorRef] = null // vertices in the graph.
  var ret: ActorRef          = null // Actor to send result to.

  def receive = {

    case node: Array[ActorRef] => {
      this.nodes = nodes
      vertexCount = nodes.size
      sourceIndex = 0
      sinkIndex = vertexCount - 1
      for (u <- node)
        u ! Control(self)
    }

    case edges: Array[Edge] => this.edges = edges

    case Flow(f: Int) => {
      ret ! f // somebody (hopefully the sink) told us its current excess preflow.
    }

    case Maxflow => {
      ret = sender

      nodes(
        sinkIndex
      ) ! Excess // ask sink for its excess preflow (which certainly still is zero).
    }
  }
}

object main extends App {
  implicit val t = Timeout(4 seconds);

  val begin   = System.currentTimeMillis()
  val system  = ActorSystem("Main")
  val control = system.actorOf(Props[Preflow], name = "control")

  var nodeCount              = 0;
  var edgeCount              = 0;
  var edges: Array[Edge]     = null
  var nodes: Array[ActorRef] = null

  val scanner = new Scanner(System.in);

  nodeCount = scanner.nextInt
  edgeCount = scanner.nextInt

  // next ignore c and p from 6railwayplanning
  scanner.nextInt
  scanner.nextInt

  nodes = new Array[ActorRef](nodeCount)

  for (i <- 0 to nodeCount - 1)
    nodes(i) = system.actorOf(Props(new Node(i)), name = "v" + i)

  edges = new Array[Edge](edgeCount)

  for (i <- 0 to edgeCount - 1) {

    val u        = scanner.nextInt
    val v        = scanner.nextInt
    val capacity = scanner.nextInt

    edges(i) = new Edge(nodes(u), nodes(v), capacity)

    nodes(u) ! edges(i)
    nodes(v) ! edges(i)
  }

  control ! nodes
  control ! edges

  val flow = control ? Maxflow
  val f    = Await.result(flow, t.duration)

  println(s"f = $f")

  system.stop(control);
  system.terminate()

  val end = System.currentTimeMillis()

  println(s"t = ${(end - begin) / 1000.0} s")
}

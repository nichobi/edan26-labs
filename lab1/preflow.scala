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
case class PushRequest(excess: Int, height: Int, sender: ActorRef, edge: Edge)
case class PushResponse(accepted: Boolean, responseTo: PushRequest)
//case class NodeActivity(active: Boolean)
case class SinkFlowUpdate(f: Int)
case class SourceFlowUpdate(f: Int)

case object Print
case object Start
case object Excess
case object Maxflow
case object Sink
case object Hello

class Edge(var u: ActorRef, var v: ActorRef, var c: Int) {
  var f = 0

  def freeCapacities: (Int, Int) = (c - f, c + f)

  def min(a: Int, b: Int): Int = if (a < b) a else b

  def pushFrom(sendingNode: ActorRef, excess: Int): Int =
    if (sendingNode == u) min(excess, freeCapacities._1)
    else min(excess, freeCapacities._2)

  def increaseF(receivingNode: ActorRef, fIncrease: Int) = {
    assert(fIncrease > 0, s"fIncrease = $fIncrease, ${receivingNode.toString}")
    if (receivingNode == v) {
      assert(
        fIncrease <= freeCapacities._1,
        s"Trying to increase $fIncrease but ${freeCapacities._1} free"
      )
      f += fIncrease
    } else {
      assert(fIncrease <= freeCapacities._2, s"$fIncrease, $freeCapacities")
      f -= fIncrease
    }
  }
}

class Node(val index: Int) extends Actor {
  var excess            = 0; // excess preflow.
  var height            = 0; // height.
  var control: ActorRef = null // controller to report to when e is zero.
  var source: Boolean   = false // true if we are the source.
  var sink: Boolean     = false // true if we are the sink.
  var edges: List[Edge] = Nil // adjacency list with edge objects shared with other nodes.
  var debug             = true // to enable printing.
  var pushesSent        = 0 // Index of last node we tried to push to
  var repliesReceived   = 0
  var pushRejected      = false

  def min(a: Int, b: Int): Int = if (a < b) a else b

  def id: String = "@" + index;

  def other(a: Edge, u: ActorRef): ActorRef = if (u == a.u) a.v else a.u

  def flowDirection(a: Edge, u: ActorRef): Int = if (u == a.u) 1 else -1

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

    if (!sink) height += 1

    exit("relabel")
  }

  def changeExcess(n: Int) = {
    enter("changeExcess")
    excess += n
    if (source) control ! SourceFlowUpdate(calculateFlow)
    else if (sink) control ! SinkFlowUpdate(excess)
    exit("changeExcess")
  }

  def calculateFlow = {
    enter("calculateFlow")
    (for (edge <- edges) yield {
      if (edge.u == self) edge.f
      else -edge.f
    }).sum
  }

  def receive = {
    case any =>
      println(s"$id received: $any")
      receive2(any)
  }

  def receive2: PartialFunction[Any, Unit] = {

    case Debug(debug: Boolean) => this.debug = debug

    case Print => status

    case Excess => {
      sender ! Flow(
        excess
      ) // send our current excess preflow to actor that asked for it.
    }

    case Start => {
      println("We are starting")
      assert(source)
      for (edge <- edges) {
        val otherNode = other(edge, self)
        otherNode ! PushRequest(edge.c, height, self, edge)

      }
    }

    case edge: Edge => {
      this.edges =
        edge :: this.edges // put this edge first in the adjacency-list.
    }

    case Control(control: ActorRef) => this.control = control

    case Sink => { sink = true }

    case Source(n: Int) => { height = n; source = true }

    case PushRequest(excess: Int, height: Int, sender: ActorRef, edge: Edge) =>
      enter("Recevied PushRequest")
      if (this.height < height) {
        changeExcess(excess)
        edge.increaseF(self, excess)
        sender ! PushResponse(true, PushRequest(excess, height, sender, edge))
        pushToEveryone
      } else {
        sender ! PushResponse(false, PushRequest(excess, height, sender, edge))
      }
      exit("Received PushRequest")

    case PushResponse(accepted: Boolean, responseTo: PushRequest) =>
      enter("PushResponse")
      if (!accepted) {
        changeExcess(responseTo.excess)
        pushRejected = true
        pushToEveryone
      }
      exit("PushResponse")

    case m => {
      println(s"$index received an unknown message: $m")
    }

    assert(false)
  }

  def edgesToSendTo: Seq[Int] = {
    val (previous, next) = edges.indices.splitAt(pushesSent)
    return next ++ previous
  }

  def pushToEveryone: Unit = {
    enter("pushToEveryone")
    if (!sink && !source) {
      if (pushesSent < edges.size) {
        for (edgeIndex <- edgesToSendTo) {
          if (excess > 0) {
            val edge      = edges(edgeIndex)
            val otherNode = other(edge, self)
            val toSend    = edge.pushFrom(self, excess)
            if (toSend > 0) otherNode ! PushRequest(toSend, this.height, self, edge)
            this.excess -= toSend
            pushesSent += 1
          }
        }
      } else if (pushRejected) {
        relabel
        pushesSent = 0
        repliesReceived = 0
        pushRejected = false
        pushToEveryone
      }
    }
    exit("pushToEveryone")
  }
}

class Controller extends Actor {
  var sourceIndex            = 0; // index of source node.
  var sinkIndex              = 0; // index of sink node.
  var vertexCount            = 0; // number of vertices in the graph.
  var edges: Array[Edge]     = null // edges in the graph.
  var nodes: Array[ActorRef] = null // vertices in the graph.
  var ret: ActorRef          = null // Actor to send result to.
  var nodesActive            = 0
  var sourceFlow             = 0
  var sinkFlow               = 0

  def receive = {

    case nodes: Array[ActorRef] => {
      this.nodes = nodes
      vertexCount = nodes.size
      sourceIndex = 0
      sinkIndex = vertexCount - 1
      for (u <- nodes)
        u ! Control(self)
      nodes(sourceIndex) ! Source(nodes.size)
      nodes(sinkIndex) ! Sink
    }

    case edges: Array[Edge] => this.edges = edges

    case Flow(f: Int) => {
      //ret ! f // somebody (hopefully the sink) told us its current excess preflow.
    }

    case Maxflow => {
      ret = sender
      nodes(sourceIndex) ! Start
      //nodes(sinkIndex) ! Excess // ask sink for its excess preflow (which certainly still is zero).
    }

    case SourceFlowUpdate(f) => {
      sourceFlow = f
      println(s"SourceFlowUpdate: source=$sourceFlow, sink=$sinkFlow")
      if (sourceFlow == sinkFlow) endProgram
    }

    case SinkFlowUpdate(f) => {
      sinkFlow = f
      println(s"SinkFlowUpdate: source=$sourceFlow, sink=$sinkFlow")
      if (sourceFlow == sinkFlow) endProgram
    }
  }

  def endProgram =
    ret ! sinkFlow
}

object main extends App {
  implicit val t = Timeout(300 seconds);

  val begin   = System.currentTimeMillis()
  val system  = ActorSystem("Main")
  val control = system.actorOf(Props[Controller], name = "control")

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

RayPlatform is a development framework to ease the creation of 
massively distributed high performance computing applications.

RayPlatform is free software distributed under the terms of the GNU
Lesser General Public License, version 3 (LGPLv3).

Content creation is done by creating plugins that can be 
added on the RayPlatform compute engine.

It uses the message-passing interface for interprocess communication, but this is
transparent to the developer.


## Models

Mainly two models are available:

- Rank model (with RayPlatform Plugin API)
- Actor Model (with the RayPlatform Actor Playground API)

Also: RayPlatform implements a "mini-ranks" programming model. It is a hybrid
programming model where MPI ranks and mini-ranks exist. Mini-ranks run in separate
threads inside a MPI rank process. This eases the usage of a superior hybrid
model with MPI and threads (pthread). But the actor model is superior in every aspect.


## Illustration of the RayPlatform Plugin API

```
+--------------------------------------------------------------------------+
|                                                                          |
|                                Application                               |
|                                                                          |
+------------------------+------------------------+------------------------+
|                        |                        |                        |
|         Plugin         |         Plugin         |         Plugin         |
|                        |                        |                        |
+---------+---------+    +---------+---------+    +---------+---------+    +
|         |         |    |         |         |    |         |         |    |
| Adapter | Adapter |    | Adapter | Adapter |    | Adapter | Adapter |    |
|         |         |    |         |         |    |         |         |    |
+---------+---------+----+---------+---------+----+---------+---------+----+
|                                                                          |
|                                RayPlatform                               |
|                                                                          |
+--------------------------------------------------------------------------+
|                                                                          |
|                        Message Passing Interface                         |
|                                                                          |
+--------------------------------------------------------------------------+
```

## Projects using RayPlatform 

- The Ray genome assembler (and also Ray Meta, Ray Communities, Ray Surveyor)
	http://github.com/sebhtml/ray
- RayPlatform example
	http://github.com/sebhtml/RayPlatform-example
- MessageWarden
	http://github.com/sebhtml/MessageWarden

## Description 

There is some documentation in Documentation/

You can also generate doxygen documentation.

The framework provides facilities for parallel software architecture,
communication, memory management, profiling, thread pools 
and some structures.

It can be compiled as libRayPlatform.a, libRayPlatform.so or libRayPlatform.dll
using GNU Make or CMake.


### Parallel software architecture:

- a compute core implementation in which the main loop lives (core/ComputeCore.h);
- a system of plugins that can be registered with the ComputeCore (core/CorePlugin.h);
- a system of callbacks for message tags (handlers/MessageTagHandler.h);
- a system of callbacks for master modes (handlers/MasterModeHandler.h);
- a system of callbacks for slave modes (handlers/SlaveModeHandler.h);
- a system for master switches (scheduling/Switchman.h)
- a system for slave switches (scheduling/Switchman.h)

### Communication:

- a message inbox and outbox (structures/StaticVector.h);
- a virtual communicator for automated message aggregation (communication/VirtualCommunicator.h);
- a buffered data object for less-automated message aggregation (communication/BufferedData.h);
- a message router (including various graphs such as de Bruijn) 
  for jobs running on numerous cores (> 1000) (communication/MessageRouter.h and routing/*);
- a wrapper on top of the provided MPI library (communication/MessagesHandler.h).

### Memory management:

- a ring allocator (memory/RingAllocator.h);
- a "chunk" allocator (memory/MyAllocator.h);
- an allocator with real-time defragmentation (memory/ChunkAllocatorWithDefragmentation.h).

### Profiling:

- a profiler that reports granularity (profiling/Profiler.h);
- a tick counter for slave and master modes (profiling/TickLogger.h);

### Thread pools:

- a virtual processor containing a lot of workers (scheduling/VirtualProcessor.h);
- a general interface to define a worker (scheduling/Worker.h);
- a general way to use the virtual processor (scheduling/TaskCreator.h).

### Structures:

- very space-efficient sparse hash table (structures/MyHashTable.h);
- a splay tree (structures/SplayTree.h).

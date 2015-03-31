# mtest

A simple, fast memory validator that checks the RAM for a server.

When the server is NUMA, each NUMA node is tested independently and in
parallel using all of the CPUs in the NUMA node. When the server is UMA
then all of the CPUs are used.

Testing is done by allocating all of the RAM for a NUMA node, locking it
in RAM (to prevent swapping), writing patterns to the RAM, then reading
the patterns and checking they are correct.

## Options

*  -n node_number - zero index'd node to test, default is to test all
   nodes

*  -c continuous operation, never stop testing

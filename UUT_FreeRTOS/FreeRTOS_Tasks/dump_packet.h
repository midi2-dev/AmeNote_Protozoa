#pragma once

#include <midi/universal_packet.h>
#include <cstdio>

inline void dump_packet(const char *what, const midi::universal_packet &p)
{
  switch (p.size())
  {
  case 1:
    printf("%s 0x%08x\n", what, p.data[0]);
    break;
  case 2:
    printf("%s (0x%08x, 0x%08x)\n", what, p.data[0], p.data[1]);
    break;
  case 4:
    printf("%s (0x%08x, 0x%08x, 0x%08x, 0x%08x)\n", what, p.data[0], p.data[1], p.data[2], p.data[3]);
    break;
  }
}

#if PROTOZOA_TRACE_INCOMING_TRAFFIC
# define TRACE_INCOMING_PACKET(what, packet) dump_packet(what, packet)
#else
# define TRACE_INCOMING_PACKET(what, packet) 
#endif

#if PROTOZOA_TRACE_OUTGOING_TRAFFIC
# define TRACE_OUTGOING_PACKET(what, packet) dump_packet(what, packet)
#else
# define TRACE_OUTGOING_PACKET(what, packet) 
#endif

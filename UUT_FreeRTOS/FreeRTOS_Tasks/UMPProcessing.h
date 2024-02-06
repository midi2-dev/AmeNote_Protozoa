#ifndef UMPPROCESSING_H
#define UMPPROCESSING_H

#include <midi/capability_inquiry.h>
#include <midi/stream_message.h>
#include <midi/sysex_collector.h>
#include <midi/universal_packet.h>

#include <string>
#include <string_view>

class UMPProcessing
{
public:
  typedef void sendPacketProc(const midi::universal_packet&);

  UMPProcessing(std::string_view epName, sendPacketProc);

  midi::protocol_t   curProtocol { midi::protocol::midi1 };
  midi::extensions_t curExtensions { 0 };
  
  void process(const midi::universal_packet&);
  void sendPendingUMPs();
  void clearPendingUMPs();
  
protected:
  void processStreamMessage(const midi::universal_packet&);
  void processEndpointDiscovery(const midi::endpoint_discovery_view&);
  void processFunctionBlockDiscovery(const midi::function_block_discovery_view&);
  void processStreamConfigurationRequest(const midi::stream_configuration_view&);

  void processSysexMessage(const midi::sysex7&);
  void processMIDICIMessage(const midi::capability_inquiry_view&);
  void processMIDICIGetProperty(const midi::ci::get_property_data_view&);

  void sendSysex(const midi::sysex7&, midi::group_t = 0);

private:
  static constexpr size_t maxSysexMessageSize { 512 };

  std::string m_endpointName;
  sendPacketProc *sendPacket = nullptr;
  uint16_t m_controlReadPtr { 0 };
  uint16_t m_DINReadPtr { 0 };
  uint16_t m_BTReadPtr { 0 };
  midi::muid_t m_muid;
  midi::sysex7_collector m_ciMain;
};

#endif // UMPPROCESSING_H
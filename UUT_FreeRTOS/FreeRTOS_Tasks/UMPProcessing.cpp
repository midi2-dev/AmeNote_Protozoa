#include "UMPProcessing.h"

#include "CMEWidiTask.h"
#include "DINSerialTask.h"
#include "FreeRTOS_Tasks.h"
#include "PicoMainTask.h"
#include "PEHeaderParser.h"

#include "pico/unique_id.h"

#include <midi/channel_voice_message.h>
#include <midi/midi1_byte_stream.h>
#include <midi/stream_message.h>

#include <cstring>

constexpr auto my_identity = midi::device_identity { 0x7D, 0, 0, 1 };
constexpr std::string_view my_ResourceList {
R"([
  {"resource":"DeviceInfo"},
  {"resource":"ChannelList"}
])" };

constexpr std::string_view my_DeviceInfo {
R"({
  "manufacturerId": [ 125 ],
  "familyId": [ 0, 0 ],
  "modelId": [ 0, 0 ],
  "versionId": [ 1, 0, 0, 0 ],
  "manufacturer": "ProtoZOA",
  "family": "UUT_FreeRTOS_TASKS",
  "model": "-",
  "version": "0.0.1"
})" };

constexpr std::string_view my_ChannelList {
R"([
{
  "title":"ch1",
  "channel":1
}
])" };

static uint32_t random(uint32_t max)
{
    return (rand() % static_cast<uint32_t>(max + 1));
}

UMPProcessing::UMPProcessing(std::string_view epName, sendPacketProc s) :
    m_endpointName(epName),
    sendPacket(s),
    m_muid(random(0xFFFFEFF)),
    m_ciMain([this](const midi::sysex7 &sx) { this->processSysexMessage(sx); })
{
    m_ciMain.set_max_sysex_data_size(maxSysexMessageSize-1);
}

void UMPProcessing::process(const midi::universal_packet &p)
{
    switch (p.type())
    {
    case midi::packet_type::utility:
        // TODO: JR timestamps
        return;
    case midi::packet_type::stream:
        processStreamMessage(p);
        return;
    default:
        break;
    }

    switch (p.group())
    {
    case MainGroup:
        switch (p.type())
        {
        case midi::packet_type::data:
            if (midi::is_sysex7_packet(p))
            {
                m_ciMain.feed(p);
            }
            break;
        default:
            break;
        }
        return;
    case DINPortsGroup:
        DINPortSendBuffer.write(p);
        return;
#if PROTOZOA_EXPANSION_CME_WIDI_CORE
    case CMEWidiGroup:
        CMEWidiSendBuffer.write(p);
        return;
#endif
    default:
        break;
    }
}

void UMPProcessing::sendPendingUMPs()
{
    midi::universal_packet p;

    // Read DIN port
    while (DINPortReceiveBuffer.read(m_DINReadPtr, p))
    {
        sendPacket(p);
    }

#if PROTOZOA_EXPANSION_CME_WIDI_CORE
    // Read BT port
    while (CMEWidiReceiveBuffer.read(m_BTReadPtr, p))
    {
        sendPacket(p);
    }
#endif

    // Read control events
    while (ControlMessageBuffer.read(m_controlReadPtr, p))
    {
        switch (curProtocol)
        {
        case midi::protocol::midi1:
            if (p.type() == midi::packet_type::midi2_channel_voice)
            {
                if (auto m = midi::as_midi1_channel_voice_message(midi::midi2_channel_voice_message_view{ p }))
                {
                    sendPacket(*m);
                }
                continue;
            }
            break;
        case midi::protocol::midi2:
            if (p.type() == midi::packet_type::midi1_channel_voice)
            {
                if (auto m = midi::as_midi2_channel_voice_message(midi::midi1_channel_voice_message_view{ p }))
                {
                    sendPacket(*m);
                }
                continue;
            }
            break;
        }

        sendPacket(p);
    }
}

void UMPProcessing::clearPendingUMPs()
{
    DINPortReceiveBuffer.resetReadPtr(m_DINReadPtr);
#if PROTOZOA_EXPANSION_CME_WIDI_CORE
    CMEWidiReceiveBuffer.resetReadPtr(m_BTReadPtr);
#endif
    ControlMessageBuffer.resetReadPtr(m_controlReadPtr);
}

void UMPProcessing::processStreamMessage(const midi::universal_packet &p)
{
    switch (p.status())
    {
    case midi::stream_status::endpoint_discovery:
        processEndpointDiscovery(midi::endpoint_discovery_view(p));
        break;
    case midi::stream_status::function_block_discovery:
        processFunctionBlockDiscovery(midi::function_block_discovery_view(p));
        break;
    case midi::stream_status::stream_configuration_request:
        processStreamConfigurationRequest(midi::stream_configuration_view(p));
        break;
    }
}

void UMPProcessing::processEndpointDiscovery(const midi::endpoint_discovery_view &m)
{
    printf("endpoint_discovery(%04X - %02X)\n", (int)m.ump_version(), (int)m.filter());

    if (m.requests_info())
    {
        constexpr auto endpoint_info = midi::make_endpoint_info_message(3, true, midi::protocol::midi1 + midi::protocol::midi2, 0);
        sendPacket(endpoint_info);
    }

    if (m.requests_device_identity())
    {
        constexpr auto device_identity = midi::make_device_identity_message(my_identity);
        sendPacket(device_identity);
    }

    if (m.requests_name())
    {
        midi::send_endpoint_name(m_endpointName, sendPacket);
    }

    if (m.requests_product_instance_id())
    {
        const static std::string_view product_instance_id = []() {
            constexpr size_t strLength = 2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1;
            static char idStr[strLength];
            pico_get_unique_board_id_string(idStr, strLength);
            return std::string_view(idStr, strLength-1);
        }();

        midi::send_product_instance_id(product_instance_id, sendPacket);
    }

    if (m.requests_stream_configuration())
    {
        sendPacket(midi::make_stream_configuration_notification(curProtocol, curExtensions));
    }
}

void UMPProcessing::processFunctionBlockDiscovery(const midi::function_block_discovery_view &m)
{
    printf("function_block_discovery(%02X - %02X)\n", (int)m.function_block(), (int)m.filter());

    if (m.requests_info())
    {
        if (m.requests_function_block(0))
        {
            constexpr auto reply = midi::make_function_block_info_message(
                0, midi::function_block_options::bidirectional, 0);
            sendPacket(reply);
        }

      #if PROTOZOA_EXPANSION_CME_WIDI_CORE
        if (m.requests_function_block(DINPortsGroup))
        {
            constexpr auto reply = midi::make_function_block_info_message(
                DINPortsGroup,
                midi::function_block_options{
                    true,
                    midi::function_block_options::bidirectional,
                    midi::function_block_options::midi1_31250,
                    midi::function_block_options::ui_hint_as_direction,
                    0x00,
                    0
                },
                DINPortsGroup);
            sendPacket(reply);
        }

        if (m.requests_function_block(CMEWidiGroup))
        {
            constexpr auto reply = midi::make_function_block_info_message(
                CMEWidiGroup,
                midi::function_block_options{
                    true,
                    midi::function_block_options::direction_input,
                    midi::function_block_options::midi1_31250,
                    midi::function_block_options::ui_hint_as_direction,
                    0x00,
                    0
                },
                CMEWidiGroup);
            sendPacket(reply);
        }
      #else
        if (m.requests_function_block(DINPortOutGroup))
        {
            constexpr auto reply = midi::make_function_block_info_message(
                DINPortOutGroup,
                midi::function_block_options{
                    true,
                    midi::function_block_options::direction_input,
                    midi::function_block_options::midi1_31250,
                    midi::function_block_options::ui_hint_as_direction,
                    0x00,
                    0
                },
                DINPortOutGroup);
            sendPacket(reply);
        }

        if (m.requests_function_block(DINPortInGroup))
        {
            constexpr auto reply = midi::make_function_block_info_message(
                DINPortInGroup,
                midi::function_block_options{
                    true,
                    midi::function_block_options::direction_output,
                    midi::function_block_options::midi1_31250,
                    midi::function_block_options::ui_hint_as_direction,
                    0x00,
                    0
                },
                DINPortInGroup);
            sendPacket(reply);
        }
      #endif
    }

    if (m.requests_name())
    {
        if (m.requests_function_block(0))
        {
            constexpr auto reply = midi::make_function_block_name_message(
                midi::packet_format::complete, 0, "Main");
            sendPacket(reply);
        }

      #if PROTOZOA_EXPANSION_CME_WIDI_CORE
        if (m.requests_function_block(DINPortsGroup))
        {
            constexpr auto reply = midi::make_function_block_name_message(
                midi::packet_format::complete, DINPortsGroup, "5-pin DIN");
            sendPacket(reply);
        }

        if (m.requests_function_block(CMEWidiGroup))
        {
            constexpr auto reply = midi::make_function_block_name_message(
                midi::packet_format::complete, CMEWidiGroup, "CME Widi");
            sendPacket(reply);
        }

      #else
        if (m.requests_function_block(DINPortOutGroup))
        {
            constexpr auto reply = midi::make_function_block_name_message(
                midi::packet_format::complete, DINPortOutGroup, "EXT OUT");
            sendPacket(reply);
        }

        if (m.requests_function_block(DINPortInGroup))
        {
            constexpr auto reply = midi::make_function_block_name_message(
                midi::packet_format::complete, DINPortInGroup, "EXT IN");
            sendPacket(reply);
        }
      #endif
    }
}

void UMPProcessing::processStreamConfigurationRequest(const midi::stream_configuration_view &m)
{
    printf("stream_configuration_request(%02X - %02X)\n", (int)m.protocol(), (int)m.extensions());

    switch (m.protocol())
    {
    case midi::protocol::midi1:
    case midi::protocol::midi2:
        curProtocol = m.protocol();
        break;
    }

    sendPacket(midi::make_stream_configuration_notification(curProtocol, curExtensions));
}

void UMPProcessing::processSysexMessage(const midi::sysex7 &sx)
{
    if (midi::universal_sysex::is_identity_request(sx))
    {
        sendSysex(midi::universal_sysex::make_identity_reply(my_identity));
    }
    else if (midi::is_capability_inquiry_message(sx))
    {
        processMIDICIMessage(midi::capability_inquiry_view{ sx });
    }
    else
    {
        printf("sysex7: unhandled message type %02X subtype %02X\n",
                    (int)midi::universal_sysex_type_of(sx),
                    (int)midi::universal_sysex_subtype_of(sx));
    }
}

void UMPProcessing::processMIDICIMessage(const midi::capability_inquiry_view& msg)
{
    using namespace midi::ci;

    // ignore requests with invalid muids
    if ((msg.destination_muid() != m_muid) && (msg.destination_muid() != broadcast_muid))
    {
        printf("midi-ci: request with non-matching muid\n");
        return;
    }

    switch (msg.subtype())
    {
    case subtype::discovery_inquiry:
        printf("midi-ci: discovery_inquiry\n");
        if (auto di = msg.as<discovery_inquiry_view>())
        {
            printf("sendSysex(make_discovery_reply)\n");
            sendSysex(make_discovery_reply(
                m_muid, di->source_muid(), my_identity, category::property_exchange, maxSysexMessageSize, di->output_path_id(), 0));
        
            return;
        }
        break;
    case subtype::invalidate_muid:
        printf("midi-ci: invalidate_muid\n");
        if (auto im = msg.as<invalidate_muid_view>())
        {
            // if our muid is invalidated, generate a new one
            if (im->source_muid() == m_muid)
                m_muid = random(0xFFFFEFF);

            return;
        }
        break;
    case subtype::property_exchange_capabilities_inquiry:
        printf("midi-ci: property_exchange_capabilities_inquiry\n");
        if (auto peci = msg.as<property_exchange_capabilities_view>())
        {
            printf("sendSysex(make_property_exchange_capabilities_reply)\n");
            sendSysex(make_property_exchange_capabilities_reply(m_muid, peci->source_muid()));
        
            return;
        }
        break;
    case subtype::get_property_data_inquiry:
        printf("midi-ci: get_property_data_inquiry\n");
        if (auto gpdi = msg.as<get_property_data_view>())
        {
            processMIDICIGetProperty(*gpdi);

            return;
        }
        break;
    default:
        printf("midi-ci: unhandled request %02X\n", (int)msg.subtype());
        return;
    }

    printf("midi-ci: invalid / corrupted message\n");
}

void UMPProcessing::processMIDICIGetProperty(const midi::ci::get_property_data_view& msg)
{
    using namespace midi::ci;

    static const auto status_404 = property_exchange::header{ std::string_view{ "{\"status\":404}" } };

    PEHeaderParser p { reinterpret_cast<const char*>(msg.header_begin()), msg.header_size() };
    Resource r { Resource::None };
    
    if (p.get_resource(r) != 0)
    {
        printf("midi-ci: invalid resource requested\n");
        sendSysex(make_get_property_data_reply(m_muid, msg.source_muid(), status_404, 1, 1, { }));
        return;
    }

    auto sendGetPropertyReply = [&](const std::string_view &payload)
    {
        sendSysex(make_get_property_data_reply(m_muid, msg.source_muid(), 200, 1, 1, property_exchange::chunk{ payload }, msg.request_id(), msg.device_id()));
    };

    switch (r)
    {
    case Resource::ResourceList:
        printf("midi-ci: sendGetPropertyReply(ResourceList)\n");
        sendGetPropertyReply(my_ResourceList);
        break;
    case Resource::DeviceInfo:
        printf("midi-ci: sendGetPropertyReply(DeviceInfo)\n");
        sendGetPropertyReply(my_DeviceInfo);
        break;
    case Resource::ChannelList:
        printf("midi-ci: sendGetPropertyReply(ChannelList)\n");
        sendGetPropertyReply(my_ChannelList);
        break;
    case Resource::ChCtrlList:
    case Resource::ProgramList:
    default:
        printf("midi-ci: invalid resource requested\n");
        sendSysex(make_get_property_data_reply(m_muid, msg.source_muid(), status_404, 1, 1, { }));
        break;
    }
}

void UMPProcessing::sendSysex(const midi::sysex7& sx, midi::group_t group)
{
    midi::send_sysex7(sx, group, sendPacket);
}

#if NIMIDI2_CUSTOM_SYSEX_DATA_ALLOCATOR
// TODO: implement pool allocator
midi::sysex::data_allocator::value_type* midi::sysex::data_allocator::allocate(std::size_t n)
{
    return new value_type[n];
}

void midi::sysex::data_allocator::deallocate(value_type* p, std::size_t) noexcept
{
    return delete[](p);
}
#endif
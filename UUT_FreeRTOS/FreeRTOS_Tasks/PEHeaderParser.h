#pragma once

//----------------------------------------------------

#include <cstddef>
#include <cstdint>

//----------------------------------------------------
/// supported resources
enum class Resource : uint8_t {
  None = 0,
  ResourceList,
  DeviceInfo,
  ChannelList,
  ChCtrlList,
  ProgramList,
  __count__
};

//----------------------------------------------------
/// parsed option
struct Option {
  /// supported options
  enum class What : uint8_t {
    None,
    Limit,
    Offset,
    ID,
    Encoding,
    __count__
  } what { What::None };

  /// parsed option value
  struct Value {
    const char *string { nullptr };
    size_t length { 0 };
  } value;
};

//----------------------------------------------------
/// Property Exchange header parser suitable for very small devices 
/***
 * 
 * This class implements the complete resource and option header line
 * parsing including syntax and sanity checks in around 440 lines of
 * assembler listing (including all static data declarations, labels, etc).
 * 
 * The Restricted JSON header format as define in Contribution 16 allows small
 * devices to support Property Exchange without the need for a JSON parser and
 * without the need for any request data caching.
 * 
 * Even very basic embedded JSON parsers like jsmn (https://github.com/zserge/jsmn)
 * compile to around 950 instructions and do not include any syntax and sanity
 * checks at all.
 * 
 ***/
class PEHeaderParser {
public:
  PEHeaderParser(const char *data, size_t len) :
    cur(data), length(len)
  {}

  int get_resource(Resource&);
  int get_status(unsigned&);
  int get_next_option(Option&);

private:
  const char *cur { nullptr };
  size_t length { 0 };

  struct ResourceEntry {
    Resource resource;
    const char *string;
    size_t length;
  };
  static const ResourceEntry resources[static_cast<unsigned>(Resource::__count__)-1];

  struct OptionEntry {
    Option::What what;
    const char *string;
    size_t length;
  };
  static const OptionEntry options[static_cast<unsigned>(Option::What::__count__)-1];

  void advance(size_t);
};

//----------------------------------------------------

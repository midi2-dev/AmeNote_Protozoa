#include "PEHeaderParser.h"

#include <cstring>

//----------------------------------------------------

const PEHeaderParser::ResourceEntry PEHeaderParser::resources[] = {
  { Resource::ResourceList, "ResourceList", 12 },
  { Resource::DeviceInfo,   "DeviceInfo",   10 },
  { Resource::ChannelList,  "ChannelList",  11 },
  { Resource::ChCtrlList,   "ChCtrlList",   10 },
  { Resource::ProgramList,  "ProgramList",  11 },
};

const PEHeaderParser::OptionEntry PEHeaderParser::options[] = {
  { Option::What::Limit,    "limit",    5 },
  { Option::What::Offset,   "offset",   6 },
  { Option::What::ID,       "id",       2 },
  { Option::What::Encoding, "encoding", 8 },
};

//----------------------------------------------------
/// extract resource (first header line)
int PEHeaderParser::get_resource(Resource &res)
{
  if ((length > 13) && (strncmp("{\"resource\":\"", cur, 13) == 0))
  {
    advance(13); // skip JSON tag

    for (const auto &c : resources)
    {
      const auto byte_cnt = c.length+1;

      if ((length > byte_cnt) &&
          (cur[c.length]=='"') &&
          (strncmp(c.string, cur, c.length) == 0))
      {
        advance(byte_cnt);
        res = c.resource;
        return 0;
      }
    }
  }

  return -1; // invalid resource or malformed header
}

//----------------------------------------------------
/// extract status (first header line)
int PEHeaderParser::get_status(unsigned &status)
{
  if ((length > 10) && (strncmp("{\"status\":", cur, 10) == 0))
  {
    advance(10); // skip JSON tag

    status = 0;
    for ( ; length; ++cur,--length)
    {
      if ((*cur=='}') || (*cur==','))
        return status ? 0 : -1;

      if ((*cur<'0') || (*cur>'9'))
        return -1; // invalid status
      
      status = status * 10 + (*cur-'0');
    }
  }

  return -1; // invalid status
}

//----------------------------------------------------
/// get next header option (second and following header lines)
int PEHeaderParser::get_next_option(Option &option)
{
  if (!length)
    return -1; // invalid option or malformed header
  else if (*cur=='}')
  {
    advance(1);
    return 1; // end of header, no more options
  }

  if ((length>5) && (cur[0]==',') && (cur[1]=='"'))
  {
    advance(2); // skip comma and quote

    for (const auto &p : options)
    {
      const auto byte_cnt = p.length+1;

      if ((length > byte_cnt) &&
          (cur[p.length]=='"') && 
          (strncmp(p.string, cur, p.length) == 0))
      {
        advance(byte_cnt);

        option.what = p.what;

        if (*cur!=':')
          break; // error, exit early
        cur++;
        --length;

        if (*cur=='"')
          advance(1); // skip quote

        // extract value
        option.value.string = cur;
        option.value.length = 0;

        for ( ; length; ++cur,--length,++option.value.length)
        {
          if ((*cur=='}') || (*cur==',')) 
            return 0;
          else if (*cur=='"')
          {
            advance(1); // skip quote
            return 0;
          }
        }

        break; // error, exit early
      }
    }
  }

  return -1; // invalid option or malformed header
}

//----------------------------------------------------

void PEHeaderParser::advance(size_t cnt)
{
  cur += cnt;
  length =  (length>cnt) ? (length-cnt) : 0;
}

//----------------------------------------------------

#pragma once

#include <midi/universal_packet.h>

#include <cassert>

#if PROTOZOA_SERIAL_BRACKET16
//! 16 Bit Bracketing
struct SerialBracketing
{
  static constexpr uint8_t STX = 0xF0;

  static uint8_t encode(uint32_t *words, uint8_t num_words, uint8_t *buffer)
  {
    assert(num_words <= 4);
    midi::universal_packet ump{words[0]};
    for (uint8_t w = 1; w < num_words; ++w)
      ump.data[w] = words[w];

    return encode(ump, buffer);
  }

  static uint8_t encode(const midi::universal_packet &ump, uint8_t *buffer)
  {
    const uint8_t num_words = ump.size() & 0xF;
    uint8_t num_bytes = 0;
    uint8_t csum = 0xf7;
    uint8_t b;
    buffer[num_bytes++] = STX | num_words;
    for (uint8_t word = 0; word < num_words; ++word)
    {
      for (uint8_t i = 4; i != 0; --i)
      {
        b = ump.data[word] >> (8 * (i - 1));
        buffer[num_bytes++] = b;
        csum -= b;
      }
    }
    buffer[num_bytes++] = csum;
    return num_bytes;
  }

  midi::universal_packet ump;

  enum status
  {
    SUCCESS = 0,
    SYNCING = 1,
    COLLECTING = 2,
    INVALID_UMP = 0xFE,
    CRC_ERROR = 0xFF
  };

  status feed(uint8_t inByte)
  {
    if (bytes_left == 0) // Looking for sync here
    {
      if ((inByte & 0xF0) == STX) // Hi nibble is STX think about it
      {
        ump_size = inByte & 0x0f;
        bytes_left = (ump_size * 4) + 1; // add one because when bytes_left = 1 we will do checksum check
        usPos = 3;
        chksum = 0x00;
        inWord = 0;
      }
      else
        return SYNCING;
    }
    else if (bytes_left == 1) // Last byte in packet
    {
      bytes_left = 0;

      if (((inByte + chksum) & 0xFF) == 0xF7) // Does checksum match?
      {
        if (ump.size() != ump_size)
          return INVALID_UMP;

        return SUCCESS;
      }
      return CRC_ERROR;
    }
    else // pack the bytes into 32 bit words
    {
      switch (usPos)
      {
      case 3:
        ump.data[inWord] = inByte << 24;
        usPos = 2;
        break;
      case 2:
      case 1:
        ump.data[inWord] += inByte << (8 * usPos);
        usPos--;
        break;
      case 0:
        ump.data[inWord] += inByte;
        usPos = 3;
        inWord++;
        break;
      }
      bytes_left--;
      chksum += inByte;
    }
    return COLLECTING;
  }

  uint8_t checksum() const { return chksum; }
  
private:
  uint8_t usPos = 3;
  uint8_t bytes_left = 0;
  uint8_t chksum = 0xf7;
  uint8_t ump_size = 0;
  uint8_t inWord = 0;
};

#else

#include "include/cobs.h"

//! COBS bracketing
struct SerialBracketing
{
  static uint8_t encode(uint32_t *words, uint8_t num_words, uint8_t *buffer)
  {
    uint8_t sBytes[num_words*4];
    uint8_t sbPos=0;
    for(uint8_t j = 0;j<num_words;j++){
        for(uint8_t i = 4;i;i--){
            sBytes[sbPos++] = words[j] >> (8*(i-1));
        }
    }

    uint8_t length = cobsUMP::encode(sBytes, num_words*4, buffer);
    assert(length < num_words*4+2);
    buffer[length++] = 0;
    return length;
  }

  static uint8_t encode(const midi::universal_packet &ump, uint8_t *buffer)
  {
    return encode((uint32_t*)&ump.data, uint8_t(ump.size()), buffer);
  }

  midi::universal_packet ump;

  enum status
  {
    SUCCESS = 0,
    SYNCING = 1,
    COLLECTING = 2,
    INVALID_UMP = 0xFE,
    CRC_ERROR = 0xFF
  };

  status feed(uint8_t inByte)
  {
    cobs.processSerial(inByte);
    if (cobs.availableUMP())
    {
      if (num_missing_words == 0)
      {
        ump.data[0] = cobs.readUMP();
        num_missing_words = ump.size()-1;
      }
      else
      {
        ump.data[ump.size() - num_missing_words] = cobs.readUMP();
        --num_missing_words;
      }

      return (num_missing_words ? COLLECTING : SUCCESS);
    }

    return COLLECTING;
  }

private:
  cobsUMP cobs;
  size_t num_missing_words { 0 };
};

#endif
/*******************************************************************************
 * Project:  Nebula
 * @file     Http2Header.cpp
 * @brief    
 * @author   nebim
 * @date:    2020-05-03
 * @note     
 * Modify history:
 ******************************************************************************/
#include "Http2Header.hpp"
#include "Huffman.hpp"

namespace neb
{

Http2Header::Http2Header(const std::string& strName, const std::string& strValue)
    : m_strName(strName), m_strValue(strValue), m_uiHpackSize(0)
{
    m_uiHpackSize = m_strName.size() + m_strValue.size() + 32;
}

Http2Header::Http2Header(Http2Header&& rHeader)
    : m_strName(std::move(rHeader.m_strName)),
      m_strValue(std::move(rHeader.m_strValue)),
      m_uiHpackSize(rHeader.m_uiHpackSize)
{
}

Http2Header::~Http2Header()
{
}

Http2Header& Http2Header::operator=(Http2Header&& rHeader)
{
    m_strName = std::move(rHeader.m_strName);
    m_strValue = std::move(rHeader.m_strValue);
    m_uiHpackSize = rHeader.m_uiHpackSize;
    return(*this);
}

void Http2Header::EncodeInt(size_t uiValue, size_t uiPrefix, char cBits, CBuffer* pBuff)
{
    if (uiValue < uiPrefix)
    {
        pBuff->WriteByte(cBits | uiValue);
    }
    else
    {
        pBuff->WriteByte(cBits | uiPrefix);
        int I = uiValue - uiPrefix;
        int B = 0;
        while (I >= 128)
        {
            B = (I & 0x7f) | 0x80;          // I % 128 + 128
            I >>= 7;                        // I / 128
            pBuff->WriteByte(B);
        }
        pBuff->WriteByte(I);
    }
}

int Http2Header::DecodeInt(int iPrefixMask, CBuffer* pBuff)
{
    char B;
    pBuff->ReadByte(B);
    int I = B & iPrefixMask;
    if (I < iPrefixMask)
    {
        return(I);     // This was a single byte value.
    }
    else
    {
        // This is a multibyte value. Read 7 bits at a time.
        I = iPrefixMask;
        int M = 0;
        do
        {
            pBuff->ReadByte(B);         // next octet
            I += (B & 0x7f) << M;       // I = I + (B & 127) * 2^M
            M += 7;                     // M = M + 7
        }
        while ((B & 128) == 128);

        return(I);
    }
}

void Http2Header::EncodeStringLiteral(const std::string& strLiteral, CBuffer* pBuff)
{
    Http2Header::EncodeInt(strLiteral.size(), (size_t)H2_HPACK_PREFIX_7_BITS,
            (char)0, pBuff);
    pBuff->Write(strLiteral.c_str(), strLiteral.size());
}

void Http2Header::EncodeStringLiteralWithHuffman(const std::string& strLiteral, CBuffer* pBuff)
{
    Huffman::Instance()->Encode(strLiteral, pBuff);
}

bool Http2Header::DecodeStringLiteral(CBuffer* pBuff, std::string& strLiteral, bool& bWithHuffman)
{
    char B;
    size_t uiReadIndex = pBuff->GetReadIndex();
    pBuff->ReadByte(B);
    pBuff->SetReadIndex(uiReadIndex);
    int iStringLength = DecodeInt(H2_HPACK_PREFIX_7_BITS, pBuff);
    if (pBuff->ReadableBytes() < (size_t)iStringLength)
    {
        return(false);
    }

    if (B & 0x80)   // HUFFMAN
    {
        bWithHuffman = true;
        return(Huffman::Instance()->Decode(pBuff, iStringLength, strLiteral));
    }
    else
    {
        strLiteral.assign(pBuff->GetRawReadBuffer(), iStringLength);
        pBuff->AdvanceReadIndex(iStringLength);
        return(true);
    }
}

size_t Http2Header::GetStaticTableIndex(const std::string& strHeaderName, const std::string& strHeaderValue)
{
    if (strHeaderValue.size() == 0)
    {
        auto c_iter = sc_mapStaticTable.find(strHeaderName);
        if (c_iter == sc_mapStaticTable.end())
        {
            return(0);
        }
        return(c_iter->second);
    }
    else
    {
        auto c_iter = sc_mapStaticTable.find(strHeaderName + " " + strHeaderValue);
        if (c_iter == sc_mapStaticTable.end())
        {
            return(0);
        }
        return(c_iter->second);
    }
}

} /* namespace neb */


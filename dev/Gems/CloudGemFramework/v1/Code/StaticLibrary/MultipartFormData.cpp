/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include "CloudGemFramework_precompiled.h"

#include <AzCore/IO/FileIO.h>
#include <CloudGemFramework/MultipartFormData.h>

namespace CloudGemFramework
{
    namespace Detail
    {
        namespace
        {
            const char FIELD_HEADER_FMT[] = "--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n";
            const char FILE_HEADER_FMT[] = "--%s\r\nContent-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n\r\n";
            const char FOOTER_FMT[] = "--%s--\r\n";
            const char ENTRY_SEPARATOR[] = "\r\n";
        }
    }

    void MultipartFormData::AddField(AZStd::string name, AZStd::string value)
    {
        m_fields.emplace_back(Field{ std::move(name), std::move(value) });
    }

    void MultipartFormData::AddFile(AZStd::string fieldName, AZStd::string fileName, const char* path)
    {
        AZ::IO::FileIOStream fp(path, AZ::IO::OpenMode::ModeRead | AZ::IO::OpenMode::ModeBinary);

        if (fp.IsOpen())
        {
            m_fileFields.emplace_back(FileField{ std::move(fieldName), std::move(fileName) , AZStd::vector<char>{}});
            auto destFileBuffer = &m_fileFields.back().m_fileData;
            destFileBuffer->resize(fp.GetLength());
            fp.Read(destFileBuffer->size(), &destFileBuffer->at(0));
        }
    }

    void MultipartFormData::AddFileBytes(AZStd::string fieldName, AZStd::string fileName, const void* bytes, size_t length)
    {
        m_fileFields.emplace_back(FileField{ std::move(fieldName), std::move(fileName) , AZStd::vector<char>{}});
        m_fileFields.back().m_fileData.reserve(length);
        m_fileFields.back().m_fileData.assign((const char*)bytes, (const char*)bytes + length);
    }

    void MultipartFormData::SetCustomBoundary(AZStd::string boundary)
    {
        m_boundary = boundary;
    }

    void MultipartFormData::Prepare()
    {
        if (m_boundary.empty())
        {
            char buffer[33];
            AZ::Uuid::CreateRandom().ToString(buffer, sizeof(buffer), false, false);
            m_boundary = buffer;
        }
    }

    size_t MultipartFormData::EstimateBodySize() const
    {
        // Estimate the size of the final string as best we can to avoid unnecessary copies
        const size_t boundarySize = m_boundary.length();
        const size_t individualFieldBaseSize = boundarySize + sizeof(Detail::FIELD_HEADER_FMT) + sizeof(Detail::ENTRY_SEPARATOR);
        const size_t individualFileBaseSize = boundarySize + sizeof(Detail::FILE_HEADER_FMT) + sizeof(Detail::ENTRY_SEPARATOR);
        size_t estimatedSize = sizeof(Detail::FOOTER_FMT) + boundarySize;

        for (const auto& field : m_fields)
        {
            estimatedSize += individualFieldBaseSize + field.m_fieldName.length() + field.m_value.length();
        }

        for (const auto& fileField : m_fileFields)
        {
            estimatedSize += individualFileBaseSize + fileField.m_fieldName.length() + fileField.m_fileName.length() + fileField.m_fileData.size();
        }

        return estimatedSize;
    }

    MultipartFormData::ComposeResult MultipartFormData::ComposeForm()
    {
        ComposeResult result;
        Prepare();

        // Build the form body
        result.m_content.reserve(EstimateBodySize());

        for (const auto& field : m_fields)
        {
            result.m_content.append(AZStd::string::format(Detail::FIELD_HEADER_FMT, m_boundary.c_str(), field.m_fieldName.c_str()));
            result.m_content.append(field.m_value);
            result.m_content.append(Detail::ENTRY_SEPARATOR);
        }

        for (const auto& fileField : m_fileFields)
        {
            result.m_content.append(AZStd::string::format(Detail::FILE_HEADER_FMT, m_boundary.c_str(), fileField.m_fieldName.c_str(), fileField.m_fileName.c_str()));
            result.m_content.append(fileField.m_fileData.begin(), fileField.m_fileData.end());
            result.m_content.append(Detail::ENTRY_SEPARATOR);
        }

        result.m_content.append(AZStd::string::format(Detail::FOOTER_FMT, m_boundary.c_str()));

        // Populate the metadata
        result.m_contentLength = AZStd::string::format("%lu", result.m_content.length());
        result.m_contentType = AZStd::string::format("multipart/form-data; boundary=%s", m_boundary.c_str());

        return result;
    }


} // namespace CloudGemFramework

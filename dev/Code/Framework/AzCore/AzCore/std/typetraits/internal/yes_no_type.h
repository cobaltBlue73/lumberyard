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
#ifndef AZSTD_TYPE_TRAITS_INTERNAL_YES_NO_TYPE_INCLUDED
#define AZSTD_TYPE_TRAITS_INTERNAL_YES_NO_TYPE_INCLUDED

namespace AZStd
{
    namespace type_traits
    {
        typedef char yes_type;
        struct no_type
        {
            char padding[8];
        };
    }
}

#endif // AZSTD_TYPE_TRAITS_INTERNAL_YES_NO_TYPE_INCLUDED
#pragma once

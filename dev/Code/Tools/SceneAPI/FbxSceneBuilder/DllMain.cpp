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

#if !defined(AZ_MONOLITHIC_BUILD)

#include <AzCore/Component/Component.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/Module/Environment.h>
#include <SceneAPI/FbxSceneBuilder/FbxImportRequestHandler.h>

#include <SceneAPI/FbxSceneBuilder/FbxImporter.h>
#include <SceneAPI/FbxSceneBuilder/Importers/FbxAnimationImporter.h>
#include <SceneAPI/FbxSceneBuilder/Importers/FbxBlendShapeImporter.h>
#include <SceneAPI/FbxSceneBuilder/Importers/FbxBoneImporter.h>
#include <SceneAPI/FbxSceneBuilder/Importers/FbxColorStreamImporter.h>
#include <SceneAPI/FbxSceneBuilder/Importers/FbxMaterialImporter.h>
#include <SceneAPI/FbxSceneBuilder/Importers/FbxMeshImporter.h>
#include <SceneAPI/FbxSceneBuilder/Importers/FbxSkinImporter.h>
#include <SceneAPI/FbxSceneBuilder/Importers/FbxSkinWeightsImporter.h>
#include <SceneAPI/FbxSceneBuilder/Importers/FbxTransformImporter.h>
#include <SceneAPI/FbxSceneBuilder/Importers/FbxUvMapImporter.h>

static AZ::SceneAPI::FbxSceneImporter::FbxImportRequestHandler* g_fbxImporter = nullptr;
static AZStd::vector<AZ::ComponentDescriptor*> g_componentDescriptors;

extern "C" AZ_DLL_EXPORT void InitializeDynamicModule(void* env)
{
    using namespace AZ::SceneAPI;
    
    AZ::Environment::Attach(static_cast<AZ::EnvironmentInstance>(env));

    // Currently it's still needed to explicitly create an instance of this instead of letting
    //      it be a normal component. This is because ResourceCompilerScene needs to return
    //      the list of available extensions before it can start the application.
    if (!g_fbxImporter)
    {
        g_fbxImporter = aznew FbxSceneImporter::FbxImportRequestHandler();
        g_fbxImporter->Activate();
    }
}

extern "C" AZ_DLL_EXPORT void Reflect(AZ::SerializeContext* /*context*/)
{
    // Descriptor registration is done in Reflect instead of Initialize because the ResourceCompilerScene initializes the libraries before
    // there's an application.
    using namespace AZ::SceneAPI;
    using namespace AZ::SceneAPI::FbxSceneBuilder;

    if (g_componentDescriptors.empty())
    {
        // Global importer and behavior
        g_componentDescriptors.push_back(FbxSceneBuilder::FbxImporter::CreateDescriptor());

        // Node and attribute importers
        g_componentDescriptors.push_back(FbxAnimationImporter::CreateDescriptor());
        g_componentDescriptors.push_back(FbxBlendShapeImporter::CreateDescriptor());
        g_componentDescriptors.push_back(FbxBoneImporter::CreateDescriptor());
        g_componentDescriptors.push_back(FbxColorStreamImporter::CreateDescriptor());
        g_componentDescriptors.push_back(FbxMaterialImporter::CreateDescriptor());
        g_componentDescriptors.push_back(FbxMeshImporter::CreateDescriptor());
        g_componentDescriptors.push_back(FbxSkinImporter::CreateDescriptor());
        g_componentDescriptors.push_back(FbxSkinWeightsImporter::CreateDescriptor());
        g_componentDescriptors.push_back(FbxTransformImporter::CreateDescriptor());
        g_componentDescriptors.push_back(FbxUvMapImporter::CreateDescriptor());
        
        for (AZ::ComponentDescriptor* descriptor : g_componentDescriptors)
        {
            AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationBus::Handler::RegisterComponentDescriptor, descriptor);
        }
    }
}

extern "C" AZ_DLL_EXPORT void UninitializeDynamicModule()
{
    if (!g_componentDescriptors.empty())
    {
        for (AZ::ComponentDescriptor* descriptor : g_componentDescriptors)
        {
            descriptor->ReleaseDescriptor();
        }
        g_componentDescriptors.clear();
        g_componentDescriptors.shrink_to_fit();
    }

    if (g_fbxImporter)
    {
        g_fbxImporter->Deactivate();
        delete g_fbxImporter;
        g_fbxImporter = nullptr;
    }
    
    AZ::Environment::Detach();
}

#endif // !defined(AZ_MONOLITHIC_BUILD)

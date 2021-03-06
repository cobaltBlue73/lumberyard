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
#pragma once

#include <AzCore/std/containers/set.h>
#include <AzCore/std/containers/unordered_map.h>

#include <GraphCanvas/Utils/StateControllers/StateController.h>

namespace GraphCanvas
{
    template<class T, class Compare = AZStd::less<T> >
    class PrioritizedStateController
        : public StateController<T>
    {
    public:
        PrioritizedStateController(const T& defaultValue)
            : StateController<T>(defaultValue)
        {        
        }
        
        ~PrioritizedStateController()
        {
            // Not a big fan of this. Would need to link them using a weak ptr of some sort.
            while (!m_valueMapping.empty())
            {
                auto mapIter = m_valueMapping.rbegin();
                StateSetter<T>* stateSetter = mapIter->first;
                
                bool releasedState = stateSetter->RemoveStateController(this);

                AZ_Error("PrioritizedStateController", releasedState, "Failed to properly release StateSetter state from owning StateController.");
                if (!releasedState)
                {
                    m_valueMapping.erase(mapIter.base());   
                }
            }
            
            m_valueMapping.clear();
            m_valueSet.clear();
        }

        bool HasState() const
        {
            return !m_valueSet.empty();
        }

    protected:
        
        bool OnPushState(StateSetter<T>* stateSetter, const T& state) override
        {
            auto insertResult = m_valueMapping.insert(AZStd::pair<StateSetter<T>*, T>(stateSetter, state));
            
            AZ_Error("PrioritizedStateController",insertResult.second,"Trying to set two values from a single state setter.");            
            
            if (insertResult.second)
            {
                m_valueSet.insert(state);
            }
            
            return insertResult.second;
        }
        
        bool OnReleaseState(StateSetter<T>* stateSetter) override
        {
            bool releasedValue = false;
            auto valueIter = m_valueMapping.find(stateSetter);
            
            if (valueIter != m_valueMapping.end())
            {
                m_valueSet.erase(valueIter->second);
                m_valueMapping.erase(valueIter);
                
                releasedValue = true;
            }
            
            return releasedValue;
        }

        const T& GetCalculatedState() const
        {
            auto valueIter = m_valueSet.begin();
            return (*valueIter);
        }
    
    private:
    
        AZStd::multiset<T, Compare> m_valueSet;
        AZStd::unordered_map<StateSetter<T>*, T> m_valueMapping;
    };
}